/*
 * Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "hal_adapter/camera3_device_ops_delegate.h"

#include <utility>

#include "arc/common.h"
#include "hal_adapter/camera_device_adapter.h"

namespace arc {

Camera3DeviceOpsDelegate::Camera3DeviceOpsDelegate(
    CameraDeviceAdapter* camera_device_adapter)
    : camera_device_adapter_(camera_device_adapter) {}

Camera3DeviceOpsDelegate::~Camera3DeviceOpsDelegate() {}

void Camera3DeviceOpsDelegate::Initialize(
    mojom::Camera3CallbackOpsPtr callback_ops,
    const InitializeCallback& callback) {
  VLOGF_ENTER();
  DCHECK(thread_checker_.CalledOnValidThread());
  callback.Run(camera_device_adapter_->Initialize(std::move(callback_ops)));
}

void Camera3DeviceOpsDelegate::ConfigureStreams(
    mojom::Camera3StreamConfigurationPtr config,
    const ConfigureStreamsCallback& callback) {
  VLOGF_ENTER();
  DCHECK(thread_checker_.CalledOnValidThread());
  callback.Run(camera_device_adapter_->ConfigureStreams(std::move(config)));
}

void Camera3DeviceOpsDelegate::ConstructDefaultRequestSettings(
    int32_t type,
    const ConstructDefaultRequestSettingsCallback& callback) {
  VLOGF_ENTER();
  DCHECK(thread_checker_.CalledOnValidThread());
  callback.Run(camera_device_adapter_->ConstructDefaultRequestSettings(type));
}

void Camera3DeviceOpsDelegate::ProcessCaptureRequest(
    mojom::Camera3CaptureRequestPtr request,
    const ProcessCaptureRequestCallback& callback) {
  VLOGF_ENTER();
  DCHECK(thread_checker_.CalledOnValidThread());
  callback.Run(
      camera_device_adapter_->ProcessCaptureRequest(std::move(request)));
}

void Camera3DeviceOpsDelegate::Dump(mojo::ScopedHandle fd,
                                    const DumpCallback& callback) {
  VLOGF_ENTER();
  DCHECK(thread_checker_.CalledOnValidThread());
  camera_device_adapter_->Dump(std::move(fd));
  callback.Run();
}

void Camera3DeviceOpsDelegate::Flush(const FlushCallback& callback) {
  VLOGF_ENTER();
  DCHECK(thread_checker_.CalledOnValidThread());
  callback.Run(camera_device_adapter_->Flush());
}

void Camera3DeviceOpsDelegate::RegisterBuffer(
    uint64_t buffer_id,
    mojom::Camera3DeviceOps::BufferType type,
    mojo::Array<mojo::ScopedHandle> fds,
    uint32_t format,
    uint32_t width,
    uint32_t height,
    mojo::Array<uint32_t> strides,
    mojo::Array<uint32_t> offsets,
    const RegisterBufferCallback& callback) {
  VLOGF_ENTER();
  DCHECK(thread_checker_.CalledOnValidThread());
  callback.Run(camera_device_adapter_->RegisterBuffer(
      buffer_id, type, std::move(fds), format, width, height,
      std::move(strides), std::move(offsets)));
}

}  // namespace arc
