/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "usb/camera_client.h"

#include <system/camera_metadata.h>

#include "usb/camera_hal.h"
#include "usb/camera_hal_device_ops.h"
#include "usb/common.h"

namespace arc {

CameraClient::CameraClient(int id,
                           const std::string& device_path,
                           const hw_module_t* module,
                           hw_device_t** hw_device)
    : id_(id), device_path_(device_path) {
  memset(&device_, 0, sizeof(device_));
  device_.common.tag = HARDWARE_DEVICE_TAG;
  device_.common.version = CAMERA_DEVICE_API_VERSION_3_3;
  device_.common.close = arc::camera_device_close;
  device_.common.module = const_cast<hw_module_t*>(module);
  device_.ops = &g_camera_device_ops;
  device_.priv = this;
  *hw_device = &device_.common;

  ops_thread_checker_.DetachFromThread();
}

CameraClient::~CameraClient() {}

int CameraClient::OpenDevice() {
  VLOGFID(1, id_);
  DCHECK(thread_checker_.CalledOnValidThread());
  return 0;
}

int CameraClient::CloseDevice() {
  VLOGFID(1, id_);
  DCHECK(thread_checker_.CalledOnValidThread());
  return 0;
}

int CameraClient::Initialize(const camera3_callback_ops_t* callback_ops) {
  VLOGFID(1, id_);
  DCHECK(ops_thread_checker_.CalledOnValidThread());
  return 0;
}

int CameraClient::ConfigureStreams(
    camera3_stream_configuration_t* stream_config) {
  VLOGFID(1, id_);
  DCHECK(ops_thread_checker_.CalledOnValidThread());
  return 0;
}

const camera_metadata_t* CameraClient::ConstructDefaultRequestSettings(
    int type) {
  VLOGFID(1, id_);
  DCHECK(ops_thread_checker_.CalledOnValidThread());
  return NULL;
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

}  // namespace arc
