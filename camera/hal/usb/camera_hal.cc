/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "hal/usb/camera_hal.h"

#include <algorithm>
#include <utility>

#include <base/bind.h>
#include <base/threading/thread_task_runner_handle.h>

#include "cros-camera/common.h"
#include "cros-camera/udev_watcher.h"
#include "hal/usb/camera_characteristics.h"
#include "hal/usb/common_types.h"
#include "hal/usb/metadata_handler.h"
#include "hal/usb/stream_format.h"
#include "hal/usb/v4l2_camera_device.h"

namespace cros {

namespace {

CameraMetadataUniquePtr GetStaticInfoFromDeviceInfo(
    const DeviceInfo& device_info) {
  android::CameraMetadata metadata;

  if (MetadataHandler::FillDefaultMetadata(&metadata) != 0) {
    return nullptr;
  }

  if (MetadataHandler::FillMetadataFromDeviceInfo(device_info, &metadata) !=
      0) {
    return nullptr;
  }

  SupportedFormats supported_formats =
      V4L2CameraDevice::GetDeviceSupportedFormats(device_info.device_path);
  SupportedFormats qualified_formats = GetQualifiedFormats(supported_formats);
  if (MetadataHandler::FillMetadataFromSupportedFormats(qualified_formats,
                                                        &metadata) != 0) {
    return nullptr;
  }

  return CameraMetadataUniquePtr(metadata.release());
}

}  // namespace

CameraHal::CameraHal()
    : task_runner_(nullptr), udev_watcher_(this, "video4linux") {
  thread_checker_.DetachFromThread();
}

CameraHal::~CameraHal() {}

int CameraHal::GetNumberOfCameras() const {
  return num_builtin_cameras_;
}

CameraHal& CameraHal::GetInstance() {
  static CameraHal camera_hal;
  return camera_hal;
}

int CameraHal::OpenDevice(int id,
                          const hw_module_t* module,
                          hw_device_t** hw_device) {
  VLOGFID(1, id);
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!IsValidCameraId(id)) {
    LOGF(ERROR) << "Camera id " << id << " is invalid";
    return -EINVAL;
  }

  if (cameras_.find(id) != cameras_.end()) {
    LOGF(ERROR) << "Camera " << id << " is already opened";
    return -EBUSY;
  }
  cameras_[id].reset(new CameraClient(
      id, device_infos_[id], *static_infos_[id].get(), module, hw_device));
  if (cameras_[id]->OpenDevice()) {
    cameras_.erase(id);
    return -ENODEV;
  }
  if (!task_runner_) {
    task_runner_ = base::ThreadTaskRunnerHandle::Get();
  }
  return 0;
}

bool CameraHal::IsValidCameraId(int id) {
  return device_infos_.find(id) != device_infos_.end();
}

int CameraHal::GetCameraInfo(int id, struct camera_info* info) {
  VLOGFID(1, id);
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!IsValidCameraId(id)) {
    LOGF(ERROR) << "Camera id " << id << " is invalid";
    return -EINVAL;
  }

  switch (device_infos_[id].lens_facing) {
    case ANDROID_LENS_FACING_FRONT:
      info->facing = CAMERA_FACING_FRONT;
      break;
    case ANDROID_LENS_FACING_BACK:
      info->facing = CAMERA_FACING_BACK;
      break;
    case ANDROID_LENS_FACING_EXTERNAL:
      info->facing = CAMERA_FACING_EXTERNAL;
      break;
    default:
      LOGF(ERROR) << "Unknown facing type: " << device_infos_[id].lens_facing;
      break;
  }
  info->orientation = device_infos_[id].sensor_orientation;
  info->device_version = CAMERA_DEVICE_API_VERSION_3_3;
  info->static_camera_characteristics = static_infos_[id].get();
  info->resource_cost = 0;
  info->conflicting_devices = nullptr;
  info->conflicting_devices_length = 0;
  return 0;
}

int CameraHal::SetCallbacks(const camera_module_callbacks_t* callbacks) {
  VLOGF(1) << "New callbacks = " << callbacks;
  DCHECK(thread_checker_.CalledOnValidThread());

  callbacks_ = callbacks;

  // Some external cameras might be detected before SetCallbacks, we should
  // enumerate existing devices again after we return from SetCallbacks().
  base::MessageLoop::current()->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(IgnoreResult(&UdevWatcher::EnumerateExistingDevices),
                 base::Unretained(&udev_watcher_)));

  return 0;
}

int CameraHal::Init() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!udev_watcher_.Start(base::ThreadTaskRunnerHandle::Get())) {
    LOGF(ERROR) << "Failed to Start()";
    return -ENODEV;
  }

  if (!udev_watcher_.EnumerateExistingDevices()) {
    LOGF(ERROR) << "Failed to EnumerateExistingDevices()";
    return -ENODEV;
  }

  // TODO(shik): possible race here. We may have 2 built-in cameras but just
  // detect one.
  if (CameraCharacteristics::ConfigFileExists() && num_builtin_cameras_ == 0) {
    LOGF(ERROR) << "Expect to find at least one camera if config file exists";
    return -ENODEV;
  }

  // TODO(shik): Some skus of unibuild devices may have only user-facing
  // camera as "camera1" in |characteristics_|.  They are currently running
  // HALv1, and we need to fix this before migrating them to HALv3 with
  // v1-over-v3 adapter. (b/111770440)
  for (int i = 0; i < num_builtin_cameras_; i++) {
    if (!IsValidCameraId(i)) {
      LOGF(ERROR)
          << "The camera devices should be numbered 0 through N-1, but id = "
          << i << " is missing";
      return -ENODEV;
    }
  }

  next_external_camera_id_ = num_builtin_cameras_;

  return 0;
}

void CameraHal::CloseDeviceOnOpsThread(int id) {
  DCHECK(task_runner_);
  auto future = cros::Future<void>::Create(nullptr);
  task_runner_->PostTask(
      FROM_HERE, base::Bind(&CameraHal::CloseDevice, base::Unretained(this), id,
                            base::RetainedRef(future)));
  future->Wait();
}

void CameraHal::CloseDevice(int id, scoped_refptr<cros::Future<void>> future) {
  VLOGFID(1, id);
  DCHECK(thread_checker_.CalledOnValidThread());

  if (cameras_.find(id) == cameras_.end()) {
    LOGF(ERROR) << "Failed to close camera device " << id
                << ": device is not opened";
    future->Set();
    return;
  }
  cameras_.erase(id);
  future->Set();
}

void CameraHal::OnDeviceAdded(ScopedUdevDevicePtr dev) {
  udev_device* parent_dev = udev_device_get_parent_with_subsystem_devtype(
      dev.get(), "usb", "usb_device");
  if (!parent_dev) {
    // TODO(shik): The vivid device might not be a usb device.
    VLOGF(2) << "Non USB device is ignored";
    return;
  }

  const char* path = udev_device_get_devnode(dev.get());
  if (!path) {
    LOGF(ERROR) << "udev_device_get_devnode failed";
    return;
  }

  const char* vid = udev_device_get_sysattr_value(parent_dev, "idVendor");
  if (!vid) {
    LOGF(ERROR) << "Failed to get vid";
    return;
  }

  const char* pid = udev_device_get_sysattr_value(parent_dev, "idProduct");
  if (!pid) {
    LOGF(ERROR) << "Failed to get pid";
    return;
  }

  {
    // We have to check this because of:
    //  1. Limitation of libudev
    //  2. Reenumeration after SetCallbacks()
    //  3. Suspend/Resume
    auto it = path_to_id_.find(path);
    if (it != path_to_id_.end()) {
      int id = it->second;
      const DeviceInfo& info = device_infos_[id];
      if (info.usb_vid == vid && info.usb_pid == pid) {
        VLOGF(1) << "Ignore " << path << " since it's already connected";
      } else {
        LOGF(ERROR) << "Device path conflict: " << path;
      }
      return;
    }
  }

  if (!V4L2CameraDevice::IsCameraDevice(path)) {
    LOGF(INFO) << path << " is not a camera device";
    return;
  }

  LOGF(INFO) << "New camera device at " << path << " vid: " << vid
             << " pid: " << pid;

  const DeviceInfo* info_ptr = characteristics_.Find(vid, pid);
  DeviceInfo info;
  if (info_ptr != nullptr) {
    VLOGF(1) << "Found a built-in camera";
    info = *info_ptr;
    num_builtin_cameras_ = std::max(num_builtin_cameras_, info.camera_id + 1);
  } else {
    VLOGF(1) << "Found an external camera";
    if (callbacks_ == nullptr) {
      VLOGF(1) << "No callbacks set, ignore it for now";
      return;
    }
    info.camera_id = next_external_camera_id_++;
    info.lens_facing = ANDROID_LENS_FACING_EXTERNAL;
  }

  info.device_path = path;
  info.usb_vid = vid;
  info.usb_pid = pid;
  info.power_line_frequency = V4L2CameraDevice::GetPowerLineFrequency(path);

  CameraMetadataUniquePtr static_info = GetStaticInfoFromDeviceInfo(info);
  if (!static_info) {
    if (info.lens_facing == ANDROID_LENS_FACING_EXTERNAL) {
      LOGF(ERROR) << "GetStaticInfoFromDeviceInfo failed, the new external "
                     "camera would be ignored";
      return;
    } else {
      LOGF(FATAL) << "GetStaticInfoFromDeviceInfo failed for a built-in "
                     "camera, please check your camera config";
    }
  }

  path_to_id_[info.device_path] = info.camera_id;
  device_infos_[info.camera_id] = info;
  static_infos_[info.camera_id] = std::move(static_info);

  if (info.lens_facing == ANDROID_LENS_FACING_EXTERNAL) {
    callbacks_->camera_device_status_change(callbacks_, info.camera_id,
                                            CAMERA_DEVICE_STATUS_PRESENT);
  }
}

void CameraHal::OnDeviceRemoved(ScopedUdevDevicePtr dev) {
  const char* path = udev_device_get_devnode(dev.get());
  if (!path) {
    LOGF(ERROR) << "udev_device_get_devnode failed";
    return;
  }

  auto it = path_to_id_.find(path);
  if (it == path_to_id_.end()) {
    VLOGF(1) << "Cannot found id for " << path << ", ignore it";
    return;
  }

  int id = it->second;

  if (id < num_builtin_cameras_) {
    VLOGF(1) << "Camera " << id << "is a built-in camera, ignore it";
    return;
  }

  LOGF(INFO) << "Camera " << id << " at " << path << " removed";

  // TODO(shik): Handle this more gracefully, sometimes it even trigger a kernel
  // panic.
  CHECK(cameras_.find(id) == cameras_.end())
      << "Unplug an opening camera, abort as intended";

  path_to_id_.erase(it);
  device_infos_.erase(id);
  static_infos_.erase(id);

  if (callbacks_) {
    callbacks_->camera_device_status_change(callbacks_, id,
                                            CAMERA_DEVICE_STATUS_NOT_PRESENT);
  }
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

static void get_vendor_tag_ops(vendor_tag_ops_t* /*ops*/) {
  // no-op
}

static int open_legacy(const struct hw_module_t* /*module*/, const char* /*id*/,
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

int camera_device_close(struct hw_device_t* hw_device) {
  camera3_device_t* cam_dev = reinterpret_cast<camera3_device_t*>(hw_device);
  CameraClient* cam = static_cast<CameraClient*>(cam_dev->priv);
  if (!cam) {
    LOGF(ERROR) << "Camera device is NULL";
    return -EIO;
  }
  cam_dev->priv = NULL;
  int ret = cam->CloseDevice();
  CameraHal::GetInstance().CloseDeviceOnOpsThread(cam->GetId());
  return ret;
}

}  // namespace cros

static hw_module_methods_t gCameraModuleMethods = {
    .open = cros::camera_device_open};

camera_module_t HAL_MODULE_INFO_SYM CROS_CAMERA_EXPORT = {
    .common = {.tag = HARDWARE_MODULE_TAG,
               .module_api_version = CAMERA_MODULE_API_VERSION_2_4,
               .hal_api_version = HARDWARE_HAL_API_VERSION,
               .id = CAMERA_HARDWARE_MODULE_ID,
               .name = "V4L2 UVC Camera HAL v3",
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
