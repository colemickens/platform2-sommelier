/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "hal/usb/camera_client.h"

#include <utility>
#include <vector>

#include <sync/sync.h>
#include <system/camera_metadata.h>

#include "cros-camera/common.h"
#include "hal/usb/cached_frame.h"
#include "hal/usb/camera_hal.h"
#include "hal/usb/camera_hal_device_ops.h"
#include "hal/usb/stream_format.h"

namespace cros {

const int kBufferFenceReady = -1;

// We need to compare the aspect ratio from native sensor resolution.
// The native resolution may not be just the size. It may be a little larger.
// Add a margin to check if the sensor aspect ratio fall in the specific aspect
// ratio.
// 16:9=1.778, 16:10=1.6, 3:2=1.5, 4:3=1.333
const float kAspectRatioMargin = 0.04;

CameraClient::CameraClient(int id,
                           const DeviceInfo& device_info,
                           const camera_metadata_t& static_info,
                           const hw_module_t* module,
                           hw_device_t** hw_device)
    : id_(id),
      device_info_(device_info),
      device_(new V4L2CameraDevice(device_info)),
      callback_ops_(nullptr),
      metadata_handler_(new MetadataHandler(static_info)),
      request_thread_("Capture request thread") {
  memset(&camera3_device_, 0, sizeof(camera3_device_));
  camera3_device_.common.tag = HARDWARE_DEVICE_TAG;
  camera3_device_.common.version = CAMERA_DEVICE_API_VERSION_3_3;
  camera3_device_.common.close = cros::camera_device_close;
  camera3_device_.common.module = const_cast<hw_module_t*>(module);
  camera3_device_.ops = &g_camera_device_ops;
  camera3_device_.priv = this;
  *hw_device = &camera3_device_.common;

  ops_thread_checker_.DetachFromThread();

  SupportedFormats supported_formats =
      device_->GetDeviceSupportedFormats(device_info_.device_path);
  qualified_formats_ = GetQualifiedFormats(supported_formats);
}

CameraClient::~CameraClient() {}

int CameraClient::OpenDevice() {
  VLOGFID(1, id_);
  DCHECK(thread_checker_.CalledOnValidThread());

  int ret = device_->Connect(device_info_.device_path);
  if (ret) {
    LOGFID(ERROR, id_) << "Connect failed: " << strerror(-ret);
    return ret;
  }

  return 0;
}

int CameraClient::CloseDevice() {
  VLOGFID(1, id_);
  DCHECK(ops_thread_checker_.CalledOnValidThread());

  StreamOff();
  device_->Disconnect();
  return 0;
}

int CameraClient::Initialize(const camera3_callback_ops_t* callback_ops) {
  VLOGFID(1, id_);
  DCHECK(ops_thread_checker_.CalledOnValidThread());

  callback_ops_ = callback_ops;
  return 0;
}

int CameraClient::ConfigureStreams(
    camera3_stream_configuration_t* stream_config) {
  VLOGFID(1, id_);
  DCHECK(ops_thread_checker_.CalledOnValidThread());
  /* TODO(henryhsu):
   * 1. Remove all pending requests. Post a task to request thread and wait for
   *    the task to be run.
   */
  if (callback_ops_ == nullptr) {
    LOGFID(ERROR, id_) << "Device is not initialized";
    return -EINVAL;
  }

  if (stream_config == nullptr) {
    LOGFID(ERROR, id_) << "NULL stream configuration array";
    return -EINVAL;
  }
  if (stream_config->num_streams == 0) {
    LOGFID(ERROR, id_) << "Empty stream configuration array";
    return -EINVAL;
  }
  if (stream_config->operation_mode !=
      CAMERA3_STREAM_CONFIGURATION_NORMAL_MODE) {
    LOGFID(ERROR, id_) << "Unsupported operation mode: "
                       << stream_config->operation_mode;
    return -EINVAL;
  }

  VLOGFID(1, id_) << "Number of Streams: " << stream_config->num_streams;

  Size stream_on_resolution(0, 0);
  std::vector<camera3_stream_t*> streams;
  int crop_rotate_scale_degrees = 0;
  for (size_t i = 0; i < stream_config->num_streams; i++) {
    VLOGFID(1, id_) << "Stream[" << i
                    << "] type=" << stream_config->streams[i]->stream_type
                    << " width=" << stream_config->streams[i]->width
                    << " height=" << stream_config->streams[i]->height
                    << " rotation=" << stream_config->streams[i]->rotation
                    << " degrees="
                    << stream_config->streams[i]->crop_rotate_scale_degrees
                    << " format=0x" << std::hex
                    << stream_config->streams[i]->format;

    if (!IsFormatSupported(qualified_formats_, *(stream_config->streams[i]))) {
      LOGF(ERROR) << "Unsupported stream parameters. Width: "
                  << stream_config->streams[i]->width
                  << ", height: " << stream_config->streams[i]->height
                  << ", format: " << stream_config->streams[i]->format;
      return -EINVAL;
    }
    streams.push_back(stream_config->streams[i]);
    if (i && stream_config->streams[i]->crop_rotate_scale_degrees !=
                 stream_config->streams[i - 1]->crop_rotate_scale_degrees) {
      LOGF(ERROR) << "Unsupported different crop ratate scale degrees";
      return -EINVAL;
    }
    // Here assume the attribute of all streams are the same.
    switch (stream_config->streams[i]->crop_rotate_scale_degrees) {
      case CAMERA3_STREAM_ROTATION_0:
        crop_rotate_scale_degrees = 0;
        break;
      case CAMERA3_STREAM_ROTATION_90:
        crop_rotate_scale_degrees = 90;
        break;
      case CAMERA3_STREAM_ROTATION_270:
        crop_rotate_scale_degrees = 270;
        break;
      default:
        LOGF(ERROR) << "Unrecognized crop_rotate_scale_degrees: "
                    << stream_config->streams[i]->crop_rotate_scale_degrees;
        return -EINVAL;
    }

    // Skip BLOB format to avoid to use too large resolution as preview size.
    if (stream_config->streams[i]->format == HAL_PIXEL_FORMAT_BLOB &&
        stream_config->num_streams > 1)
      continue;

    // Find maximum area of stream_config to stream on.
    if (stream_config->streams[i]->width * stream_config->streams[i]->height >
        stream_on_resolution.width * stream_on_resolution.height) {
      stream_on_resolution.width = stream_config->streams[i]->width;
      stream_on_resolution.height = stream_config->streams[i]->height;
    }
  }

  if (!IsValidStreamSet(streams)) {
    LOGFID(ERROR, id_) << "Invalid stream set";
    return -EINVAL;
  }

  Size resolution(0, 0);
  bool use_native_sensor_ratio;
  use_native_sensor_ratio =
      ShouldUseNativeSensorRatio(*stream_config, &resolution);
  if (use_native_sensor_ratio) {
    stream_on_resolution = resolution;
  }

  // We don't have enough information to decide whether to enable constant frame
  // rate or not here.  Tried some common camera apps and seems that true is a
  // sensible default.
  bool constant_frame_rate = true;

  int num_buffers;
  int ret = StreamOn(stream_on_resolution, constant_frame_rate,
                     crop_rotate_scale_degrees, &num_buffers,
                     use_native_sensor_ratio);
  if (ret) {
    LOGFID(ERROR, id_) << "StreamOn failed";
    StreamOff();
    return ret;
  }
  SetUpStreams(num_buffers, &streams);

  return 0;
}

const camera_metadata_t* CameraClient::ConstructDefaultRequestSettings(
    int type) {
  VLOGFID(1, id_) << "type=" << type;

  return metadata_handler_->GetDefaultRequestSettings(type);
}

int CameraClient::ProcessCaptureRequest(camera3_capture_request_t* request) {
  VLOGFID(1, id_);
  DCHECK(ops_thread_checker_.CalledOnValidThread());

  if (!request_handler_.get()) {
    LOGFID(INFO, id_) << "Request handler has stopped; ignoring request";
    return -ENODEV;
  }

  if (request == nullptr) {
    LOGFID(ERROR, id_) << "NULL request recieved";
    return -EINVAL;
  }

  VLOGFID(1, id_) << "Request Frame:" << request->frame_number
                  << ", settings:" << request->settings;

  if (request->input_buffer != nullptr) {
    LOGFID(ERROR, id_) << "Input buffer is not supported";
    return -EINVAL;
  }

  if (request->num_output_buffers <= 0) {
    LOGFID(ERROR, id_) << "Invalid number of output buffers: "
                       << request->num_output_buffers;
    return -EINVAL;
  }

  if (request->settings) {
    latest_request_metadata_ = request->settings;
    if (VLOG_IS_ON(2)) {
      dump_camera_metadata(request->settings, 1, 1);
    }
  }

  for (size_t i = 0; i < request->num_output_buffers; i++) {
    const camera3_stream_buffer_t* buffer = &request->output_buffers[i];
    if (!IsFormatSupported(qualified_formats_, *(buffer->stream))) {
      LOGF(ERROR) << "Unsupported stream parameters. Width: "
                  << buffer->stream->width
                  << ", height: " << buffer->stream->height
                  << ", format: " << buffer->stream->format;
      return -EINVAL;
    }
  }

  // We cannot use |request| after this function returns. So we have to copy
  // necessary information out to |capture_request|. If |request->settings|
  // doesn't exist, use previous metadata.
  std::unique_ptr<CaptureRequest> capture_request(
      new CaptureRequest(*request, latest_request_metadata_));
  request_task_runner_->PostTask(
      FROM_HERE, base::Bind(&CameraClient::RequestHandler::HandleRequest,
                            base::Unretained(request_handler_.get()),
                            base::Passed(&capture_request)));
  return 0;
}

void CameraClient::Dump(int fd) {
  VLOGFID(1, id_);
}

int CameraClient::Flush(const camera3_device_t* dev) {
  VLOGFID(1, id_);

  // Do nothing if stream is off.
  if (!request_handler_.get()) {
    return 0;
  }

  auto future = cros::Future<int>::Create(nullptr);
  request_handler_->HandleFlush(cros::GetFutureCallback(future));
  future->Get();
  return 0;
}

bool CameraClient::IsValidStreamSet(
    const std::vector<camera3_stream_t*>& streams) {
  DCHECK(ops_thread_checker_.CalledOnValidThread());
  int num_input = 0, num_output = 0;

  // Validate there is no input stream and at least one output stream.
  for (const auto& stream : streams) {
    // A stream may be both input and output (bidirectional).
    if (stream->stream_type == CAMERA3_STREAM_INPUT ||
        stream->stream_type == CAMERA3_STREAM_BIDIRECTIONAL)
      num_input++;
    if (stream->stream_type == CAMERA3_STREAM_OUTPUT ||
        stream->stream_type == CAMERA3_STREAM_BIDIRECTIONAL)
      num_output++;

    if (stream->rotation != CAMERA3_STREAM_ROTATION_0) {
      LOGFID(ERROR, id_) << "Unsupported rotation " << stream->rotation;
      return false;
    }
  }
  VLOGFID(1, id_) << "Configuring " << num_output << " output streams and "
                  << num_input << " input streams";

  if (num_output < 1) {
    LOGFID(ERROR, id_) << "Stream config must have >= 1 output";
    return false;
  }
  if (num_input > 0) {
    LOGFID(ERROR, id_) << "Input Stream is not supported. Number: "
                       << num_input;
    return false;
  }
  return true;
}

void CameraClient::SetUpStreams(int num_buffers,
                                std::vector<camera3_stream_t*>* streams) {
  for (auto& stream : *streams) {
    uint32_t usage = 0;
    if (stream->stream_type == CAMERA3_STREAM_OUTPUT ||
        stream->stream_type == CAMERA3_STREAM_BIDIRECTIONAL) {
      usage |= GRALLOC_USAGE_SW_WRITE_OFTEN | GRALLOC_USAGE_HW_CAMERA_READ |
               GRALLOC_USAGE_HW_CAMERA_WRITE;
    }
    stream->usage = usage;
    stream->max_buffers = num_buffers;
  }
}

int CameraClient::StreamOn(Size stream_on_resolution,
                           bool constant_frame_rate,
                           int crop_rotate_scale_degrees,
                           int* num_buffers,
                           bool use_native_sensor_ratio) {
  DCHECK(ops_thread_checker_.CalledOnValidThread());

  if (!request_handler_.get()) {
    if (!request_thread_.Start()) {
      LOGFID(ERROR, id_) << "Request thread failed to start";
      return -EINVAL;
    }
    request_task_runner_ = request_thread_.task_runner();

    request_handler_.reset(
        new RequestHandler(id_, device_info_, device_.get(), callback_ops_,
                           request_task_runner_, metadata_handler_.get()));
  }

  auto future = cros::Future<int>::Create(nullptr);
  base::Callback<void(int, int)> streamon_callback =
      base::Bind(&CameraClient::StreamOnCallback, base::Unretained(this),
                 base::RetainedRef(future), num_buffers);
  request_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&CameraClient::RequestHandler::StreamOn,
                 base::Unretained(request_handler_.get()), stream_on_resolution,
                 constant_frame_rate, crop_rotate_scale_degrees,
                 use_native_sensor_ratio, streamon_callback));
  return future->Get();
}

void CameraClient::StreamOff() {
  DCHECK(ops_thread_checker_.CalledOnValidThread());
  if (request_handler_.get()) {
    auto future = cros::Future<int>::Create(nullptr);
    base::Callback<void(int)> streamoff_callback =
        base::Bind(&CameraClient::StreamOffCallback, base::Unretained(this),
                   base::RetainedRef(future));
    request_task_runner_->PostTask(
        FROM_HERE, base::Bind(&CameraClient::RequestHandler::StreamOff,
                              base::Unretained(request_handler_.get()),
                              streamoff_callback));
    int ret = future->Get();
    if (ret) {
      LOGFID(ERROR, id_) << "StreamOff failed";
    }
    request_thread_.Stop();
    request_handler_.reset();
  }
}

void CameraClient::StreamOnCallback(scoped_refptr<cros::Future<int>> future,
                                    int* out_num_buffers,
                                    int num_buffers,
                                    int result) {
  if (!result && out_num_buffers) {
    *out_num_buffers = num_buffers;
  }
  future->Set(result);
}

void CameraClient::StreamOffCallback(scoped_refptr<cros::Future<int>> future,
                                     int result) {
  future->Set(result);
}

bool CameraClient::ShouldUseNativeSensorRatio(
    const camera3_stream_configuration_t& stream_config, Size* resolution) {
  bool try_native_sensor_ratio = false;

  // Check if we have more than 1 resolution.
  for (size_t i = 1; i < stream_config.num_streams; i++) {
    if (stream_config.streams[0]->width != stream_config.streams[i]->width ||
        stream_config.streams[0]->height != stream_config.streams[i]->height) {
      try_native_sensor_ratio = true;
      break;
    }
  }
  if (!try_native_sensor_ratio)
    return false;

  // Find maximum width and height of all streams.
  Size max_stream_resolution(0, 0);
  for (size_t i = 0; i < stream_config.num_streams; i++) {
    if (stream_config.streams[i]->width > max_stream_resolution.width) {
      max_stream_resolution.width = stream_config.streams[i]->width;
    }
    if (stream_config.streams[i]->height > max_stream_resolution.height) {
      max_stream_resolution.height = stream_config.streams[i]->height;
    }
  }

  bool use_native_sensor_ratio = false;
  // Find the same ratio maximium resolution with minimum 30 fps.
  float target_aspect_ratio =
      static_cast<float>(device_info_.sensor_info_pixel_array_size_width) /
      device_info_.sensor_info_pixel_array_size_height;

  resolution->width = 0;
  resolution->height = 0;

  VLOGFID(1, id_) << "native aspect ratio:" << target_aspect_ratio;
  for (const auto& format : qualified_formats_) {
    float max_fps = GetMaximumFrameRate(format);
    if (max_fps < 29.0) {
      continue;
    }
    if (format.width < max_stream_resolution.width ||
        format.height < max_stream_resolution.height) {
      continue;
    }
    if (format.width < resolution->width ||
        format.height < resolution->height) {
      continue;
    }
    float aspect_ratio = static_cast<float>(format.width) / format.height;
    VLOGFID(2, id_) << "Try " << format.width << "," << format.height << "("
                    << aspect_ratio << ")";
    if (std::fabs(target_aspect_ratio - aspect_ratio) < kAspectRatioMargin) {
      resolution->width = format.width;
      resolution->height = format.height;
      use_native_sensor_ratio = true;
    }
  }
  LOGFID(INFO, id_) << "Use native sensor ratio:" << std::boolalpha
                    << use_native_sensor_ratio << " " << resolution->width
                    << "," << resolution->height;
  return use_native_sensor_ratio;
}

CameraClient::RequestHandler::RequestHandler(
    const int device_id,
    const DeviceInfo& device_info,
    V4L2CameraDevice* device,
    const camera3_callback_ops_t* callback_ops,
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    MetadataHandler* metadata_handler)
    : device_id_(device_id),
      device_info_(device_info),
      device_(device),
      callback_ops_(callback_ops),
      task_runner_(task_runner),
      metadata_handler_(metadata_handler),
      stream_on_resolution_(0, 0),
      current_v4l2_buffer_id_(-1),
      flush_started_(false) {
  SupportedFormats supported_formats =
      device_->GetDeviceSupportedFormats(device_info_.device_path);
  qualified_formats_ = GetQualifiedFormats(supported_formats);
}

CameraClient::RequestHandler::~RequestHandler() {}

void CameraClient::RequestHandler::StreamOn(
    Size stream_on_resolution,
    bool constant_frame_rate,
    int crop_rotate_scale_degrees,
    bool use_native_sensor_ratio,
    const base::Callback<void(int, int)>& callback) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  crop_rotate_scale_degrees_ = crop_rotate_scale_degrees;

  int ret = StreamOnImpl(stream_on_resolution, constant_frame_rate,
                         use_native_sensor_ratio);
  if (ret) {
    callback.Run(0, ret);
  }
  callback.Run(input_buffers_.size(), 0);
}

void CameraClient::RequestHandler::StreamOff(
    const base::Callback<void(int)>& callback) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  int ret = StreamOffImpl();
  callback.Run(ret);
}

void CameraClient::RequestHandler::HandleRequest(
    std::unique_ptr<CaptureRequest> request) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  camera3_capture_result_t capture_result;
  memset(&capture_result, 0, sizeof(camera3_capture_result_t));

  capture_result.frame_number = request->GetFrameNumber();

  std::vector<camera3_stream_buffer_t>* output_stream_buffers =
      request->GetStreamBuffers();
  capture_result.num_output_buffers = output_stream_buffers->size();
  capture_result.output_buffers = &(*output_stream_buffers)[0];

  if (flush_started_) {
    AbortGrallocBufferSync(&capture_result);
    HandleAbortedRequest(&capture_result);
    return;
  }

  if (!WaitGrallocBufferSync(&capture_result)) {
    HandleAbortedRequest(&capture_result);
    return;
  }

  android::CameraMetadata* metadata = request->GetMetadata();
  metadata_handler_->PreHandleRequest(capture_result.frame_number, *metadata);

  VLOGFID(1, device_id_) << "Request Frame:" << capture_result.frame_number
                         << ", Number of output buffers: "
                         << capture_result.num_output_buffers;
  bool constant_frame_rate = ShouldEnableConstantFrameRate(*metadata);
  VLOGFID(1, device_id_) << "constant_frame_rate " << std::boolalpha
                         << constant_frame_rate;

  bool stream_resolution_reconfigure = false;
  Size old_resolution = stream_on_resolution_;
  Size new_resolution = stream_on_resolution_;
  if (!use_native_sensor_ratio_) {
    // Fallback to stream on/off operation case.
    // Check if it requires different configuration for blob format.
    // (Note: We only support one blob format stream.)
    for (size_t i = 0; i < capture_result.num_output_buffers; i++) {
      const camera3_stream_buffer_t* buffer = &capture_result.output_buffers[i];
      if (buffer->stream->format == HAL_PIXEL_FORMAT_BLOB &&
          (new_resolution.width != buffer->stream->width ||
           new_resolution.height != buffer->stream->height)) {
        stream_resolution_reconfigure = true;
        new_resolution.width = buffer->stream->width;
        new_resolution.height = buffer->stream->height;
        break;
      }
    }
  }

  if (stream_resolution_reconfigure ||
      constant_frame_rate != constant_frame_rate_) {
    VLOGFID(1, device_id_) << "Restart stream";
    int ret = StreamOffImpl();
    if (ret) {
      HandleAbortedRequest(&capture_result);
      return;
    }

    ret = StreamOnImpl(new_resolution, constant_frame_rate,
                       use_native_sensor_ratio_);
    if (ret) {
      HandleAbortedRequest(&capture_result);
      return;
    }
  }

  // Handle each stream output buffer and convert it to corresponding format.
  for (size_t i = 0; i < capture_result.num_output_buffers; i++) {
    int ret = WriteStreamBuffer(i, capture_result.num_output_buffers, *metadata,
                                &capture_result.output_buffers[i]);
    if (ret) {
      LOGFID(ERROR, device_id_)
          << "Handle stream buffer failed for output buffer id: " << i;
      camera3_stream_buffer_t* b = const_cast<camera3_stream_buffer_t*>(
          capture_result.output_buffers + i);
      b->status = CAMERA3_BUFFER_STATUS_ERROR;
    }
  }

  NotifyShutter(capture_result.frame_number);
  int ret = metadata_handler_->PostHandleRequest(
      capture_result.frame_number, current_v4l2_buffer_timestamp_, metadata);
  if (ret) {
    LOGFID(WARNING, device_id_)
        << "Update metadata in PostHandleRequest failed";
  }

  capture_result.partial_result = 1;

  // The HAL retains ownership of result structure, which only needs to be valid
  // to access during process_capture_result. The framework will copy whatever
  // it needs before process_capture_result returns. Hence we use getAndLock()
  // instead of release() here, and the underlying buffer would be freed when
  // metadata is out of scope.
  capture_result.result = metadata->getAndLock();

  // After process_capture_result, HAL cannot access the output buffer in
  // camera3_stream_buffer anymore unless the release fence is not -1.
  callback_ops_->process_capture_result(callback_ops_, &capture_result);

  // Switch back to old resolution for non-blob format.
  if (stream_resolution_reconfigure) {
    VLOGFID(1, device_id_) << "Switch back stream";
    ret = StreamOffImpl();
    if (ret) {
      return;
    }

    ret = StreamOnImpl(old_resolution, constant_frame_rate_,
                       use_native_sensor_ratio_);
    if (ret) {
      return;
    }
  }
}

void CameraClient::RequestHandler::HandleFlush(
    const base::Callback<void(int)>& callback) {
  VLOGFID(1, device_id_);
  {
    base::AutoLock l(flush_lock_);
    flush_started_ = true;
  }
  task_runner_->PostTask(FROM_HERE,
                         base::Bind(&CameraClient::RequestHandler::FlushDone,
                                    base::Unretained(this), callback));
}

int CameraClient::RequestHandler::StreamOnImpl(Size stream_on_resolution,
                                               bool constant_frame_rate,
                                               bool use_native_sensor_ratio) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  int ret;
  // If new stream configuration is the same as current stream, do nothing.
  if (stream_on_resolution.width == stream_on_resolution_.width &&
      stream_on_resolution.height == stream_on_resolution_.height &&
      constant_frame_rate == constant_frame_rate_ &&
      use_native_sensor_ratio == use_native_sensor_ratio_) {
    VLOGFID(1, device_id_) << "Skip stream on for the same configuration";
    return 0;
  } else if (!input_buffers_.empty()) {
    // StreamOff first if stream is started.
    ret = StreamOffImpl();
    if (ret) {
      LOGFID(ERROR, device_id_) << "Restart stream failed.";
      return ret;
    }
  }
  const SupportedFormat* format =
      FindFormatByResolution(qualified_formats_, stream_on_resolution.width,
                             stream_on_resolution.height);
  if (format == nullptr) {
    LOGFID(ERROR, device_id_)
        << "Cannot find resolution in supported list: width "
        << stream_on_resolution.width << ", height "
        << stream_on_resolution.height;
    return -EINVAL;
  }

  float max_fps = GetMaximumFrameRate(*format);
  VLOGFID(1, device_id_) << "streamOn with width " << format->width
                         << ", height " << format->height << ", fps " << max_fps
                         << ", format " << FormatToString(format->fourcc)
                         << ", constant_frame_rate " << std::boolalpha
                         << constant_frame_rate;

  std::vector<base::ScopedFD> fds;
  uint32_t buffer_size;
  ret = device_->StreamOn(format->width, format->height, format->fourcc,
                          max_fps, constant_frame_rate, &fds, &buffer_size);
  if (ret) {
    LOGFID(ERROR, device_id_) << "StreamOn failed: " << strerror(-ret);
    return ret;
  }

  for (size_t i = 0; i < fds.size(); i++) {
    std::unique_ptr<V4L2FrameBuffer> frame(
        new V4L2FrameBuffer(std::move(fds[i]), buffer_size, format->width,
                            format->height, format->fourcc));
    ret = frame->Map();
    if (ret) {
      return -errno;
    }
    VLOGFID(1, device_id_) << "Buffer " << i << ", fd: " << frame->GetFd()
                           << " address: " << std::hex
                           << reinterpret_cast<uintptr_t>(frame->GetData());
    input_buffers_.push_back(std::move(frame));
  }

  stream_on_resolution_ = stream_on_resolution;
  constant_frame_rate_ = constant_frame_rate;
  use_native_sensor_ratio_ = use_native_sensor_ratio;
  SkipFramesAfterStreamOn(device_info_.frames_to_skip_after_streamon);

  // Reset test pattern.
  test_pattern_.reset(new TestPattern(stream_on_resolution_));
  return 0;
}

int CameraClient::RequestHandler::StreamOffImpl() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  input_buffers_.clear();
  int ret = device_->StreamOff();
  if (ret) {
    LOGFID(ERROR, device_id_) << "StreamOff failed: " << strerror(-ret);
  }
  stream_on_resolution_.width = stream_on_resolution_.height = 0;
  return ret;
}

void CameraClient::RequestHandler::HandleAbortedRequest(
    camera3_capture_result_t* capture_result) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  VLOGFID(1, device_id_) << "Request Frame:" << capture_result->frame_number
                         << " is aborted due to flush";
  for (size_t i = 0; i < capture_result->num_output_buffers; i++) {
    camera3_stream_buffer_t* b = const_cast<camera3_stream_buffer_t*>(
        capture_result->output_buffers + i);
    b->status = CAMERA3_BUFFER_STATUS_ERROR;
  }
  NotifyRequestError(capture_result->frame_number);
  callback_ops_->process_capture_result(callback_ops_, capture_result);
}

bool CameraClient::RequestHandler::ShouldEnableConstantFrameRate(
    const android::CameraMetadata& metadata) {
  if (device_info_.constant_framerate_unsupported) {
    LOGF(ERROR) << "All usb camera modules should support constant frame rate "
                   "in HAL v3";
    return false;
  }

  // TODO(shik): Add a helper function to do the exists() and find() combo, so
  // it's less likely to have typos in the tag name.

  if (metadata.exists(ANDROID_CONTROL_AE_TARGET_FPS_RANGE)) {
    camera_metadata_ro_entry entry =
        metadata.find(ANDROID_CONTROL_AE_TARGET_FPS_RANGE);
    if (entry.data.i32[0] == entry.data.i32[1]) {
      return true;
    }
  }

  if (metadata.exists(ANDROID_CONTROL_CAPTURE_INTENT)) {
    camera_metadata_ro_entry entry =
        metadata.find(ANDROID_CONTROL_CAPTURE_INTENT);
    switch (entry.data.u8[0]) {
      case ANDROID_CONTROL_CAPTURE_INTENT_VIDEO_RECORD:
      case ANDROID_CONTROL_CAPTURE_INTENT_VIDEO_SNAPSHOT:
        return true;
    }
  }

  if (metadata.exists(ANDROID_COLOR_CORRECTION_ABERRATION_MODE)) {
    camera_metadata_ro_entry entry =
        metadata.find(ANDROID_COLOR_CORRECTION_ABERRATION_MODE);
    switch (entry.data.u8[0]) {
      case ANDROID_COLOR_CORRECTION_ABERRATION_MODE_OFF:
      case ANDROID_COLOR_CORRECTION_ABERRATION_MODE_FAST:
        return true;
    }
  }

  if (metadata.exists(ANDROID_NOISE_REDUCTION_MODE)) {
    camera_metadata_ro_entry entry =
        metadata.find(ANDROID_NOISE_REDUCTION_MODE);
    switch (entry.data.u8[0]) {
      case ANDROID_NOISE_REDUCTION_MODE_OFF:
      case ANDROID_NOISE_REDUCTION_MODE_FAST:
      case ANDROID_NOISE_REDUCTION_MODE_MINIMAL:
        return true;
    }
  }

  return false;
}

int CameraClient::RequestHandler::WriteStreamBuffer(
    int stream_index,
    int num_streams,
    const android::CameraMetadata& metadata,
    const camera3_stream_buffer_t* buffer) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  VLOGFID(1, device_id_) << "output buffer stream format: "
                         << buffer->stream->format
                         << ", buffer ptr: " << *buffer->buffer
                         << ", width: " << buffer->stream->width
                         << ", height: " << buffer->stream->height;

  int ret = 0;

  // Get frame data from device only for the first buffer.
  // We reuse the buffer for all streams.
  if (stream_index == 0) {
    int32_t pattern_mode = ANDROID_SENSOR_TEST_PATTERN_MODE_OFF;
    if (metadata.exists(ANDROID_SENSOR_TEST_PATTERN_MODE)) {
      camera_metadata_ro_entry entry =
          metadata.find(ANDROID_SENSOR_TEST_PATTERN_MODE);
      pattern_mode = entry.data.i32[0];
    }

    do {
      // Add log here to see timestamp log.
      VLOGFID(2, device_id_) << "before DequeueV4L2Buffer";
      ret = DequeueV4L2Buffer(pattern_mode);
    } while (ret == -EAGAIN);
    if (ret) {
      return ret;
    }
  }

  GrallocFrameBuffer output_frame(*buffer->buffer, buffer->stream->width,
                                  buffer->stream->height);

  ret = output_frame.Map();
  if (ret) {
    return -EINVAL;
  }

  // Crop the stream image to the same ratio as the current buffer image.
  // We want to compare w1/h1 and w2/h2. To avoid floating point precision loss
  // we compare w1*h2 and w2*h1 instead, with w1 and h1 being the width and
  // height of the stream; w2 and h2 those of the buffer.
  int buffer_aspect_ratio =
      buffer->stream->width * stream_on_resolution_.height;
  int stream_aspect_ratio =
      stream_on_resolution_.width * buffer->stream->height;
  int crop_width;
  int crop_height;

  // Same Ratio.
  if (stream_aspect_ratio == buffer_aspect_ratio) {
    crop_width = stream_on_resolution_.width;
    crop_height = stream_on_resolution_.height;
  } else if (stream_aspect_ratio > buffer_aspect_ratio) {
    // Need to crop width.
    crop_width = buffer_aspect_ratio / buffer->stream->height;
    crop_height = stream_on_resolution_.height;
  } else {
    // Need to crop height.
    crop_width = stream_on_resolution_.width;
    crop_height = stream_aspect_ratio / buffer->stream->width;
  }

  // Make sure crop size is even.
  crop_width = (crop_width + 1) & (~1);
  crop_height = (crop_height + 1) & (~1);

  ret = input_frame_.Convert(metadata, crop_width, crop_height, &output_frame);
  if (ret) {
    return -EINVAL;
  }

  // Return v4l2 buffer when last stream buffer is handled.
  if (stream_index + 1 == num_streams) {
    ret = EnqueueV4L2Buffer();
    if (ret) {
      return ret;
    }
  }
  return 0;
}

void CameraClient::RequestHandler::SkipFramesAfterStreamOn(int num_frames) {
  for (size_t i = 0; i < num_frames; i++) {
    uint32_t buffer_id, data_size;
    uint64_t timestamp;
    device_->GetNextFrameBuffer(&buffer_id, &data_size, &timestamp);
    device_->ReuseFrameBuffer(buffer_id);
  }
}

bool CameraClient::RequestHandler::WaitGrallocBufferSync(
    camera3_capture_result_t* capture_result) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  // Framework allow 4 intervals delay. If fps is 30, 4 intervals delay is
  // 132ms. Use 300ms should be enough.
  const int kSyncWaitTimeoutMs = 300;
  bool fence_timeout = false;
  for (size_t i = 0; i < capture_result->num_output_buffers; i++) {
    camera3_stream_buffer_t* b = const_cast<camera3_stream_buffer_t*>(
        capture_result->output_buffers + i);
    if (b->acquire_fence == kBufferFenceReady) {
      continue;
    }

    int ret = sync_wait(b->acquire_fence, kSyncWaitTimeoutMs);
    if (ret) {
      // If buffer is not ready, set |release_fence| to notify framework to
      // wait the buffer again.
      b->release_fence = b->acquire_fence;
      LOGFID(ERROR, device_id_) << "Fence sync_wait failed: "
                                << b->acquire_fence;
      fence_timeout = true;
    } else {
      close(b->acquire_fence);
    }

    // HAL has to set |acquire_fence| to -1 for output buffers.
    b->acquire_fence = kBufferFenceReady;
  }
  return !fence_timeout;
}

void CameraClient::RequestHandler::AbortGrallocBufferSync(
    camera3_capture_result_t* capture_result) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  for (size_t i = 0; i < capture_result->num_output_buffers; i++) {
    camera3_stream_buffer_t* b = const_cast<camera3_stream_buffer_t*>(
        capture_result->output_buffers + i);
    b->release_fence = b->acquire_fence;
    b->acquire_fence = kBufferFenceReady;
  }
}

void CameraClient::RequestHandler::NotifyShutter(uint32_t frame_number) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  camera3_notify_msg_t m;
  memset(&m, 0, sizeof(m));
  m.type = CAMERA3_MSG_SHUTTER;
  m.message.shutter.frame_number = frame_number;
  m.message.shutter.timestamp = current_v4l2_buffer_timestamp_;
  callback_ops_->notify(callback_ops_, &m);
}

void CameraClient::RequestHandler::NotifyRequestError(uint32_t frame_number) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  camera3_notify_msg_t m;
  memset(&m, 0, sizeof(m));
  m.type = CAMERA3_MSG_ERROR;
  m.message.error.frame_number = frame_number;
  m.message.error.error_stream = nullptr;
  m.message.error.error_code = CAMERA3_MSG_ERROR_REQUEST;
  callback_ops_->notify(callback_ops_, &m);
}

int CameraClient::RequestHandler::DequeueV4L2Buffer(int32_t pattern_mode) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  uint32_t buffer_id, data_size;
  uint64_t timestamp;
  int ret = device_->GetNextFrameBuffer(&buffer_id, &data_size, &timestamp);
  if (ret) {
    LOGFID(ERROR, device_id_)
        << "GetNextFrameBuffer failed: " << strerror(-ret);
    return ret;
  }
  current_v4l2_buffer_id_ = buffer_id;

  ret = input_buffers_[buffer_id]->SetDataSize(data_size);
  if (ret) {
    LOGFID(ERROR, device_id_)
        << "Set data size failed for input buffer id: " << buffer_id;
    return ret;
  }

  current_v4l2_buffer_timestamp_ = timestamp;

  if (!test_pattern_->SetTestPatternMode(pattern_mode)) {
    return -EINVAL;
  }

  if (!test_pattern_->IsTestPatternEnabled()) {
    ret = input_frame_.SetSource(input_buffers_[buffer_id].get(),
                                 crop_rotate_scale_degrees_, false);
    if (ret) {
      LOGFID(ERROR, device_id_)
          << "Set image source failed for input buffer id: " << buffer_id;
      // Try again when captured frame is not a valid MJPEG.
      return -EAGAIN;
    }
  } else {
    ret = input_frame_.SetSource(test_pattern_->GetTestPattern(),
                                 crop_rotate_scale_degrees_, true);
    if (ret) {
      LOGFID(ERROR, device_id_) << "Set image source failed for test pattern";
      return ret;
    }
  }
  return 0;
}

int CameraClient::RequestHandler::EnqueueV4L2Buffer() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  input_frame_.UnsetSource();
  int ret = device_->ReuseFrameBuffer(current_v4l2_buffer_id_);
  if (ret) {
    LOGFID(ERROR, device_id_)
        << "ReuseFrameBuffer failed: " << strerror(-ret)
        << " for input buffer id: " << current_v4l2_buffer_id_;
  }
  current_v4l2_buffer_id_ = -1;
  return ret;
}

void CameraClient::RequestHandler::FlushDone(
    const base::Callback<void(int)>& callback) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  VLOGFID(1, device_id_);
  callback.Run(0);
  {
    base::AutoLock l(flush_lock_);
    flush_started_ = false;
  }
}

}  // namespace cros
