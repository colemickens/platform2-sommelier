/*
 * Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef HAL_ADAPTER_CAMERA3_DEVICE_OPS_DELEGATE_H_
#define HAL_ADAPTER_CAMERA3_DEVICE_OPS_DELEGATE_H_

#include "hal_adapter/cros_camera_mojo_utils.h"
#include "hal_adapter/mojo/camera3.mojom.h"

namespace arc {

class CameraDeviceAdapter;

class Camera3DeviceOpsDelegate final
    : public internal::MojoBinding<mojom::Camera3DeviceOps> {
 public:
  Camera3DeviceOpsDelegate(
      CameraDeviceAdapter* camera_device_adapter,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  ~Camera3DeviceOpsDelegate();

 private:
  void Initialize(mojom::Camera3CallbackOpsPtr callback_ops,
                  const InitializeCallback& callback) final;

  void ConfigureStreams(mojom::Camera3StreamConfigurationPtr config,
                        const ConfigureStreamsCallback& callback) final;

  void ConstructDefaultRequestSettings(
      mojom::Camera3RequestTemplate type,
      const ConstructDefaultRequestSettingsCallback& callback) final;

  void ProcessCaptureRequest(
      mojom::Camera3CaptureRequestPtr request,
      const ProcessCaptureRequestCallback& callback) final;

  void Dump(mojo::ScopedHandle fd) final;

  void Flush(const FlushCallback& callback) final;

  void RegisterBuffer(uint64_t buffer_id,
                      mojom::Camera3DeviceOps::BufferType type,
                      mojo::Array<mojo::ScopedHandle> fds,
                      uint32_t drm_format,
                      mojom::HalPixelFormat hal_pixel_format,
                      uint32_t width,
                      uint32_t height,
                      mojo::Array<uint32_t> strides,
                      mojo::Array<uint32_t> offsets,
                      const RegisterBufferCallback& callback) final;

  void Close(const CloseCallback& callback) final;

  CameraDeviceAdapter* camera_device_adapter_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(Camera3DeviceOpsDelegate);
};

}  // namespace arc

#endif  // HAL_ADAPTER_CAMERA3_DEVICE_OPS_DELEGATE_H_
