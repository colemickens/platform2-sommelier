/*
 * Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef HAL_ADAPTER_CAMERA3_DEVICE_OPS_DELEGATE_H_
#define HAL_ADAPTER_CAMERA3_DEVICE_OPS_DELEGATE_H_

#include "hal_adapter/arc_camera3_mojo_utils.h"
#include "hal_adapter/mojo/arc_camera3.mojom.h"

namespace arc {

class CameraDeviceAdapter;

class Camera3DeviceOpsDelegate
    : public internal::MojoBindingDelegate<mojom::Camera3DeviceOps> {
 public:
  explicit Camera3DeviceOpsDelegate(CameraDeviceAdapter* camera_device_adapter);

  ~Camera3DeviceOpsDelegate();

 private:
  void Initialize(mojom::Camera3CallbackOpsPtr callback_ops,
                  const InitializeCallback& callback) override;

  void ConfigureStreams(mojom::Camera3StreamConfigurationPtr config,
                        const ConfigureStreamsCallback& callback) override;

  void ConstructDefaultRequestSettings(
      int32_t type,
      const ConstructDefaultRequestSettingsCallback& callback) override;

  void ProcessCaptureRequest(
      mojom::Camera3CaptureRequestPtr request,
      const ProcessCaptureRequestCallback& callback) override;

  void Dump(mojo::ScopedHandle fd, const DumpCallback& callback) override;

  void Flush(const FlushCallback& callback) override;

  void RegisterBuffer(uint64_t buffer_id,
                      mojom::Camera3DeviceOps::BufferType type,
                      mojo::Array<mojo::ScopedHandle> fds,
                      uint32_t format,
                      uint32_t width,
                      uint32_t height,
                      mojo::Array<uint32_t> strides,
                      mojo::Array<uint32_t> offsets,
                      const RegisterBufferCallback& callback) override;

  void Close(const CloseCallback& callback) override;

  CameraDeviceAdapter* camera_device_adapter_;

  DISALLOW_COPY_AND_ASSIGN(Camera3DeviceOpsDelegate);
};

}  // namespace arc

#endif  // HAL_ADAPTER_CAMERA3_DEVICE_OPS_DELEGATE_H_
