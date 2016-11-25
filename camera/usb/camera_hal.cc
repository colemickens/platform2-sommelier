/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "usb/camera_hal.h"

#include <base/lazy_instance.h>

#include "arc/common.h"
#include "usb/common_types.h"
#include "usb/v4l2_camera_device.h"

namespace arc {

base::LazyInstance<CameraHal> g_camera_hal = LAZY_INSTANCE_INITIALIZER;

CameraHal::CameraHal() {
  device_infos_ = V4L2CameraDevice().GetCameraDeviceInfos();
  VLOGF(1) << "Number of cameras is " << GetNumberOfCameras();
}

CameraHal::~CameraHal() {}

CameraHal& CameraHal::GetInstance() {
  return g_camera_hal.Get();
}

int CameraHal::OpenDevice(int id,
                          const hw_module_t* module,
                          hw_device_t** hw_device) {
  VLOGFID(1, id);
  DCHECK(thread_checker_.CalledOnValidThread());
  if (id < 0 || id >= GetNumberOfCameras()) {
    LOGF(ERROR) << "Camera id " << id << " is out of bounds [0,"
                << GetNumberOfCameras() - 1 << "]";
    return -EINVAL;
  }

  if (cameras_.find(id) != cameras_.end()) {
    LOGF(ERROR) << "Camera " << id << " is already opened";
    return -EBUSY;
  }
  cameras_[id].reset(
      new CameraClient(id, device_infos_[id].device_path, module, hw_device));
  if (cameras_[id]->OpenDevice()) {
    cameras_.erase(id);
    return -ENODEV;
  }
  return 0;
}

int CameraHal::GetCameraInfo(int id, struct camera_info* info) {
  VLOGFID(1, id);
  DCHECK(thread_checker_.CalledOnValidThread());
  if (id < 0 || id >= GetNumberOfCameras()) {
    LOGF(ERROR) << "Camera id " << id << " is out of bounds [0,"
                << GetNumberOfCameras() - 1 << "]";
    return -EINVAL;
  }

  info->facing = device_infos_[id].lens_facing;
  info->orientation = device_infos_[id].sensor_orientation;
  info->device_version = CAMERA_DEVICE_API_VERSION_3_3;
  info->static_camera_characteristics = NULL;
  return 0;
}

int CameraHal::SetCallbacks(const camera_module_callbacks_t* callbacks) {
  VLOGF(1) << "New callbacks = " << callbacks;
  DCHECK(thread_checker_.CalledOnValidThread());
  callbacks_ = callbacks;
  return 0;
}

int CameraHal::CloseDevice(int id) {
  VLOGFID(1, id);
  DCHECK(thread_checker_.CalledOnValidThread());

  if (cameras_.find(id) == cameras_.end()) {
    LOGF(ERROR) << "Failed to close camera device " << id
                << ": device is not opened";
    return -EINVAL;
  }
  int ret = cameras_[id]->CloseDevice();
  cameras_.erase(id);
  return ret;
}

static int camera_device_open(const hw_module_t* module,
                              const char* name,
                              hw_device_t** device) {
  VLOGF(1);
  // Make sure hal adapter loads the correct symbol.
  if (module != &HAL_MODULE_INFO_SYM.common) {
    LOGF(ERROR) << std::hex << "Invalid module 0x" << module << " expected 0x"
                << &HAL_MODULE_INFO_SYM.common;
    return -EINVAL;
  }

  char* nameEnd;
  int id = strtol(name, &nameEnd, 10);
  if (*nameEnd != '\0') {
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

int camera_device_close(struct hw_device_t* hw_device) {
  camera3_device_t* cam_dev = reinterpret_cast<camera3_device_t*>(hw_device);
  CameraClient* cam = static_cast<CameraClient*>(cam_dev->priv);
  if (!cam) {
    LOGF(ERROR) << "Camera device is NULL";
    return -EIO;
  }
  cam_dev->priv = NULL;
  return CameraHal::GetInstance().CloseDevice(cam->GetId());
}

}  // namespace arc

static hw_module_methods_t gCameraModuleMethods = {.open =
                                                       arc::camera_device_open};

camera_module_t HAL_MODULE_INFO_SYM
    __attribute__((__visibility__("default"))) = {
        .common = {.tag = HARDWARE_MODULE_TAG,
                   .module_api_version = CAMERA_MODULE_API_VERSION_2_2,
                   .hal_api_version = HARDWARE_HAL_API_VERSION,
                   .id = CAMERA_HARDWARE_MODULE_ID,
                   .name = "V4L2 Camera HAL v3",
                   .author = "The Chromium OS Authors",
                   .methods = &gCameraModuleMethods,
                   .dso = NULL,
                   .reserved = {0}},
        .get_number_of_cameras = arc::get_number_of_cameras,
        .get_camera_info = arc::get_camera_info,
        .set_callbacks = arc::set_callbacks,
        .get_vendor_tag_ops = NULL,
        .open_legacy = NULL,
        .set_torch_mode = NULL,
        .init = NULL,
        .reserved = {0}};
