/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <base/memory/shared_memory.h>
#include <memory>
#include <mojo/edk/embedder/embedder.h>
#include <utility>

#include "cros-camera/common.h"
#include "cros-camera/ipc_util.h"
#include "hal/ip/camera_device.h"
#include "hal/ip/camera_hal.h"
#include "hal/ip/metadata_handler.h"

namespace cros {

static int initialize(const camera3_device_t* dev,
                      const camera3_callback_ops_t* callback_ops) {
  CameraDevice* device = reinterpret_cast<CameraDevice*>(dev->priv);
  if (!device) {
    LOGF(ERROR) << "Camera device is NULL";
    return -ENODEV;
  }
  return device->Initialize(callback_ops);
}

static int configure_streams(const camera3_device_t* dev,
                             camera3_stream_configuration_t* stream_list) {
  CameraDevice* device = reinterpret_cast<CameraDevice*>(dev->priv);
  if (!device) {
    LOGF(ERROR) << "Camera device is NULL";
    return -ENODEV;
  }
  return device->ConfigureStreams(stream_list);
}

static const camera_metadata_t* construct_default_request_settings(
    const camera3_device_t* dev, int type) {
  CameraDevice* device = reinterpret_cast<CameraDevice*>(dev->priv);
  if (!device) {
    LOGF(ERROR) << "Camera device is NULL";
    return nullptr;
  }
  return device->ConstructDefaultRequestSettings(type);
}

static int process_capture_request(const camera3_device_t* dev,
                                   camera3_capture_request_t* request) {
  CameraDevice* device = reinterpret_cast<CameraDevice*>(dev->priv);
  if (!device) {
    LOGF(ERROR) << "Camera device is NULL";
    return -ENODEV;
  }
  return device->ProcessCaptureRequest(request);
}

static void dump(const camera3_device_t* dev, int fd) {}

static int flush(const camera3_device_t* dev) {
  CameraDevice* device = reinterpret_cast<CameraDevice*>(dev->priv);
  if (!device) {
    LOGF(ERROR) << "Camera device is NULL";
    return -ENODEV;
  }
  return device->Flush();
}

}  // namespace cros

static camera3_device_ops_t g_camera_device_ops = {
    .initialize = cros::initialize,
    .configure_streams = cros::configure_streams,
    .register_stream_buffers = nullptr,
    .construct_default_request_settings =
        cros::construct_default_request_settings,
    .process_capture_request = cros::process_capture_request,
    .get_metadata_vendor_tag_ops = nullptr,
    .dump = cros::dump,
    .flush = cros::flush,
    .reserved = {},
};

namespace cros {

static int camera_device_close(struct hw_device_t* hw_device) {
  camera3_device_t* dev = reinterpret_cast<camera3_device_t*>(hw_device);
  CameraDevice* device = static_cast<CameraDevice*>(dev->priv);
  if (!device) {
    LOGF(ERROR) << "Camera device is NULL";
    return -EIO;
  }

  device->Close();
  return 0;
}

CameraDevice::CameraDevice(int id)
    : open_(false),
      id_(id),
      camera3_device_(),
      callback_ops_(nullptr),
      format_(0),
      width_(0),
      height_(0),
      binding_(this),
      buffer_manager_(nullptr) {
  memset(&camera3_device_, 0, sizeof(camera3_device_));
  camera3_device_.common.tag = HARDWARE_DEVICE_TAG;
  camera3_device_.common.version = CAMERA_DEVICE_API_VERSION_3_3;
  camera3_device_.common.close = cros::camera_device_close;
  camera3_device_.common.module = nullptr;
  camera3_device_.ops = &g_camera_device_ops;
  camera3_device_.priv = this;

  buffer_manager_ = CameraBufferManager::GetInstance();
}

int CameraDevice::Init(mojom::IpCameraDevicePtr ip_device,
                       mojom::PixelFormat format,
                       int32_t width,
                       int32_t height,
                       double fps) {
  ipc_task_runner_ = mojo::edk::GetIOTaskRunner();
  DCHECK(ipc_task_runner_->RunsTasksOnCurrentThread());

  ip_device_ = std::move(ip_device);
  width_ = width;
  height_ = height;

  switch (format) {
    case mojom::PixelFormat::YUV_420:
      format_ = HAL_PIXEL_FORMAT_YCbCr_420_888;
      break;
    default:
      LOGF(ERROR) << "Unrecognized pixel format: " << format;
      return -EINVAL;
  }

  static_metadata_ =
      MetadataHandler::CreateStaticMetadata(format_, width_, height_, fps);

  mojom::IpCameraFrameListenerPtr listener;
  binding_.Bind(mojo::MakeRequest(&listener));
  ip_device_->RegisterFrameListener(std::move(listener));

  return 0;
}

void CameraDevice::Open(const hw_module_t* module, hw_device_t** hw_device) {
  camera3_device_.common.module = const_cast<hw_module_t*>(module);
  *hw_device = &camera3_device_.common;
  open_ = true;
}

CameraDevice::~CameraDevice() {
  DCHECK(ipc_task_runner_->RunsTasksOnCurrentThread());

  ip_device_.reset();
  binding_.Close();
}

bool CameraDevice::IsOpen() {
  return open_;
}

android::CameraMetadata* CameraDevice::GetStaticMetadata() {
  return &static_metadata_;
}

int CameraDevice::Initialize(const camera3_callback_ops_t* callback_ops) {
  callback_ops_ = callback_ops;
  request_queue_.SetCallbacks(callback_ops_);

  return 0;
}

void CameraDevice::Close() {
  DCHECK(!ipc_task_runner_->RunsTasksOnCurrentThread());
  open_ = false;
  request_queue_.Flush();

  auto return_val = Future<void>::Create(nullptr);
  ipc_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&CameraDevice::StopStreamingOnIpcThread,
                                base::Unretained(this), return_val));
  return_val->Wait(-1);
}

void CameraDevice::StopStreamingOnIpcThread(
    scoped_refptr<Future<void>> return_val) {
  DCHECK(ipc_task_runner_->RunsTasksOnCurrentThread());
  ip_device_->StopStreaming();
  return_val->Set();
}

bool CameraDevice::ValidateStream(camera3_stream_t* stream) {
  if (!stream) {
    LOGFID(ERROR, id_) << "NULL stream";
    return false;
  }

  if (stream->stream_type != CAMERA3_STREAM_OUTPUT) {
    LOGFID(ERROR, id_) << "Unsupported stream type: " << stream->stream_type;
    return false;
  }

  if (stream->width != width_) {
    LOGFID(ERROR, id_) << "Unsupported stream width: " << stream->width;
    return false;
  }

  if (stream->height != height_) {
    LOGFID(ERROR, id_) << "Unsupported stream height: " << stream->height;
    return false;
  }

  if (stream->format != format_) {
    LOGFID(ERROR, id_) << "Unsupported stream format: " << stream->format;
    return false;
  }

  if (stream->rotation != CAMERA3_STREAM_ROTATION_0) {
    LOGFID(ERROR, id_) << "Unsupported stream rotation: " << stream->rotation;
    return false;
  }

  return true;
}

int CameraDevice::ConfigureStreams(
    camera3_stream_configuration_t* stream_list) {
  DCHECK(!ipc_task_runner_->RunsTasksOnCurrentThread());

  if (callback_ops_ == nullptr) {
    LOGFID(ERROR, id_) << "Device is not initialized";
    return -EINVAL;
  }

  if (stream_list == nullptr) {
    LOGFID(ERROR, id_) << "Null stream list array";
    return -EINVAL;
  }

  if (stream_list->num_streams != 1) {
    LOGFID(ERROR, id_) << "Unsupported number of streams: "
                       << stream_list->num_streams;
    return -EINVAL;
  }

  if (stream_list->operation_mode != CAMERA3_STREAM_CONFIGURATION_NORMAL_MODE) {
    LOGFID(ERROR, id_) << "Unsupported operation mode: "
                       << stream_list->operation_mode;
    return -EINVAL;
  }

  if (!ValidateStream(stream_list->streams[0])) {
    return -EINVAL;
  }

  // TODO(pceballos): revisit these two values, the number of buffers may need
  // to be adjusted by each different device
  stream_list->streams[0]->usage |= GRALLOC_USAGE_SW_WRITE_OFTEN;
  stream_list->streams[0]->max_buffers = 4;

  auto return_val = Future<void>::Create(nullptr);
  ipc_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&CameraDevice::StartStreamingOnIpcThread,
                                base::Unretained(this), return_val));

  return_val->Wait(-1);
  return 0;
}

void CameraDevice::StartStreamingOnIpcThread(
    scoped_refptr<Future<void>> return_val) {
  DCHECK(ipc_task_runner_->RunsTasksOnCurrentThread());
  ip_device_->StartStreaming();
  return_val->Set();
}

const camera_metadata_t* CameraDevice::ConstructDefaultRequestSettings(
    int type) {
  if (type != CAMERA3_TEMPLATE_PREVIEW) {
    LOGFID(ERROR, id_) << "Unsupported request template:" << type;
    return nullptr;
  }
  return MetadataHandler::GetDefaultRequestSettings();
}

int CameraDevice::ProcessCaptureRequest(camera3_capture_request_t* request) {
  if (!request) {
    LOGFID(ERROR, id_) << "Received a NULL request";
    return -EINVAL;
  }

  if (request->input_buffer) {
    LOGFID(ERROR, id_) << "Input buffers are not supported";
    return -EINVAL;
  }

  if (request->num_output_buffers != 1) {
    LOGFID(ERROR, id_) << "Invalid number of output buffers: "
                       << request->num_output_buffers;
    return -EINVAL;
  }

  const camera3_stream_buffer_t* buffer = request->output_buffers;

  if (!ValidateStream(buffer->stream)) {
    return -EINVAL;
  }

  request_queue_.Push(request);

  return 0;
}

int CameraDevice::Flush() {
  request_queue_.Flush();
  return 0;
}

void CameraDevice::OnFrameCaptured(mojo::ScopedHandle shm_handle,
                                   uint32_t size) {
  if (request_queue_.IsEmpty()) {
    return;
  }

  int fd = UnwrapPlatformHandle(std::move(shm_handle));
  base::SharedMemory shm(base::FileDescriptor(fd, true), true);
  if (!shm.Map(size)) {
    LOGFID(ERROR, id_) << "Error mapping shm, unable to handle captured frame";
    return;
  }

  std::unique_ptr<CaptureRequest> request = request_queue_.Pop();
  buffer_handle_t* output_buffer = request->GetOutputBuffer()->buffer;
  buffer_manager_->Register(*output_buffer);
  struct android_ycbcr ycbcr;
  buffer_manager_->LockYCbCr(*output_buffer, 0, 0, 0, width_, height_, &ycbcr);

  // Convert from I420 to NV12 while copying the buffer since the buffer manager
  // allocates an NV12 buffer
  // TODO(pceballos): support other formats, this only works for YUV 4:2:0
  // format
  memcpy(ycbcr.y, shm.memory(), width_ * height_);

  char* u = reinterpret_cast<char*>(shm.memory()) + width_ * height_;
  char* v = reinterpret_cast<char*>(shm.memory()) + width_ * height_ * 5 / 4;
  char* uv = reinterpret_cast<char*>(ycbcr.cb);

  for (uint32_t i = 0; i < width_ * height_ / 4; i++) {
    *uv = u[i];
    uv++;
    *uv = v[i];
    uv++;
  }

  buffer_manager_->Unlock(*output_buffer);
  buffer_manager_->Deregister(*output_buffer);

  shm.Unmap();
  shm.Close();
  close(fd);

  request_queue_.NotifyCapture(std::move(request));
}

}  // namespace cros
