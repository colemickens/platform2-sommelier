/*
 * Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "hal_adapter/camera_module_delegate.h"

#include <utility>

#include "cros-camera/common.h"
#include "hal_adapter/camera_hal_adapter.h"

namespace cros {

CameraModuleDelegate::CameraModuleDelegate(
    CameraHalAdapter* camera_hal_adapter,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : internal::MojoBinding<mojom::CameraModule>(task_runner),
      camera_hal_adapter_(camera_hal_adapter) {}

CameraModuleDelegate::~CameraModuleDelegate() {}

void CameraModuleDelegate::OpenDevice(
    int32_t camera_id,
    mojom::Camera3DeviceOpsRequest device_ops_request,
    const OpenDeviceCallback& callback) {
  VLOGF_ENTER();
  DCHECK(task_runner_->BelongsToCurrentThread());
  mojom::Camera3DeviceOpsPtr device_ops;
  callback.Run(camera_hal_adapter_->OpenDevice(camera_id,
                                               std::move(device_ops_request)));
}

void CameraModuleDelegate::GetNumberOfCameras(
    const GetNumberOfCamerasCallback& callback) {
  VLOGF_ENTER();
  DCHECK(task_runner_->BelongsToCurrentThread());
  callback.Run(camera_hal_adapter_->GetNumberOfCameras());
}

void CameraModuleDelegate::GetCameraInfo(
    int32_t camera_id, const GetCameraInfoCallback& callback) {
  VLOGF_ENTER();
  DCHECK(task_runner_->BelongsToCurrentThread());
  mojom::CameraInfoPtr camera_info;
  int32_t result = camera_hal_adapter_->GetCameraInfo(camera_id, &camera_info);
  callback.Run(result, std::move(camera_info));
}

void CameraModuleDelegate::SetCallbacks(
    mojom::CameraModuleCallbacksPtr callbacks,
    const SetCallbacksCallback& callback) {
  VLOGF_ENTER();
  DCHECK(task_runner_->BelongsToCurrentThread());
  callback.Run(camera_hal_adapter_->SetCallbacks(std::move(callbacks)));
}

void CameraModuleDelegate::SetTorchMode(int32_t camera_id,
                                        bool enabled,
                                        const SetTorchModeCallback& callback) {
  VLOGF_ENTER();
  DCHECK(task_runner_->BelongsToCurrentThread());
  callback.Run(camera_hal_adapter_->SetTorchMode(camera_id, enabled));
}

void CameraModuleDelegate::Init(const InitCallback& callback) {
  VLOGF_ENTER();
  DCHECK(task_runner_->BelongsToCurrentThread());
  callback.Run(camera_hal_adapter_->Init());
}

}  // namespace cros
