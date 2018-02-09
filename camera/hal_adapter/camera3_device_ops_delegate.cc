/*
 * Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "hal_adapter/camera3_device_ops_delegate.h"

#include <utility>

#include "cros-camera/common.h"
#include "hal_adapter/camera_device_adapter.h"

namespace arc {

Camera3DeviceOpsDelegate::Camera3DeviceOpsDelegate(
    CameraDeviceAdapter* camera_device_adapter,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : internal::MojoBinding<Camera3DeviceOps>(task_runner),
      camera_device_adapter_(camera_device_adapter) {}

Camera3DeviceOpsDelegate::~Camera3DeviceOpsDelegate() {}

void Camera3DeviceOpsDelegate::Initialize(
    mojom::Camera3CallbackOpsPtr callback_ops,
    const InitializeCallback& callback) {
  VLOGF_ENTER();
  DCHECK(task_runner_->BelongsToCurrentThread());
  callback.Run(camera_device_adapter_->Initialize(std::move(callback_ops)));
}

void Camera3DeviceOpsDelegate::ConfigureStreams(
    mojom::Camera3StreamConfigurationPtr config,
    const ConfigureStreamsCallback& callback) {
  VLOGF_ENTER();
  DCHECK(task_runner_->BelongsToCurrentThread());
  mojom::Camera3StreamConfigurationPtr updated_config;
  int32_t result = camera_device_adapter_->ConfigureStreams(std::move(config),
                                                            &updated_config);
  callback.Run(result, std::move(updated_config));
}

void Camera3DeviceOpsDelegate::ConstructDefaultRequestSettings(
    mojom::Camera3RequestTemplate type,
    const ConstructDefaultRequestSettingsCallback& callback) {
  VLOGF_ENTER();
  DCHECK(task_runner_->BelongsToCurrentThread());
  callback.Run(camera_device_adapter_->ConstructDefaultRequestSettings(type));
}

void Camera3DeviceOpsDelegate::ProcessCaptureRequest(
    mojom::Camera3CaptureRequestPtr request,
    const ProcessCaptureRequestCallback& callback) {
  VLOGF_ENTER();
  DCHECK(task_runner_->BelongsToCurrentThread());
  callback.Run(
      camera_device_adapter_->ProcessCaptureRequest(std::move(request)));
}

void Camera3DeviceOpsDelegate::Dump(mojo::ScopedHandle fd) {
  VLOGF_ENTER();
  DCHECK(task_runner_->BelongsToCurrentThread());
  camera_device_adapter_->Dump(std::move(fd));
}

void Camera3DeviceOpsDelegate::Flush(const FlushCallback& callback) {
  VLOGF_ENTER();
  DCHECK(task_runner_->BelongsToCurrentThread());
  callback.Run(camera_device_adapter_->Flush());
}

void Camera3DeviceOpsDelegate::RegisterBuffer(
    uint64_t buffer_id,
    mojom::Camera3DeviceOps::BufferType type,
    mojo::Array<mojo::ScopedHandle> fds,
    uint32_t drm_format,
    mojom::HalPixelFormat hal_pixel_format,
    uint32_t width,
    uint32_t height,
    mojo::Array<uint32_t> strides,
    mojo::Array<uint32_t> offsets,
    const RegisterBufferCallback& callback) {
  VLOGF_ENTER();
  DCHECK(task_runner_->BelongsToCurrentThread());
  callback.Run(camera_device_adapter_->RegisterBuffer(
      buffer_id, type, std::move(fds), drm_format, hal_pixel_format, width,
      height, std::move(strides), std::move(offsets)));
}

void Camera3DeviceOpsDelegate::Close(const CloseCallback& callback) {
  VLOGF_ENTER();
  DCHECK(task_runner_->BelongsToCurrentThread());
  callback.Run(camera_device_adapter_->Close());
}

}  // namespace arc
