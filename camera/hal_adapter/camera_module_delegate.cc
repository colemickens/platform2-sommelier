/*
 * Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "camera_hal_adapter.h"
#include "camera_module_delegate.h"

namespace arc {

CameraModuleDelegate::CameraModuleDelegate(CameraHalAdapter* camera_hal_adapter,
                                           base::Closure quit_cb)
    : internal::MojoBindingDelegate<mojom::CameraModule>(quit_cb),
      camera_hal_adapter_(camera_hal_adapter) {}

CameraModuleDelegate::~CameraModuleDelegate() {}

void CameraModuleDelegate::OpenDevice(int32_t device_id,
                                      const OpenDeviceCallback& callback) {
  VLOG(2) << "CameraModuleDelegate::OpenDevice";
  callback.Run(camera_hal_adapter_->OpenDevice(device_id));
}

void CameraModuleDelegate::CloseDevice(int32_t device_id,
                                       const CloseDeviceCallback& callback) {
  VLOG(2) << "CameraModuleDelegate::CloseDevice";
  callback.Run(camera_hal_adapter_->CloseDevice(device_id));
}

void CameraModuleDelegate::GetNumberOfCameras(
    const GetNumberOfCamerasCallback& callback) {
  VLOG(2) << "CameraModuleDelegate::GetNumberOfCameras";
  callback.Run(camera_hal_adapter_->GetNumberOfCameras());
}

void CameraModuleDelegate::GetCameraInfo(
    int32_t device_id,
    const GetCameraInfoCallback& callback) {
  VLOG(2) << "CameraModuleDelegate::GetCameraInfo";
  callback.Run(camera_hal_adapter_->GetCameraInfo(device_id));
}

void CameraModuleDelegate::SetCallbacks(
    mojom::CameraModuleCallbacksPtr callbacks,
    const SetCallbacksCallback& callback) {
  VLOG(2) << "CameraModuleDelegate::SetCallbacks";
  callback.Run(camera_hal_adapter_->SetCallbacks(std::move(callbacks)));
}

}  // namespace arc
