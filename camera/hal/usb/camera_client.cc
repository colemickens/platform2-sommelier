/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "hal/usb/camera_client.h"

#include <vector>

#include <system/camera_metadata.h>

#include "arc/common.h"
#include "hal/usb/camera_hal.h"
#include "hal/usb/camera_hal_device_ops.h"
#include "hal/usb/camera_metadata.h"
#include "hal/usb/stream_format.h"

namespace arc {

CameraClient::CameraClient(int id,
                           const std::string& device_path,
                           const camera_metadata_t& static_info,
                           const hw_module_t* module,
                           hw_device_t** hw_device)
    : id_(id), device_path_(device_path), device_(new V4L2CameraDevice()) {
  memset(&camera3_device_, 0, sizeof(camera3_device_));
  camera3_device_.common.tag = HARDWARE_DEVICE_TAG;
  camera3_device_.common.version = CAMERA_DEVICE_API_VERSION_3_3;
  camera3_device_.common.close = arc::camera_device_close;
  camera3_device_.common.module = const_cast<hw_module_t*>(module);
  camera3_device_.ops = &g_camera_device_ops;
  camera3_device_.priv = this;
  *hw_device = &camera3_device_.common;

  ops_thread_checker_.DetachFromThread();

  // MetadataBase::operator= will make a copy of camera_metadata_t.
  metadata_ = &static_info;
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
  DCHECK(thread_checker_.CalledOnValidThread());

  device_->Disconnect();
  return 0;
}

int CameraClient::Initialize(const camera3_callback_ops_t* callback_ops) {
  VLOGFID(1, id_);
  DCHECK(ops_thread_checker_.CalledOnValidThread());

  callback_ops_ = callback_ops;

  // camera3_request_template_t starts at 1.
  for (int i = 1; i < CAMERA3_TEMPLATE_COUNT; i++) {
    template_settings_[i] = metadata_.CreateDefaultRequestSettings(i);
    if (template_settings_[i].get() == NULL) {
      LOGFID(ERROR, id_) << "metadata for template type (" << i << ") is NULL";
      return -ENODATA;
    }
  }
  return 0;
}

int CameraClient::ConfigureStreams(
    camera3_stream_configuration_t* stream_config) {
  VLOGFID(1, id_);
  DCHECK(ops_thread_checker_.CalledOnValidThread());
  /* TODO(henryhsu):
   * 1. Check there are no in-flight requests.
   * 2. Check configure_streams is called after initialize.
   * 3. Check format, width, and height are supported.
   */

  if (stream_config == NULL) {
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
  }

  if (!IsValidStreamSet(streams)) {
    LOGFID(ERROR, id_) << "Invalid stream set";
    return -EINVAL;
  }

  SetUpStreams(&streams);

  return 0;
}

const camera_metadata_t* CameraClient::ConstructDefaultRequestSettings(
    int type) {
  VLOGFID(1, id_) << "type=" << type;
  DCHECK(ops_thread_checker_.CalledOnValidThread());

  if (!CameraMetadata::IsValidTemplateType(type)) {
    LOGFID(ERROR, id_) << "Invalid template request type: " << type;
    return NULL;
  }
  return template_settings_[type].get();
}

int CameraClient::ProcessCaptureRequest(camera3_capture_request_t* request) {
  VLOGFID(1, id_);
  DCHECK(ops_thread_checker_.CalledOnValidThread());
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

void CameraClient::SetUpStreams(std::vector<camera3_stream_t*>* streams) {
  for (auto& stream : *streams) {
    uint32_t usage = 0;
    if (stream->stream_type == CAMERA3_STREAM_OUTPUT ||
        stream->stream_type == CAMERA3_STREAM_BIDIRECTIONAL) {
      usage |= GRALLOC_USAGE_SW_WRITE_OFTEN;
    }
    stream->usage = usage;
    // TODO(henryhsu): get correct number of buffers.
    stream->max_buffers = 0;
  }
}

}  // namespace arc
