/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "hal/ip/camera_device.h"
#include "hal/ip/camera_hal.h"
#include "hal/ip/metadata_handler.h"

#include "cros-camera/common.h"

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
    return NULL;
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
    .register_stream_buffers = NULL,
    .construct_default_request_settings =
        cros::construct_default_request_settings,
    .process_capture_request = cros::process_capture_request,
    .get_metadata_vendor_tag_ops = NULL,
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

  dev->priv = NULL;
  device->Close();
  return CameraHal::GetInstance().CloseDevice(device->GetId());
}

CameraDevice::CameraDevice(int id,
                           IpCameraDevice* ip_device,
                           const hw_module_t* module,
                           hw_device_t** hw_device)
    : id_(id),
      ip_device_(ip_device),
      camera3_device_(),
      callback_ops_(NULL),
      request_queue_(NULL) {
  memset(&camera3_device_, 0, sizeof(camera3_device_));
  camera3_device_.common.tag = HARDWARE_DEVICE_TAG;
  camera3_device_.common.version = CAMERA_DEVICE_API_VERSION_3_3;
  camera3_device_.common.close = cros::camera_device_close;
  camera3_device_.common.module = const_cast<hw_module_t*>(module);
  camera3_device_.ops = &g_camera_device_ops;
  camera3_device_.priv = this;
  *hw_device = &camera3_device_.common;
}

CameraDevice::~CameraDevice() {}

int CameraDevice::GetId() const {
  return id_;
}

int CameraDevice::Initialize(const camera3_callback_ops_t* callback_ops) {
  callback_ops_ = callback_ops;
  return 0;
}

void CameraDevice::Close() {
  ip_device_->StopStreaming();
  if (request_queue_) {
    request_queue_->Flush();
  }
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

  if (stream->width != ip_device_->GetWidth()) {
    LOGFID(ERROR, id_) << "Unsupported stream width: " << stream->width;
    return false;
  }

  if (stream->height != ip_device_->GetHeight()) {
    LOGFID(ERROR, id_) << "Unsupported stream height: " << stream->height;
    return false;
  }

  if (stream->format != ip_device_->GetFormat()) {
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
  if (callback_ops_ == NULL) {
    LOGFID(ERROR, id_) << "Device is not initialized";
    return -EINVAL;
  }

  if (stream_list == NULL) {
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

  if (!request_queue_) {
    request_queue_ = ip_device_->StartStreaming();
    request_queue_->SetCallbacks(callback_ops_);
  }

  return 0;
}

const camera_metadata_t* CameraDevice::ConstructDefaultRequestSettings(
    int type) {
  if (type != CAMERA3_TEMPLATE_PREVIEW) {
    LOGFID(ERROR, id_) << "Unsupported request template:" << type;
    return NULL;
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

  request_queue_->Push(request);

  return 0;
}

int CameraDevice::Flush() {
  request_queue_->Flush();
  return 0;
}

}  // namespace cros
