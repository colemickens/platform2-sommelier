/*
 * Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "hal_adapter/camera_device_adapter.h"

#include <map>
#include <set>
#include <utility>
#include <vector>

#include <base/bind.h>
#include <base/bind_helpers.h>
#include <base/logging.h>
#include <drm_fourcc.h>
#include <sync/sync.h>

#include "arc/camera_buffer_mapper.h"
#include "arc/common.h"
#include "common/camera_buffer_handle.h"
#include "hal_adapter/camera3_callback_ops_delegate.h"
#include "hal_adapter/camera3_device_ops_delegate.h"

namespace arc {

CameraDeviceAdapter::CameraDeviceAdapter(camera3_device_t* camera_device,
                                         base::Callback<void()> close_callback)
    : camera_device_ops_thread_("CameraDeviceOpsThread"),
      camera_callback_ops_thread_("CameraCallbackOpsThread"),
      fence_sync_thread_("FenceSyncThread"),
      close_callback_(close_callback),
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

bool CameraDeviceAdapter::Start() {
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
  return true;
}

void CameraDeviceAdapter::Bind(
    mojom::Camera3DeviceOpsRequest device_ops_request) {
  device_ops_delegate_->Bind(device_ops_request.PassMessagePipe());
}

int32_t CameraDeviceAdapter::Initialize(
    mojom::Camera3CallbackOpsPtr callback_ops) {
  VLOGF_ENTER();
  DCHECK(!callback_ops_delegate_);
  if (!fence_sync_thread_.Start()) {
    LOGF(ERROR) << "Fence sync thread failed to start";
    return -ENODEV;
  }
  callback_ops_delegate_.reset(new Camera3CallbackOpsDelegate(
      this, callback_ops.PassInterface(),
      camera_callback_ops_thread_.task_runner()));
  return camera_device_->ops->initialize(camera_device_, this);
}

int32_t CameraDeviceAdapter::ConfigureStreams(
    mojom::Camera3StreamConfigurationPtr config,
    mojom::Camera3StreamConfigurationPtr* updated_config) {
  VLOGF_ENTER();

  base::AutoLock l(streams_lock_);

  std::map<uint64_t, std::unique_ptr<camera3_stream_t>> new_streams;
  for (const auto& s : config->streams) {
    uint64_t id = s->id;
    std::unique_ptr<camera3_stream_t>& stream = new_streams[id];
    stream.reset(new camera3_stream_t);
    memset(stream.get(), 0, sizeof(*stream.get()));
    stream->stream_type = static_cast<camera3_stream_type_t>(s->stream_type);
    stream->width = s->width;
    stream->height = s->height;
    stream->format = static_cast<int32_t>(s->format);
    stream->data_space = static_cast<android_dataspace_t>(s->data_space);
    stream->rotation = static_cast<camera3_stream_rotation_t>(s->rotation);
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

  internal::CameraMetadataUniquePtr settings =
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
  std::vector<buffer_handle_t> output_buffer_handles(num_output_buffers);
  {
    base::AutoLock streams_lock(streams_lock_);
    base::AutoLock buffer_handles_lock(buffer_handles_lock_);
    for (size_t i = 0; i < num_output_buffers; ++i) {
      mojom::Camera3StreamBufferPtr& out_buf_ptr = request->output_buffers[i];
      output_buffers.at(i).buffer =
          const_cast<const native_handle_t**>(&output_buffer_handles.at(i));
      internal::DeserializeStreamBuffer(out_buf_ptr, streams_, buffer_handles_,
                                        &output_buffers.at(i));
    }
    req.output_buffers =
        const_cast<const camera3_stream_buffer_t*>(output_buffers.data());
  }

  return camera_device_->ops->process_capture_request(camera_device_, &req);
}

void CameraDeviceAdapter::Dump(mojo::ScopedHandle fd) {
  VLOGF_ENTER();
  base::ScopedFD dump_fd(internal::UnwrapPlatformHandle(std::move(fd)));
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
    buffer_handle->fds[i].reset(
        internal::UnwrapPlatformHandle(std::move(fds[i])));
    buffer_handle->strides[i] = strides[i];
    buffer_handle->offsets[i] = offsets[i];
  }
  DCHECK(buffer_handles_.find(buffer_id) == buffer_handles_.end());
  buffer_handles_[buffer_id] = std::move(buffer_handle);

  VLOGF(1) << std::hex << "Buffer 0x" << buffer_id << " registered: "
           << "format: " << FormatToString(drm_format)
           << " dimension: " << std::dec << width << "x" << height
           << " num_planes: " << num_planes;
  return 0;
}

int32_t CameraDeviceAdapter::Close() {
  // Close the device.
  VLOGF_ENTER();
  int32_t ret = camera_device_->common.close(&camera_device_->common);
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
  mojom::Camera3CaptureResultPtr result_ptr =
      self->PrepareCaptureResult(result);
  self->callback_ops_delegate_->ProcessCaptureResult(std::move(result_ptr));
}

// static
void CameraDeviceAdapter::Notify(const camera3_callback_ops_t* ops,
                                 const camera3_notify_msg_t* msg) {
  VLOGF_ENTER();
  CameraDeviceAdapter* self = const_cast<CameraDeviceAdapter*>(
      static_cast<const CameraDeviceAdapter*>(ops));
  mojom::Camera3NotifyMsgPtr msg_ptr = self->PrepareNotifyMsg(msg);
  self->callback_ops_delegate_->Notify(std::move(msg_ptr));
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

void CameraDeviceAdapter::ResetDeviceOpsDelegateOnThread() {
  DCHECK(camera_device_ops_thread_.task_runner()->BelongsToCurrentThread());
  device_ops_delegate_.reset();
}

void CameraDeviceAdapter::ResetCallbackOpsDelegateOnThread() {
  DCHECK(camera_callback_ops_thread_.task_runner()->BelongsToCurrentThread());
  callback_ops_delegate_.reset();
}

}  // namespace arc
