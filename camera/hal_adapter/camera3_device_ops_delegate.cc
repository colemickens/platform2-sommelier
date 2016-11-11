/*
 * Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "hal_adapter/camera3_device_ops_delegate.h"

#include <utility>

#include "hal_adapter/camera_device_adapter.h"

namespace arc {

Camera3DeviceOpsDelegate::Camera3DeviceOpsDelegate(
    CameraDeviceAdapter* camera_device_adapter)
    : camera_device_adapter_(camera_device_adapter) {}

Camera3DeviceOpsDelegate::~Camera3DeviceOpsDelegate() {}

void Camera3DeviceOpsDelegate::Initialize(
    mojom::Camera3CallbackOpsPtr callback_ops,
    const InitializeCallback& callback) {
  VLOG(2) << "Camera3DeviceOpsDelegate::Initialize";
  callback.Run(camera_device_adapter_->Initialize(std::move(callback_ops)));
}

void Camera3DeviceOpsDelegate::ConfigureStreams(
    mojom::Camera3StreamConfigurationPtr config,
    const ConfigureStreamsCallback& callback) {
  VLOG(2) << "Camera3DeviceOpsDelegate::ConfigureStreams";
  callback.Run(camera_device_adapter_->ConfigureStreams(std::move(config)));
}

void Camera3DeviceOpsDelegate::ConstructDefaultRequestSettings(
    int32_t type,
    const ConstructDefaultRequestSettingsCallback& callback) {
  VLOG(2) << "Camera3DeviceOpsDelegate::ConstructDefaultRequestSettings";
  callback.Run(camera_device_adapter_->ConstructDefaultRequestSettings(type));
}

void Camera3DeviceOpsDelegate::ProcessCaptureRequest(
    mojom::Camera3CaptureRequestPtr request,
    const ProcessCaptureRequestCallback& callback) {
  VLOG(2) << "Camera3DeviceOpsDelegate::ProcessCaptureRequest";
  callback.Run(
      camera_device_adapter_->ProcessCaptureRequest(std::move(request)));
}

void Camera3DeviceOpsDelegate::Dump(mojo::ScopedHandle fd,
                                    const DumpCallback& callback) {
  VLOG(2) << "Camera3DeviceOpsDelegate::Dump";
  camera_device_adapter_->Dump(std::move(fd));
  callback.Run();
}

void Camera3DeviceOpsDelegate::Flush(const FlushCallback& callback) {
  VLOG(2) << "Camera3DeviceOpsDelegate::Flush";
  callback.Run(camera_device_adapter_->Flush());
}

}  // namespace arc
