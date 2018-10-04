/*
 * Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "hal_adapter/camera_device_adapter.h"

#include <unistd.h>

#include <map>
#include <set>
#include <utility>
#include <vector>

#include <base/bind.h>
#include <base/bind_helpers.h>
#include <base/logging.h>
#include <drm_fourcc.h>
#include <libyuv.h>
#include <sync/sync.h>
#include <system/camera_metadata.h>

#include "common/camera_buffer_handle.h"
#include "cros-camera/common.h"
#include "cros-camera/ipc_util.h"
#include "hal_adapter/camera3_callback_ops_delegate.h"
#include "hal_adapter/camera3_device_ops_delegate.h"

namespace cros {

Camera3CaptureRequest::Camera3CaptureRequest(
    const camera3_capture_request_t& req)
    : settings_(android::CameraMetadata(clone_camera_metadata(req.settings))),
      input_buffer_(*req.input_buffer),
      output_stream_buffers_(req.output_buffers,
                             req.output_buffers + req.num_output_buffers) {
  Camera3CaptureRequest::frame_number = req.frame_number;
  Camera3CaptureRequest::settings = settings_.getAndLock();
  Camera3CaptureRequest::input_buffer = &input_buffer_;
  Camera3CaptureRequest::num_output_buffers = req.num_output_buffers;
  Camera3CaptureRequest::output_buffers = output_stream_buffers_.data();
}

CameraDeviceAdapter::CameraDeviceAdapter(camera3_device_t* camera_device,
                                         base::Callback<void()> close_callback)
    : camera_device_ops_thread_("CameraDeviceOpsThread"),
      camera_callback_ops_thread_("CameraCallbackOpsThread"),
      fence_sync_thread_("FenceSyncThread"),
      reprocess_effect_thread_("ReprocessEffectThread"),
      close_callback_(close_callback),
      device_closed_(false),
      camera_device_(camera_device) {
  VLOGF_ENTER() << ":" << camera_device_;
  camera3_callback_ops_t::process_capture_result = ProcessCaptureResult;
  camera3_callback_ops_t::notify = Notify;
}

CameraDeviceAdapter::~CameraDeviceAdapter() {
  VLOGF_ENTER() << ":" << camera_device_;
  camera_device_ops_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&CameraDeviceAdapter::ResetDeviceOpsDelegateOnThread,
                 base::Unretained(this)));
  camera_callback_ops_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&CameraDeviceAdapter::ResetCallbackOpsDelegateOnThread,
                 base::Unretained(this)));
  camera_device_ops_thread_.Stop();
  camera_callback_ops_thread_.Stop();
}

bool CameraDeviceAdapter::Start(
    HasReprocessEffectVendorTagCallback
        has_reprocess_effect_vendor_tag_callback,
    ReprocessEffectCallback reprocess_effect_callback) {
  if (!camera_device_ops_thread_.Start()) {
    LOGF(ERROR) << "Failed to start CameraDeviceOpsThread";
    return false;
  }
  if (!camera_callback_ops_thread_.Start()) {
    LOGF(ERROR) << "Failed to start CameraCallbackOpsThread";
    return false;
  }
  device_ops_delegate_.reset(new Camera3DeviceOpsDelegate(
      this, camera_device_ops_thread_.task_runner()));
  has_reprocess_effect_vendor_tag_callback_ =
      std::move(has_reprocess_effect_vendor_tag_callback);
  reprocess_effect_callback_ = std::move(reprocess_effect_callback);
  return true;
}

void CameraDeviceAdapter::Bind(
    mojom::Camera3DeviceOpsRequest device_ops_request) {
  device_ops_delegate_->Bind(
      device_ops_request.PassMessagePipe(),
      // Close the device when the Mojo channel breaks.
      base::Bind(base::IgnoreResult(&CameraDeviceAdapter::Close),
                 base::Unretained(this)));
}

int32_t CameraDeviceAdapter::Initialize(
    mojom::Camera3CallbackOpsPtr callback_ops) {
  VLOGF_ENTER();
  if (!fence_sync_thread_.Start()) {
    LOGF(ERROR) << "Fence sync thread failed to start";
    return -ENODEV;
  }
  if (!reprocess_effect_thread_.Start()) {
    LOGF(ERROR) << "Reprocessing effect thread failed to start";
    return -ENODEV;
  }
  base::AutoLock l(callback_ops_delegate_lock_);
  // Unlike the camera module, only one peer is allowed to access a camera
  // device at any time.
  DCHECK(!callback_ops_delegate_);
  callback_ops_delegate_.reset(new Camera3CallbackOpsDelegate(
      this, camera_callback_ops_thread_.task_runner()));
  callback_ops_delegate_->Bind(
      callback_ops.PassInterface(),
      base::Bind(&CameraDeviceAdapter::ResetCallbackOpsDelegateOnThread,
                 base::Unretained(this)));
  return camera_device_->ops->initialize(camera_device_, this);
}

int32_t CameraDeviceAdapter::ConfigureStreams(
    mojom::Camera3StreamConfigurationPtr config,
    mojom::Camera3StreamConfigurationPtr* updated_config) {
  VLOGF_ENTER();

  base::AutoLock l(streams_lock_);

  internal::ScopedStreams new_streams;
  for (const auto& s : config->streams) {
    uint64_t id = s->id;
    std::unique_ptr<camera3_stream_t>& stream = new_streams[id];
    stream.reset(new camera3_stream_t);
    memset(stream.get(), 0, sizeof(*stream.get()));
    stream->stream_type = static_cast<camera3_stream_type_t>(s->stream_type);
    stream->width = s->width;
    stream->height = s->height;
    stream->format = static_cast<int32_t>(s->format);
    stream->usage = s->usage;
    stream->max_buffers = s->max_buffers;
    stream->data_space = static_cast<android_dataspace_t>(s->data_space);
    stream->rotation = static_cast<camera3_stream_rotation_t>(s->rotation);
    stream->crop_rotate_scale_degrees = 0;
    if (s->crop_rotate_scale_info != nullptr) {
      stream->crop_rotate_scale_degrees =
          static_cast<camera3_stream_rotation_t>(
              s->crop_rotate_scale_info->crop_rotate_scale_degrees);
    }
  }
  streams_.swap(new_streams);

  camera3_stream_configuration_t stream_list;
  stream_list.num_streams = config->streams.size();
  std::vector<camera3_stream_t*> streams(stream_list.num_streams);
  stream_list.streams = streams.data();
  stream_list.operation_mode =
      static_cast<camera3_stream_configuration_mode_t>(config->operation_mode);
  size_t i = 0;
  for (auto it = streams_.begin(); it != streams_.end(); it++) {
    stream_list.streams[i++] = it->second.get();
  }

  int32_t result =
      camera_device_->ops->configure_streams(camera_device_, &stream_list);
  if (!result) {
    *updated_config = mojom::Camera3StreamConfiguration::New();
    for (const auto& s : streams_) {
      mojom::Camera3StreamPtr ptr = mojom::Camera3Stream::New();
      ptr->id = s.first;
      ptr->format = static_cast<mojom::HalPixelFormat>(s.second->format);
      ptr->width = s.second->width;
      ptr->height = s.second->height;
      ptr->stream_type =
          static_cast<mojom::Camera3StreamType>(s.second->stream_type);
      ptr->data_space = s.second->data_space;
      // HAL should only change usage and max_buffers.
      ptr->usage = s.second->usage;
      ptr->max_buffers = s.second->max_buffers;
      (*updated_config)->streams.push_back(std::move(ptr));
    }
  }

  return result;
}

mojom::CameraMetadataPtr CameraDeviceAdapter::ConstructDefaultRequestSettings(
    mojom::Camera3RequestTemplate type) {
  VLOGF_ENTER();
  const camera_metadata_t* metadata =
      camera_device_->ops->construct_default_request_settings(
          camera_device_, static_cast<int32_t>(type));
  return internal::SerializeCameraMetadata(metadata);
}

int32_t CameraDeviceAdapter::ProcessCaptureRequest(
    mojom::Camera3CaptureRequestPtr request) {
  VLOGF_ENTER();
  camera3_capture_request_t req;

  req.frame_number = request->frame_number;

  internal::ScopedCameraMetadata settings =
      internal::DeserializeCameraMetadata(request->settings);
  req.settings = settings.get();

  // Deserialize input buffer.
  buffer_handle_t input_buffer_handle;
  camera3_stream_buffer_t input_buffer;
  if (!request->input_buffer.is_null()) {
    base::AutoLock streams_lock(streams_lock_);
    base::AutoLock buffer_handles_lock(buffer_handles_lock_);
    input_buffer.buffer =
        const_cast<const native_handle_t**>(&input_buffer_handle);
    internal::DeserializeStreamBuffer(request->input_buffer, streams_,
                                      buffer_handles_, &input_buffer);
    req.input_buffer = &input_buffer;
  } else {
    req.input_buffer = nullptr;
  }

  // Deserialize output buffers.
  size_t num_output_buffers = request->output_buffers.size();
  DCHECK_GT(num_output_buffers, 0);
  req.num_output_buffers = num_output_buffers;

  DCHECK(!request->output_buffers.is_null());
  std::vector<camera3_stream_buffer_t> output_buffers(num_output_buffers);
  {
    base::AutoLock streams_lock(streams_lock_);
    base::AutoLock buffer_handles_lock(buffer_handles_lock_);
    for (size_t i = 0; i < num_output_buffers; ++i) {
      mojom::Camera3StreamBufferPtr& out_buf_ptr = request->output_buffers[i];
      internal::DeserializeStreamBuffer(out_buf_ptr, streams_, buffer_handles_,
                                        &output_buffers.at(i));
    }
    req.output_buffers =
        const_cast<const camera3_stream_buffer_t*>(output_buffers.data());
  }

  // Apply reprocessing effects
  if (req.input_buffer &&
      has_reprocess_effect_vendor_tag_callback_.Run(*req.settings)) {
    VLOGF(1) << "Applying reprocessing effects on input buffer";
    // Run reprocessing effect asynchronously so that it does not block other
    // requests.  It introduces a risk that buffers of the same stream may be
    // returned out of order.  Since CTS would not go this way and GCA would
    // not mix reprocessing effect captures with normal ones, it should be
    // fine.
    auto req_ptr = std::make_unique<Camera3CaptureRequest>(req);
    reprocess_effect_thread_.task_runner()->PostTask(
        FROM_HERE,
        base::Bind(
            &CameraDeviceAdapter::ReprocessEffectsOnReprocessEffectThread,
            base::Unretained(this), base::Passed(&req_ptr)));
    return 0;
  }

  return camera_device_->ops->process_capture_request(camera_device_, &req);
}

void CameraDeviceAdapter::Dump(mojo::ScopedHandle fd) {
  VLOGF_ENTER();
  base::ScopedFD dump_fd(UnwrapPlatformHandle(std::move(fd)));
  camera_device_->ops->dump(camera_device_, dump_fd.get());
}

int32_t CameraDeviceAdapter::Flush() {
  VLOGF_ENTER();
  return camera_device_->ops->flush(camera_device_);
}

static bool IsMatchingFormat(mojom::HalPixelFormat hal_pixel_format,
                             uint32_t drm_format) {
  switch (hal_pixel_format) {
    case mojom::HalPixelFormat::HAL_PIXEL_FORMAT_RGBA_8888:
      return drm_format == DRM_FORMAT_ABGR8888;
    case mojom::HalPixelFormat::HAL_PIXEL_FORMAT_RGBX_8888:
      return drm_format == DRM_FORMAT_XBGR8888;
    case mojom::HalPixelFormat::HAL_PIXEL_FORMAT_BGRA_8888:
      return drm_format == DRM_FORMAT_ARGB8888;
    case mojom::HalPixelFormat::HAL_PIXEL_FORMAT_YCrCb_420_SP:
      return drm_format == DRM_FORMAT_NV21;
    case mojom::HalPixelFormat::HAL_PIXEL_FORMAT_YCbCr_422_I:
      return drm_format == DRM_FORMAT_YUYV;
    case mojom::HalPixelFormat::HAL_PIXEL_FORMAT_BLOB:
      return drm_format == DRM_FORMAT_R8;
    case mojom::HalPixelFormat::HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED:
      // We can't really check implementation defined formats.
      return true;
    case mojom::HalPixelFormat::HAL_PIXEL_FORMAT_YCbCr_420_888:
      return (drm_format == DRM_FORMAT_YUV420 ||
              drm_format == DRM_FORMAT_YVU420 ||
              drm_format == DRM_FORMAT_NV21 || drm_format == DRM_FORMAT_NV12);
    case mojom::HalPixelFormat::HAL_PIXEL_FORMAT_YV12:
      return drm_format == DRM_FORMAT_YVU420;
  }
  return false;
}

int32_t CameraDeviceAdapter::RegisterBuffer(
    uint64_t buffer_id,
    mojom::Camera3DeviceOps::BufferType type,
    mojo::Array<mojo::ScopedHandle> fds,
    uint32_t drm_format,
    mojom::HalPixelFormat hal_pixel_format,
    uint32_t width,
    uint32_t height,
    mojo::Array<uint32_t> strides,
    mojo::Array<uint32_t> offsets) {
  if (!IsMatchingFormat(hal_pixel_format, drm_format)) {
    LOG(ERROR) << "HAL pixel format " << hal_pixel_format
               << " does not match DRM format " << FormatToString(drm_format);
    return -EINVAL;
  }
  size_t num_planes = fds.size();

  std::unique_ptr<camera_buffer_handle_t> buffer_handle(
      new camera_buffer_handle_t());
  buffer_handle->base.version = sizeof(buffer_handle->base);
  buffer_handle->base.numFds = kCameraBufferHandleNumFds;
  buffer_handle->base.numInts = kCameraBufferHandleNumInts;

  buffer_handle->magic = kCameraBufferMagic;
  buffer_handle->buffer_id = buffer_id;
  buffer_handle->type = static_cast<int32_t>(type);
  buffer_handle->drm_format = drm_format;
  buffer_handle->hal_pixel_format = static_cast<uint32_t>(hal_pixel_format);
  buffer_handle->width = width;
  buffer_handle->height = height;
  for (size_t i = 0; i < num_planes; ++i) {
    buffer_handle->fds[i] = UnwrapPlatformHandle(std::move(fds[i]));
    buffer_handle->strides[i] = strides[i];
    buffer_handle->offsets[i] = offsets[i];
  }
  {
    base::AutoLock l(buffer_handles_lock_);
    buffer_handles_[buffer_id] = std::move(buffer_handle);
  }

  VLOGF(1) << std::hex << "Buffer 0x" << buffer_id << " registered: "
           << "format: " << FormatToString(drm_format)
           << " dimension: " << std::dec << width << "x" << height
           << " num_planes: " << num_planes;
  return 0;
}

int32_t CameraDeviceAdapter::Close() {
  // Close the device.
  VLOGF_ENTER();
  if (device_closed_) {
    return 0;
  }
  reprocess_effect_thread_.Stop();
  int32_t ret = camera_device_->common.close(&camera_device_->common);
  device_closed_ = true;
  DCHECK_EQ(ret, 0);
  fence_sync_thread_.Stop();
  close_callback_.Run();
  return ret;
}

// static
void CameraDeviceAdapter::ProcessCaptureResult(
    const camera3_callback_ops_t* ops,
    const camera3_capture_result_t* result) {
  VLOGF_ENTER();
  CameraDeviceAdapter* self = const_cast<CameraDeviceAdapter*>(
      static_cast<const CameraDeviceAdapter*>(ops));

  camera3_capture_result_t res = *result;
  camera3_stream_buffer_t in_buf = {};
  mojom::Camera3CaptureResultPtr result_ptr;
  {
    base::AutoLock reprocess_handles_lock(self->reprocess_handles_lock_);
    if (result->input_buffer && !self->reprocess_handles_.empty() &&
        *result->input_buffer->buffer ==
            *self->reprocess_handles_.front().GetHandle()) {
      in_buf = *result->input_buffer;
      // Restore original input buffer
      base::AutoLock buffer_handles_lock(self->buffer_handles_lock_);
      in_buf.buffer =
          &self->buffer_handles_.at(self->input_buffer_handle_ids_.front())
               ->self;
      self->reprocess_handles_.pop_front();
      self->input_buffer_handle_ids_.pop_front();
      res.input_buffer = &in_buf;
    }
  }
  {
    base::AutoLock reprocess_result_metadata_lock(
        self->reprocess_result_metadata_lock_);
    auto it = self->reprocess_result_metadata_.find(res.frame_number);
    if (it != self->reprocess_result_metadata_.end() && !it->second.isEmpty() &&
        res.result != nullptr) {
      it->second.append(res.result);
      res.result = it->second.getAndLock();
    }
    result_ptr = self->PrepareCaptureResult(&res);
    if (res.result != nullptr) {
      self->reprocess_result_metadata_.erase(res.frame_number);
    }
  }

  base::AutoLock l(self->callback_ops_delegate_lock_);
  if (self->callback_ops_delegate_) {
    self->callback_ops_delegate_->ProcessCaptureResult(std::move(result_ptr));
  }
}

// static
void CameraDeviceAdapter::Notify(const camera3_callback_ops_t* ops,
                                 const camera3_notify_msg_t* msg) {
  VLOGF_ENTER();
  CameraDeviceAdapter* self = const_cast<CameraDeviceAdapter*>(
      static_cast<const CameraDeviceAdapter*>(ops));
  mojom::Camera3NotifyMsgPtr msg_ptr = self->PrepareNotifyMsg(msg);
  base::AutoLock l(self->callback_ops_delegate_lock_);
  if (self->callback_ops_delegate_) {
    self->callback_ops_delegate_->Notify(std::move(msg_ptr));
  }
  if (msg->type == CAMERA3_MSG_ERROR &&
      msg->message.error.error_code == CAMERA3_MSG_ERROR_DEVICE) {
    LOGF(ERROR) << "Fatal device error; aborting the camera service";
    _exit(EIO);
  }
}

mojom::Camera3CaptureResultPtr CameraDeviceAdapter::PrepareCaptureResult(
    const camera3_capture_result_t* result) {
  mojom::Camera3CaptureResultPtr r = mojom::Camera3CaptureResult::New();

  r->frame_number = result->frame_number;

  r->result = internal::SerializeCameraMetadata(result->result);

  // Serialize output buffers.  This may be none as num_output_buffers may be 0.
  if (result->output_buffers) {
    base::AutoLock streams_lock(streams_lock_);
    base::AutoLock buffer_handles_lock(buffer_handles_lock_);
    mojo::Array<mojom::Camera3StreamBufferPtr> output_buffers;
    for (size_t i = 0; i < result->num_output_buffers; i++) {
      mojom::Camera3StreamBufferPtr out_buf = internal::SerializeStreamBuffer(
          result->output_buffers + i, streams_, buffer_handles_);
      if (out_buf.is_null()) {
        LOGF(ERROR) << "Failed to serialize output stream buffer";
        // TODO(jcliang): Handle error?
      }
      buffer_handles_[out_buf->buffer_id]->state = kReturned;
      RemoveBufferLocked(*(result->output_buffers + i));
      output_buffers.push_back(std::move(out_buf));
    }
    r->output_buffers = std::move(output_buffers);
  }

  // Serialize input buffer.
  if (result->input_buffer) {
    base::AutoLock streams_lock(streams_lock_);
    base::AutoLock buffer_handles_lock(buffer_handles_lock_);
    mojom::Camera3StreamBufferPtr input_buffer =
        internal::SerializeStreamBuffer(result->input_buffer, streams_,
                                        buffer_handles_);
    if (input_buffer.is_null()) {
      LOGF(ERROR) << "Failed to serialize input stream buffer";
    }
    buffer_handles_[input_buffer->buffer_id]->state = kReturned;
    RemoveBufferLocked(*result->input_buffer);
    r->input_buffer = std::move(input_buffer);
  }

  r->partial_result = result->partial_result;

  return r;
}

mojom::Camera3NotifyMsgPtr CameraDeviceAdapter::PrepareNotifyMsg(
    const camera3_notify_msg_t* msg) {
  // Fill in the data from msg...
  mojom::Camera3NotifyMsgPtr m = mojom::Camera3NotifyMsg::New();
  m->type = static_cast<mojom::Camera3MsgType>(msg->type);
  m->message = mojom::Camera3NotifyMsgMessage::New();

  if (msg->type == CAMERA3_MSG_ERROR) {
    mojom::Camera3ErrorMsgPtr error = mojom::Camera3ErrorMsg::New();
    error->frame_number = msg->message.error.frame_number;
    uint64_t stream_id = 0;
    {
      base::AutoLock l(streams_lock_);
      for (const auto& s : streams_) {
        if (s.second.get() == msg->message.error.error_stream) {
          stream_id = s.first;
          break;
        }
      }
    }
    error->error_stream_id = stream_id;
    error->error_code =
        static_cast<mojom::Camera3ErrorMsgCode>(msg->message.error.error_code);
    m->message->set_error(std::move(error));
  } else if (msg->type == CAMERA3_MSG_SHUTTER) {
    mojom::Camera3ShutterMsgPtr shutter = mojom::Camera3ShutterMsg::New();
    shutter->frame_number = msg->message.shutter.frame_number;
    shutter->timestamp = msg->message.shutter.timestamp;
    m->message->set_shutter(std::move(shutter));
  } else {
    LOGF(ERROR) << "Invalid notify message type: " << msg->type;
  }

  return m;
}

void CameraDeviceAdapter::RemoveBufferLocked(
    const camera3_stream_buffer_t& buffer) {
  buffer_handles_lock_.AssertAcquired();
  int release_fence = buffer.release_fence;
  base::ScopedFD scoped_release_fence;
  if (release_fence != -1) {
    release_fence = dup(release_fence);
    if (release_fence == -1) {
      LOGF(ERROR) << "Failed to dup release_fence: " << strerror(errno);
      return;
    }
    scoped_release_fence.reset(release_fence);
  }

  // Remove the allocated camera buffer handle from |buffer_handles_| and
  // pass it to RemoveBufferOnFenceSyncThread. The buffer handle will be
  // freed after the release fence is signalled.
  const camera_buffer_handle_t* handle =
      camera_buffer_handle_t::FromBufferHandle(*(buffer.buffer));
  if (!handle) {
    return;
  }
  // Remove the buffer handle from |buffer_handles_| now to avoid a race
  // condition where the process_capture_request sends down an existing buffer
  // handle which hasn't been removed in RemoveBufferHandleOnFenceSyncThread.
  uint64_t buffer_id = handle->buffer_id;
  if (buffer_handles_[buffer_id]->state == kRegistered) {
    // Framework registered a new buffer with the same |buffer_id| before we
    // remove the old buffer handle from |buffer_handles_|.
    return;
  }
  std::unique_ptr<camera_buffer_handle_t> buffer_handle;
  buffer_handles_[buffer_id].swap(buffer_handle);
  buffer_handles_.erase(buffer_id);

  fence_sync_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&CameraDeviceAdapter::RemoveBufferOnFenceSyncThread,
                 base::Unretained(this), base::Passed(&scoped_release_fence),
                 base::Passed(&buffer_handle)));
}

void CameraDeviceAdapter::RemoveBufferOnFenceSyncThread(
    base::ScopedFD release_fence,
    std::unique_ptr<camera_buffer_handle_t> buffer) {
  // In theory the release fence should be signaled by HAL as soon as possible,
  // and we could just set a large value for the timeout.  The timeout here is
  // set to 3 ms to allow testing multiple fences in round-robin if there are
  // multiple active buffers.
  const int kSyncWaitTimeoutMs = 3;
  const camera_buffer_handle_t* handle = buffer.get();
  DCHECK(handle);

  if (!release_fence.is_valid() ||
      !sync_wait(release_fence.get(), kSyncWaitTimeoutMs)) {
    VLOGF(1) << "Buffer 0x" << std::hex << handle->buffer_id << " removed";
  } else {
    // sync_wait() timeout. Reschedule and try to remove the buffer again.
    VLOGF(2) << "Release fence sync_wait() timeout on buffer 0x" << std::hex
             << handle->buffer_id;
    fence_sync_thread_.task_runner()->PostTask(
        FROM_HERE,
        base::Bind(&CameraDeviceAdapter::RemoveBufferOnFenceSyncThread,
                   base::Unretained(this), base::Passed(&release_fence),
                   base::Passed(&buffer)));
  }
}

void CameraDeviceAdapter::ReprocessEffectsOnReprocessEffectThread(
    std::unique_ptr<Camera3CaptureRequest> req) {
  VLOGF_ENTER();
  camera3_stream_t* input_stream = req->input_buffer->stream;
  camera3_stream_t* output_stream = req->output_buffers[0].stream;
  // Here we assume reprocessing effects can provide only one output of the
  // same size and format as that of input. Invoke HAL reprocessing if more
  // outputs, scaling and/or format conversion are required since ISP
  // may provide hardware acceleration for these operations.
  bool need_hal_reprocessing =
      (req->num_output_buffers != 1) ||
      (input_stream->width != req->output_buffers[0].stream->width) ||
      (input_stream->height != req->output_buffers[0].stream->height) ||
      (input_stream->format != req->output_buffers[0].stream->format);

  struct ReprocessContext {
    ReprocessContext(CameraDeviceAdapter* d,
                     const Camera3CaptureRequest* r,
                     bool n)
        : result(0),
          device_adapter(d),
          capture_request(r),
          need_hal_reprocessing(n) {}

    ~ReprocessContext() {
      if (result != 0) {
        camera3_notify_msg_t msg{
            .type = CAMERA3_MSG_ERROR,
            .message.error.frame_number = capture_request->frame_number,
            .message.error.error_code = CAMERA3_MSG_ERROR_REQUEST};
        device_adapter->Notify(device_adapter, &msg);
      }
      if (result != 0 || !need_hal_reprocessing) {
        camera3_capture_result_t result{
            .frame_number = capture_request->frame_number,
            .result = capture_request->settings,
            .num_output_buffers = capture_request->num_output_buffers,
            .output_buffers = capture_request->output_buffers,
            .input_buffer = capture_request->input_buffer};
        device_adapter->ProcessCaptureResult(device_adapter, &result);
      }
    }

    int32_t result;
    CameraDeviceAdapter* device_adapter;
    const Camera3CaptureRequest* capture_request;
    bool need_hal_reprocessing;
  };
  ReprocessContext reprocess_context(this, req.get(), need_hal_reprocessing);
  ScopedYUVBufferHandle scoped_output_handle =
      need_hal_reprocessing ?
                            // Allocate reprocessing buffer
          ScopedYUVBufferHandle::AllocateScopedYUVHandle(
              input_stream->width, input_stream->height,
              GRALLOC_USAGE_SW_READ_OFTEN | GRALLOC_USAGE_SW_WRITE_OFTEN)
                            :
                            // Wrap the output buffer
          ScopedYUVBufferHandle::CreateScopedYUVHandle(
              *req->output_buffers[0].buffer, output_stream->width,
              output_stream->height,
              GRALLOC_USAGE_SW_READ_OFTEN | GRALLOC_USAGE_SW_WRITE_OFTEN);
  if (!scoped_output_handle) {
    LOGF(ERROR) << "Failed to create scoped output buffer";
    reprocess_context.result = -EINVAL;
    return;
  }
  ScopedYUVBufferHandle scoped_input_handle =
      ScopedYUVBufferHandle::CreateScopedYUVHandle(
          *req->input_buffer->buffer, input_stream->width, input_stream->height,
          GRALLOC_USAGE_SW_READ_OFTEN | GRALLOC_USAGE_SW_WRITE_OFTEN);
  if (!scoped_input_handle) {
    LOGF(ERROR) << "Failed to create scoped input buffer";
    reprocess_context.result = -EINVAL;
    return;
  }
  android::CameraMetadata reprocess_result_metadata;
  reprocess_context.result = reprocess_effect_callback_.Run(
      *req->settings, &scoped_input_handle, input_stream->width,
      input_stream->height, &reprocess_result_metadata, &scoped_output_handle);
  if (reprocess_context.result != 0) {
    LOGF(ERROR) << "Failed to apply reprocess effect";
    return;
  }
  if (need_hal_reprocessing) {
    // Replace the input buffer with reprocessing output buffer
    {
      base::AutoLock reprocess_handles_lock(reprocess_handles_lock_);
      reprocess_handles_.push_back(std::move(scoped_output_handle));
      input_buffer_handle_ids_.push_back(
          reinterpret_cast<const camera_buffer_handle_t*>(
              *req->input_buffer->buffer)
              ->buffer_id);
      req->input_buffer->buffer = reprocess_handles_.back().GetHandle();
    }
    {
      base::AutoLock reprocess_result_metadata_lock(
          reprocess_result_metadata_lock_);
      reprocess_result_metadata_.emplace(req->frame_number,
                                         reprocess_result_metadata);
    }
    reprocess_context.result =
        camera_device_->ops->process_capture_request(camera_device_, req.get());
    if (reprocess_context.result != 0) {
      LOGF(ERROR) << "Failed to process capture request after reprocessing";
    }
  }
}

void CameraDeviceAdapter::ResetDeviceOpsDelegateOnThread() {
  DCHECK(camera_device_ops_thread_.task_runner()->BelongsToCurrentThread());
  device_ops_delegate_.reset();
}

void CameraDeviceAdapter::ResetCallbackOpsDelegateOnThread() {
  DCHECK(camera_callback_ops_thread_.task_runner()->BelongsToCurrentThread());
  base::AutoLock l(callback_ops_delegate_lock_);
  callback_ops_delegate_.reset();
}

}  // namespace cros
