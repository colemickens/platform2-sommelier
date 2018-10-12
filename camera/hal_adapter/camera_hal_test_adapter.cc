/*
 * Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "hal_adapter/camera_hal_test_adapter.h"

#include "cros-camera/future.h"

namespace cros {

CameraHalTestAdapter::CameraHalTestAdapter(
    std::vector<camera_module_t*> camera_modules,
    bool enable_front,
    bool enable_back,
    bool enable_external)
    : CameraHalAdapter(camera_modules),
      enable_front_(enable_front),
      enable_back_(enable_back),
      enable_external_(enable_external) {
  VLOGF_ENTER();
  LOGF(INFO) << "Filter options: enable_front=" << enable_front_
             << ", enable_back=" << enable_back_
             << ", enable_external=" << enable_external_;
}

void CameraHalTestAdapter::StartOnThread(base::Callback<void(bool)> callback) {
  VLOGF_ENTER();

  auto future = cros::Future<bool>::Create(nullptr);
  CameraHalAdapter::StartOnThread(cros::GetFutureCallback(future));

  if (!future.get()) {
    callback.Run(false);
    return;
  }

  for (int cam_id = 0; cam_id < CameraHalAdapter::GetNumberOfCameras();
       cam_id++) {
    camera_module_t* m;
    int internal_id;
    std::tie(m, internal_id) = CameraHalAdapter::GetInternalModuleAndId(cam_id);

    camera_info_t info;
    int ret = m->get_camera_info(internal_id, &info);
    if (ret != 0) {
      LOGF(ERROR) << "Failed to get info of camera " << cam_id;
      callback.Run(false);
      return;
    }

    if ((info.facing == CAMERA_FACING_BACK && enable_back_) ||
        (info.facing == CAMERA_FACING_FRONT && enable_front_)) {
      LOGF(INFO) << "Remap camera id " << cam_id << "->"
                 << enable_camera_ids_.size();
      enable_camera_ids_.push_back(cam_id);
    } else {
      LOG(INFO) << "Filter out camera " << internal_id << " facing "
                << info.facing << " of module " << m->common.name;
    }
  }
  LOGF(INFO) << "Enable total " << enable_camera_ids_.size() << " cameras";
  callback.Run(true);
}

int CameraHalTestAdapter::GetRemappedExternalCameraId(int external_camera_id) {
  return external_camera_id - CameraHalAdapter::GetNumberOfCameras() +
         GetNumberOfCameras();
}

int CameraHalTestAdapter::GetUnRemappedExternalCameraId(
    int external_camera_id) {
  return external_camera_id + CameraHalAdapter::GetNumberOfCameras() -
         GetNumberOfCameras();
}

std::pair<camera_module_t*, int> CameraHalTestAdapter::GetInternalModuleAndId(
    int camera_id) {
  VLOGF_ENTER();

  if (camera_id < 0) {
    LOGF(ERROR) << "Invalid test camera id: " << camera_id;
    return {};
  }
  if (camera_id >= enable_camera_ids_.size()) {
    // external camera
    return CameraHalAdapter::GetInternalModuleAndId(
        GetUnRemappedExternalCameraId(camera_id));
  }
  return CameraHalAdapter::GetInternalModuleAndId(
      enable_camera_ids_[camera_id]);
}

int32_t CameraHalTestAdapter::GetNumberOfCameras() {
  VLOGF_ENTER();
  return enable_camera_ids_.size();
}

void CameraHalTestAdapter::NotifyCameraDeviceStatusChange(
    CameraModuleCallbacksDelegate* delegate,
    int camera_id,
    camera_device_status_t status) {
  if (enable_external_) {
    int new_external_id = GetRemappedExternalCameraId(camera_id);
    LOGF(INFO) << "Remap external camera id " << camera_id << "->"
               << new_external_id;
    CameraHalAdapter::NotifyCameraDeviceStatusChange(delegate, new_external_id,
                                                     status);
  }
}

void CameraHalTestAdapter::NotifyTorchModeStatusChange(
    CameraModuleCallbacksDelegate* delegate,
    int camera_id,
    torch_mode_status_t status) {
  if (camera_id >= CameraHalAdapter::GetNumberOfCameras()) {
    // external camera
    if (enable_external_) {
      CameraHalAdapter::NotifyTorchModeStatusChange(
          delegate, GetRemappedExternalCameraId(camera_id), status);
    }
  } else {
    auto it = std::find(enable_camera_ids_.begin(), enable_camera_ids_.end(),
                        camera_id);
    if (it != enable_camera_ids_.end()) {
      CameraHalAdapter::NotifyTorchModeStatusChange(
          delegate, std::distance(enable_camera_ids_.begin(), it), status);
    }
  }
}

}  // namespace cros
