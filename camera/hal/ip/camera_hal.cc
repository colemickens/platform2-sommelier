/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "hal/ip/camera_hal.h"
#include "hal/ip/metadata_handler.h"

#include <base/strings/string_number_conversions.h>
#include <stdlib.h>

#include "cros-camera/common.h"
#include "cros-camera/export.h"

namespace cros {

CameraHal::CameraHal()
    : ip_camera_detector_(this),
      camera_map_lock_(),
      cameras_(),
      camera_static_info_(),
      open_cameras_(),
      next_camera_id_(0),
      callbacks_set_(base::WaitableEvent::ResetPolicy::MANUAL,
                     base::WaitableEvent::InitialState::NOT_SIGNALED),
      callbacks_(NULL) {}

CameraHal::~CameraHal() {
  ip_camera_detector_.Stop();

  base::AutoLock l(camera_map_lock_);

  open_cameras_.clear();

  camera_static_info_.clear();
}

CameraHal& CameraHal::GetInstance() {
  static CameraHal camera_hal;
  return camera_hal;
}

int CameraHal::OpenDevice(int id,
                          const hw_module_t* module,
                          hw_device_t** hw_device) {
  base::AutoLock l(camera_map_lock_);

  if (cameras_.find(id) == cameras_.end()) {
    LOGF(ERROR) << "Camera " << id << " is invalid";
    return -EINVAL;
  }

  if (open_cameras_.find(id) != open_cameras_.end()) {
    LOGF(ERROR) << "Camera " << id << " is already open";
    return -EBUSY;
  }

  open_cameras_[id] =
      std::make_unique<CameraDevice>(id, cameras_[id], module, hw_device);

  return 0;
}

int CameraHal::CloseDeviceLocked(int id) {
  if (cameras_.find(id) == cameras_.end()) {
    LOGF(ERROR) << "Camera " << id << " is invalid";
    return -EINVAL;
  }

  if (open_cameras_.find(id) == open_cameras_.end()) {
    LOGF(ERROR) << "Camera " << id << " is not open";
    return -EINVAL;
  }

  open_cameras_.erase(id);

  return 0;
}

int CameraHal::CloseDevice(int id) {
  base::AutoLock l(camera_map_lock_);
  return CloseDeviceLocked(id);
}

int CameraHal::GetNumberOfCameras() const {
  // Should always return 0, only built-in cameras are counted here
  return 0;
}

int CameraHal::GetCameraInfo(int id, struct camera_info* info) {
  base::AutoLock l(camera_map_lock_);
  auto it = camera_static_info_.find(id);
  if (cameras_.find(id) == cameras_.end() || it == camera_static_info_.end()) {
    LOGF(ERROR) << "Camera id " << id << " is not valid";
    return -EINVAL;
  }

  info->facing = CAMERA_FACING_EXTERNAL;
  info->orientation = 0;
  info->device_version = CAMERA_DEVICE_API_VERSION_3_3;
  info->static_camera_characteristics = it->second.getAndLock();
  info->resource_cost = 0;
  info->conflicting_devices = nullptr;
  info->conflicting_devices_length = 0;
  return 0;
}

int CameraHal::SetCallbacks(const camera_module_callbacks_t* callbacks) {
  callbacks_ = callbacks;
  callbacks_set_.Signal();
  return 0;
}

int CameraHal::Init() {
  if (!ip_camera_detector_.Start()) {
    LOGF(ERROR) << "Failed to start IP Camera Detector";
    return -ENODEV;
  }

  return 0;
}

int CameraHal::OnDeviceConnected(IpCameraDevice* device) {
  int camera_id = -1;
  {
    base::AutoLock l(camera_map_lock_);
    camera_id = next_camera_id_;
    next_camera_id_++;
    cameras_[camera_id] = device;
    camera_static_info_[camera_id] = MetadataHandler::CreateStaticMetadata(
        device->GetFormat(), device->GetWidth(), device->GetHeight(),
        device->GetFPS());
  }

  callbacks_set_.Wait();
  callbacks_->camera_device_status_change(callbacks_, camera_id,
                                          CAMERA_DEVICE_STATUS_PRESENT);
  return camera_id;
}

void CameraHal::OnDeviceDisconnected(int id) {
  callbacks_set_.Wait();
  callbacks_->camera_device_status_change(callbacks_, id,
                                          CAMERA_DEVICE_STATUS_NOT_PRESENT);

  base::AutoLock l(camera_map_lock_);
  if (open_cameras_.find(id) != open_cameras_.end()) {
    CloseDeviceLocked(id);
  }

  if (cameras_.erase(id) == 0) {
    LOGF(ERROR) << "Camera " << id << " is invalid";
  }

  if (camera_static_info_.erase(id) == 0) {
    LOGF(ERROR) << "Camera " << id << " has no static info";
  }
}

static int camera_device_open(const hw_module_t* module,
                              const char* name,
                              hw_device_t** device) {
  if (module != &HAL_MODULE_INFO_SYM.common) {
    LOGF(ERROR) << std::hex << std::showbase << "Invalid module " << module
                << " expected " << &HAL_MODULE_INFO_SYM.common;
    return -EINVAL;
  }

  int id;
  if (!base::StringToInt(name, &id)) {
    LOGF(ERROR) << "Invalid camera name " << name;
    return -EINVAL;
  }

  return CameraHal::GetInstance().OpenDevice(id, module, device);
}

static int get_number_of_cameras() {
  return CameraHal::GetInstance().GetNumberOfCameras();
}

static int get_camera_info(int id, struct camera_info* info) {
  return CameraHal::GetInstance().GetCameraInfo(id, info);
}

static int set_callbacks(const camera_module_callbacks_t* callbacks) {
  return CameraHal::GetInstance().SetCallbacks(callbacks);
}

static void get_vendor_tag_ops(vendor_tag_ops_t* /*ops*/) {}

static int open_legacy(const struct hw_module_t* /*module*/,
                       const char* /*id*/,
                       uint32_t /*halVersion*/,
                       struct hw_device_t** /*device*/) {
  return -ENOSYS;
}

static int set_torch_mode(const char* /*camera_id*/, bool /*enabled*/) {
  return -ENOSYS;
}

static int init() {
  return CameraHal::GetInstance().Init();
}

}  // namespace cros

static hw_module_methods_t gCameraModuleMethods = {
    .open = cros::camera_device_open};

camera_module_t HAL_MODULE_INFO_SYM CROS_CAMERA_EXPORT = {
    .common = {.tag = HARDWARE_MODULE_TAG,
               .module_api_version = CAMERA_MODULE_API_VERSION_2_4,
               .hal_api_version = HARDWARE_HAL_API_VERSION,
               .id = CAMERA_HARDWARE_MODULE_ID,
               .name = "IP Camera HAL v3",
               .author = "The Chromium OS Authors",
               .methods = &gCameraModuleMethods,
               .dso = NULL,
               .reserved = {0}},
    .get_number_of_cameras = cros::get_number_of_cameras,
    .get_camera_info = cros::get_camera_info,
    .set_callbacks = cros::set_callbacks,
    .get_vendor_tag_ops = cros::get_vendor_tag_ops,
    .open_legacy = cros::open_legacy,
    .set_torch_mode = cros::set_torch_mode,
    .init = cros::init,
    .reserved = {0}};
