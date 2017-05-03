/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "hal/usb/camera_client.h"

#include <utility>
#include <vector>

#include <sync/sync.h>
#include <system/camera_metadata.h>

#include "arc/common.h"
#include "hal/usb/cached_frame.h"
#include "hal/usb/camera_hal.h"
#include "hal/usb/camera_hal_device_ops.h"
#include "hal/usb/stream_format.h"

namespace arc {

const int kBufferFenceReady = -1;

CameraClient::CameraClient(int id,
                           const std::string& device_path,
                           const camera_metadata_t& static_info,
                           const hw_module_t* module,
                           hw_device_t** hw_device)
    : id_(id),
      device_path_(device_path),
      device_(new V4L2CameraDevice()),
      metadata_handler_(new MetadataHandler(static_info)),
      request_thread_("Capture request thread") {
  memset(&camera3_device_, 0, sizeof(camera3_device_));
  camera3_device_.common.tag = HARDWARE_DEVICE_TAG;
  camera3_device_.common.version = CAMERA_DEVICE_API_VERSION_3_3;
  camera3_device_.common.close = arc::camera_device_close;
  camera3_device_.common.module = const_cast<hw_module_t*>(module);
  camera3_device_.ops = &g_camera_device_ops;
  camera3_device_.priv = this;
  *hw_device = &camera3_device_.common;

  ops_thread_checker_.DetachFromThread();
}

CameraClient::~CameraClient() {}

int CameraClient::OpenDevice() {
  VLOGFID(1, id_);
  DCHECK(thread_checker_.CalledOnValidThread());

  int ret = device_->Connect(device_path_);
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
   * 2. Check configure_streams is called after initialize.
   * 3. Check format, width, and height are supported.
   */

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

  SupportedFormat stream_on_resolution;
  stream_on_resolution.width = stream_on_resolution.height = 0;
  std::vector<camera3_stream_t*> streams;
  for (size_t i = 0; i < stream_config->num_streams; i++) {
    VLOGFID(1, id_) << "Stream[" << i
                    << "] type=" << stream_config->streams[i]->stream_type
                    << " width=" << stream_config->streams[i]->width
                    << " height=" << stream_config->streams[i]->height
                    << " rotation=" << stream_config->streams[i]->rotation
                    << " format=0x" << std::hex
                    << stream_config->streams[i]->format;
    streams.push_back(stream_config->streams[i]);

    // Skip BLOB format to avoid to use too large resolution as preview size.
    if (stream_config->streams[i]->format == HAL_PIXEL_FORMAT_BLOB)
      continue;

    // Find maximum resolution of stream_config to stream on.
    if (stream_config->streams[i]->width > stream_on_resolution.width ||
        stream_config->streams[i]->height > stream_on_resolution.height) {
      stream_on_resolution.width = stream_config->streams[i]->width;
      stream_on_resolution.height = stream_config->streams[i]->height;
    }
  }

  if (!IsValidStreamSet(streams)) {
    LOGFID(ERROR, id_) << "Invalid stream set";
    return -EINVAL;
  }

  int num_buffers;
  int ret = StreamOn(stream_on_resolution, &num_buffers);
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
  DCHECK(ops_thread_checker_.CalledOnValidThread());

  return metadata_handler_->GetDefaultRequestSettings(type);
}

int CameraClient::ProcessCaptureRequest(camera3_capture_request_t* request) {
  VLOGFID(1, id_);
  DCHECK(ops_thread_checker_.CalledOnValidThread());

  if (!request_handler_.get()) {
    LOG(INFO) << "Request handler has stopped; ignoring request";
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

  // We cannot use |request| after this function returns. So we have to copy
  // necessary information out to |capture_request|.
  std::unique_ptr<CaptureRequest> capture_request(new CaptureRequest(*request));
  request_task_runner_->PostTask(
      FROM_HERE, base::Bind(&CameraClient::RequestHandler::HandleRequest,
                            base::Unretained(request_handler_.get()),
                            base::Passed(&capture_request)));
  return 0;
}

void CameraClient::Dump(int fd) {
  VLOGFID(1, id_);
  DCHECK(ops_thread_checker_.CalledOnValidThread());
}

int CameraClient::Flush(const camera3_device_t* dev) {
  VLOGFID(1, id_);
  DCHECK(ops_thread_checker_.CalledOnValidThread());
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
      usage |= GRALLOC_USAGE_SW_WRITE_OFTEN;
    }
    stream->usage = usage;
    stream->max_buffers = num_buffers;
  }
}

int CameraClient::StreamOn(SupportedFormat stream_on_resolution,
                           int* num_buffers) {
  DCHECK(ops_thread_checker_.CalledOnValidThread());

  if (!request_thread_.Start()) {
    LOG(ERROR) << "Request thread failed to start";
    return -EINVAL;
  }
  request_task_runner_ = request_thread_.task_runner();

  request_handler_.reset(new RequestHandler(id_, device_path_, device_.get(),
                                            callback_ops_, request_task_runner_,
                                            metadata_handler_.get()));

  auto future = internal::Future<int>::Create(nullptr);
  base::Callback<void(int, int)> streamon_callback =
      base::Bind(&CameraClient::StreamOnCallback, base::Unretained(this),
                 base::RetainedRef(future), num_buffers);
  request_task_runner_->PostTask(
      FROM_HERE, base::Bind(&CameraClient::RequestHandler::StreamOn,
                            base::Unretained(request_handler_.get()),
                            stream_on_resolution, streamon_callback));
  return future->Get();
}

void CameraClient::StreamOff() {
  DCHECK(ops_thread_checker_.CalledOnValidThread());

  auto future = internal::Future<int>::Create(nullptr);
  base::Callback<void(int)> streamoff_callback =
      base::Bind(&CameraClient::StreamOffCallback, base::Unretained(this),
                 base::RetainedRef(future));
  request_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&CameraClient::RequestHandler::StreamOff,
                 base::Unretained(request_handler_.get()), streamoff_callback));
  int ret = future->Get();
  if (ret) {
    LOGFID(ERROR, id_) << "StreamOff failed";
  }
  request_thread_.Stop();
  request_handler_.reset();
}

void CameraClient::StreamOnCallback(scoped_refptr<internal::Future<int>> future,
                                    int* out_num_buffers,
                                    int num_buffers,
                                    int result) {
  if (!result && out_num_buffers) {
    *out_num_buffers = num_buffers;
  }
  future->Set(std::move(result));
}

void CameraClient::StreamOffCallback(
    scoped_refptr<internal::Future<int>> future,
    int result) {
  future->Set(std::move(result));
}

CameraClient::RequestHandler::RequestHandler(
    const int device_id,
    const std::string& device_path,
    V4L2CameraDevice* device,
    const camera3_callback_ops_t* callback_ops,
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    MetadataHandler* metadata_handler)
    : device_id_(device_id),
      device_path_(device_path),
      device_(device),
      callback_ops_(callback_ops),
      task_runner_(task_runner),
      metadata_handler_(metadata_handler) {
  SupportedFormats supported_formats =
      device_->GetDeviceSupportedFormats(device_path_);
  qualified_formats_ = GetQualifiedFormats(supported_formats);
}

CameraClient::RequestHandler::~RequestHandler() {}

void CameraClient::RequestHandler::StreamOn(
    SupportedFormat stream_on_resolution,
    const base::Callback<void(int, int)>& callback) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  const SupportedFormat* format =
      FindFormatByResolution(qualified_formats_, stream_on_resolution.width,
                             stream_on_resolution.height);
  if (format == nullptr) {
    LOGFID(ERROR, device_id_)
        << "Cannot find resolution in supported list: width "
        << stream_on_resolution.width << ", height "
        << stream_on_resolution.height;
    callback.Run(0, -EINVAL);
    return;
  }

  float max_fps = 0;
  for (const auto& frame_rate : format->frameRates) {
    if (frame_rate > max_fps) {
      max_fps = frame_rate;
    }
  }
  VLOGFID(1, device_id_) << "streamOn with width " << format->width
                         << ", height " << format->height << ", fps " << max_fps
                         << ", format " << FormatToString(format->fourcc);

  std::vector<base::ScopedFD> fds;
  uint32_t buffer_size;
  int ret = device_->StreamOn(format->width, format->height, format->fourcc,
                              max_fps, &fds, &buffer_size);
  if (ret) {
    LOGFID(ERROR, device_id_) << "StreamOn failed: " << strerror(-ret);
    callback.Run(0, ret);
    return;
  }

  for (size_t i = 0; i < fds.size(); i++) {
    std::unique_ptr<V4L2FrameBuffer> frame(
        new V4L2FrameBuffer(std::move(fds[i]), buffer_size, format->width,
                            format->height, format->fourcc));
    ret = frame->Map();
    if (ret) {
      callback.Run(0, -errno);
      return;
    }
    VLOGFID(1, device_id_) << "Buffer " << i << ", fd: " << frame->GetFd()
                           << " address: " << std::hex
                           << reinterpret_cast<uintptr_t>(frame->GetData());
    input_buffers_.push_back(std::move(frame));
  }
  callback.Run(fds.size(), 0);
}

void CameraClient::RequestHandler::StreamOff(
    const base::Callback<void(int)>& callback) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  int ret = device_->StreamOff();
  if (ret) {
    LOGFID(ERROR, device_id_) << "StreamOff failed: " << strerror(-ret);
  }
  callback.Run(ret);
}

void CameraClient::RequestHandler::HandleRequest(
    std::unique_ptr<CaptureRequest> request) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  // TODO(henryhsu): We still have to call notify when error happened. We also
  // have to call process_capture_result and set CAMERA3_BUFFER_STATUS_ERROR
  // when there is a buffer error.

  camera3_capture_result_t capture_result;
  memset(&capture_result, 0, sizeof(camera3_capture_result_t));

  capture_result.frame_number = request->GetFrameNumber();
  capture_result.partial_result = 1;

  std::vector<camera3_stream_buffer_t>* output_stream_buffers =
      request->GetStreamBuffers();
  capture_result.num_output_buffers = output_stream_buffers->size();
  capture_result.output_buffers = &(*output_stream_buffers)[0];

  int ret = WaitGrallocBufferSync(&capture_result);
  if (ret) {
    LOGFID(ERROR, device_id_) << "Fence sync failed: " << strerror(-ret);
    return;
  }

  uint32_t buffer_id, data_size;
  ret = device_->GetNextFrameBuffer(&buffer_id, &data_size);
  if (ret) {
    LOGFID(ERROR, device_id_) << "GetNextFrameBuffer failed: "
                              << strerror(-ret);
    return;
  }

  ret = input_buffers_[buffer_id]->SetDataSize(data_size);
  if (ret) {
    LOGFID(ERROR, device_id_) << "Set data size failed for input buffer id: "
                              << buffer_id;
    return;
  }
  input_frame_.SetSource(input_buffers_[buffer_id].get(), 0);

  VLOGFID(1, device_id_) << "Request Frame:" << capture_result.frame_number
                         << ", Number of output buffers: "
                         << capture_result.num_output_buffers;

  CameraMetadata* metadata = request->GetMetadata();
  // Handle each stream output buffer and convert it to corresponding format.
  for (size_t i = 0; i < capture_result.num_output_buffers; i++) {
    ret = WriteStreamBuffer(*metadata, const_cast<camera3_stream_buffer_t*>(
                                            &capture_result.output_buffers[i]));
    if (ret) {
      LOGFID(ERROR, device_id_)
          << "Handle stream buffer failed for output buffer id: " << i;
      return;
    }
  }

  int64_t timestamp;
  NotifyShutter(capture_result.frame_number, &timestamp);
  metadata_handler_->PostHandleRequest(timestamp, metadata);
  capture_result.result = metadata->Release();

  // After process_capture_result, HAL cannot access the output buffer in
  // camera3_stream_buffer anymore unless the release fence is not -1.
  callback_ops_->process_capture_result(callback_ops_, &capture_result);
  ret = device_->ReuseFrameBuffer(buffer_id);
  if (ret) {
    LOGFID(ERROR, device_id_) << "ReuseFrameBuffer failed: " << strerror(-ret)
                              << " for input buffer id: " << buffer_id;
  }
}

int CameraClient::RequestHandler::WriteStreamBuffer(
    const CameraMetadata& metadata,
    camera3_stream_buffer_t* buffer) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  VLOGFID(1, device_id_) << "output buffer stream format: "
                         << buffer->stream->format
                         << ", buffer ptr: " << *buffer->buffer
                         << ", width: " << buffer->stream->width
                         << ", height: " << buffer->stream->height;

  int fourcc = HalPixelFormatToFourcc(buffer->stream->format);
  GrallocFrameBuffer output_frame(*buffer->buffer, buffer->stream->width,
                                  buffer->stream->height, fourcc);

  int ret = output_frame.Map();
  if (ret) {
    return -EINVAL;
  }
  input_frame_.Convert(metadata, &output_frame);
  return 0;
}

int CameraClient::RequestHandler::WaitGrallocBufferSync(
    camera3_capture_result_t* capture_result) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  // Framework allow 4 intervals delay. If fps is 30, 4 intervals delay is
  // 132ms. Use 300ms should be enough.
  const int kSyncWaitTimeoutMs = 300;
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
    } else {
      close(b->acquire_fence);
    }

    // HAL has to set |acquire_fence| to -1 for output buffers.
    b->acquire_fence = kBufferFenceReady;

    if (ret) {
      // TODO(henryhsu): Check if we need to continue to wait for the next
      // buffer.
      break;
    }
  }
  return 0;
}

void CameraClient::RequestHandler::NotifyShutter(uint32_t frame_number,
                                                 int64_t* timestamp) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  int res;
  struct timespec ts;

  // TODO(henryhsu): Check we can use the timestamp from v4l2_buffer or not.
  res = clock_gettime(CLOCK_MONOTONIC, &ts);
  if (res == 0) {
    *timestamp = ts.tv_sec * 1000000000ULL + ts.tv_nsec;
  } else {
    LOGFID(ERROR, device_id_)
        << "No timestamp and failed to get CLOCK_MONOTONIC: "
        << strerror(errno);
    *timestamp = 0;
  }
  camera3_notify_msg_t m;
  memset(&m, 0, sizeof(m));
  m.type = CAMERA3_MSG_SHUTTER;
  m.message.shutter.frame_number = frame_number;
  m.message.shutter.timestamp = *timestamp;
  callback_ops_->notify(callback_ops_, &m);
}

}  // namespace arc
