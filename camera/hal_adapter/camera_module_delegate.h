/*
 * Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef CAMERA_MODULE_DELEGATE_H_
#define CAMERA_MODULE_DELEGATE_H_

#include "arc_camera3_mojo_utils.h"
#include "arc_camera3.mojom.h"

namespace arc {

class CameraHalAdapter;

class CameraModuleDelegate
    : public internal::MojoBindingDelegate<mojom::CameraModule> {
 public:
  CameraModuleDelegate(CameraHalAdapter* camera_hal_adapter,
                       base::Closure quit_cb);

  ~CameraModuleDelegate();

 private:
  void OpenDevice(int32_t device_id,
                  const OpenDeviceCallback& callback) override;

  void CloseDevice(int32_t device_id,
                   const CloseDeviceCallback& callback) override;

  void GetNumberOfCameras(const GetNumberOfCamerasCallback& callback) override;

  void GetCameraInfo(int32_t device_id,
                     const GetCameraInfoCallback& callback) override;

  void SetCallbacks(mojom::CameraModuleCallbacksPtr callbacks,
                    const SetCallbacksCallback& callback) override;

  CameraHalAdapter* camera_hal_adapter_;

  DISALLOW_COPY_AND_ASSIGN(CameraModuleDelegate);
};

}  // namespace arc

#endif  // CAMERA_MODULE_DELEGATE_H_
