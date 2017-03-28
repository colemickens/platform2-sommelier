/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "hal/usb/camera_client.h"

#include <vector>

#include <sys/mman.h>
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
    : id_(id),
      device_path_(device_path),
      device_(new V4L2CameraDevice()) {
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

  SupportedFormats supported_formats =
      device_->GetDeviceSupportedFormats(device_path_);
  qualified_formats_ = GetQualifiedFormats(supported_formats);
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

  stream_on_resolution_.width = stream_on_resolution_.height = 0;
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

    // Find maximum resolution of stream_config to stream on.
    if (stream_config->streams[i]->width > stream_on_resolution_.width ||
        stream_config->streams[i]->height > stream_on_resolution_.height) {
      stream_on_resolution_.width = stream_config->streams[i]->width;
      stream_on_resolution_.height = stream_config->streams[i]->height;
    }
  }

  if (!IsValidStreamSet(streams)) {
    LOGFID(ERROR, id_) << "Invalid stream set";
    return -EINVAL;
  }

  int ret = StreamOn();
  if (ret) {
    LOGFID(ERROR, id_) << "StreamOn failed";
    return ret;
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
    stream->max_buffers = buffers_.size();
  }
}

int CameraClient::StreamOn() {
  const SupportedFormat* format =
      FindFormatByResolution(qualified_formats_, stream_on_resolution_.width,
                             stream_on_resolution_.height);
  if (format == NULL) {
    LOGFID(ERROR, id_) << "Cannot find resolution in supported list: width "
                       << stream_on_resolution_.width << ", height "
                       << stream_on_resolution_.height;
    return -EINVAL;
  }

  float max_fps = 0;
  for (const auto& frame_rate : format->frameRates) {
    if (frame_rate > max_fps) {
      max_fps = frame_rate;
    }
  }
  VLOGFID(1, id_) << "streamOn with width " << format->width << ", height "
                  << format->height << ", fps " << max_fps << ", format "
                  << std::hex << format->fourcc;

  std::vector<int> fds;
  uint32_t buffer_size;
  int ret;
  if ((ret = device_->StreamOn(format->width, format->height, format->fourcc,
                               max_fps, &fds, &buffer_size))) {
    LOGFID(ERROR, id_) << "StreamOn failed: " << strerror(-ret);
    return ret;
  }

  FrameBuffer frame;
  frame.data_size = 0;
  frame.buffer_size = buffer_size;
  frame.width = format->width;
  frame.height = format->height;
  frame.fourcc = format->fourcc;

  for (size_t i = 0; i < fds.size(); i++) {
    void* addr = mmap(NULL, buffer_size, PROT_READ, MAP_SHARED, fds[i], 0);
    if (addr == MAP_FAILED) {
      LOGFID(ERROR, id_) << "mmap() (" << i << ") failed: " << strerror(errno);
      StreamOff();
      return -errno;
    }
    frame.fd = fds[i];
    frame.data = static_cast<uint8_t*>(addr);
    VLOGFID(1, id_) << "Buffer " << i << ", fd: " << fds[i]
                    << " address: " << std::hex << addr;
    buffers_.push_back(frame);
  }
  return 0;
}

int CameraClient::StreamOff() {
  int ret;
  if ((ret = device_->StreamOff())) {
    LOGFID(ERROR, id_) << "StreamOff failed: " << strerror(-ret);
  }
  ReleaseBuffers();
  return ret;
}

void CameraClient::ReleaseBuffers() {
  for (auto const& frame : buffers_) {
    if (munmap(frame.data, frame.buffer_size)) {
      LOGFID(ERROR, id_) << "mummap() failed: " << strerror(errno);
    }
    if (close(frame.fd)) {
      LOGFID(ERROR, id_) << "close(" << frame.fd
                         << ") failed: " << strerror(errno);
    }
  }
  buffers_.clear();
}

}  // namespace arc
