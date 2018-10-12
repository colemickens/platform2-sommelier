/*
 * Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef HAL_ADAPTER_CAMERA_HAL_TEST_ADAPTER_H_
#define HAL_ADAPTER_CAMERA_HAL_TEST_ADAPTER_H_

#include <utility>
#include <vector>

#include <hardware/camera3.h>

#include "hal_adapter/camera_hal_adapter.h"

namespace cros {

class CameraHalTestAdapter : public CameraHalAdapter {
 public:
  CameraHalTestAdapter(std::vector<camera_module_t*> camera_modules,
                       bool enable_front,
                       bool enable_back,
                       bool enable_external);

  ~CameraHalTestAdapter() override {}

  void StartOnThread(base::Callback<void(bool)> callback) override;

  int32_t GetNumberOfCameras() override;

 private:
  bool enable_front_, enable_back_, enable_external_;

  // Id of enabled cameras assigned by SuperHAL. |CameraHalTestAdapter| will
  // reassign new id exposed to framework based on its index in this vector.
  std::vector<int> enable_camera_ids_;

  int GetRemappedExternalCameraId(int external_camera_id);

  int GetUnRemappedExternalCameraId(int external_camera_id);

  std::pair<camera_module_t*, int> GetInternalModuleAndId(
      int camera_id) override;

  void NotifyCameraDeviceStatusChange(CameraModuleCallbacksDelegate* delegate,
                                      int camera_id,
                                      camera_device_status_t status) override;

  void NotifyTorchModeStatusChange(CameraModuleCallbacksDelegate* delegate,
                                   int camera_id,
                                   torch_mode_status_t status) override;

  DISALLOW_IMPLICIT_CONSTRUCTORS(CameraHalTestAdapter);
};

}  // namespace cros

#endif  // HAL_ADAPTER_CAMERA_HAL_TEST_ADAPTER_H_
