/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "hal/usb/camera_hal.h"

#include "arc/common.h"
#include "hal/usb/camera_metadata.h"
#include "hal/usb/common_types.h"
#include "hal/usb/stream_format.h"
#include "hal/usb/v4l2_camera_device.h"

namespace arc {

CameraHal::CameraHal() {
  std::unique_ptr<V4L2CameraDevice> device(new V4L2CameraDevice());
  device_infos_ = device->GetCameraDeviceInfos();
  VLOGF(1) << "Number of cameras is " << GetNumberOfCameras();

  for (auto& device_info : device_infos_) {
    CameraMetadata metadata;
    metadata.FillDefaultMetadata();
    metadata.FillMetadataFromDeviceInfo(device_info);

    SupportedFormats supported_formats =
        device->GetDeviceSupportedFormats(device_info.device_path);
    SupportedFormats qualified_formats = GetQualifiedFormats(supported_formats);
    metadata.FillMetadataFromSupportedFormats(qualified_formats);

    static_infos_.push_back(CameraMetadataUniquePtr(metadata.Release()));
  }
}

CameraHal::~CameraHal() {}

CameraHal& CameraHal::GetInstance() {
  static CameraHal camera_hal;
  return camera_hal;
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
  cameras_[id].reset(new CameraClient(id, device_infos_[id].device_path,
                                      *static_infos_[id].get(), module,
                                      hw_device));
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

  // camera_info_t.facing uses v1 definitions.
  // It should be CAMERA_FACING_BACK or CAMERA_FACING_FRONT.
  switch (device_infos_[id].lens_facing) {
    case ANDROID_LENS_FACING_FRONT:
      info->facing = CAMERA_FACING_FRONT;
      break;
    case ANDROID_LENS_FACING_BACK:
      info->facing = CAMERA_FACING_BACK;
      break;
    default:
      LOGF(ERROR) << "Unknown facing type: " << device_infos_[id].lens_facing;
      break;
  }
  info->orientation = device_infos_[id].sensor_orientation;
  info->device_version = CAMERA_DEVICE_API_VERSION_3_3;
  info->static_camera_characteristics = static_infos_[id].get();
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
