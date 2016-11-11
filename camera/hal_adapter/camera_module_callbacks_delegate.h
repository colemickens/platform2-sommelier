/*
 * Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef HAL_ADAPTER_CAMERA_MODULE_CALLBACKS_DELEGATE_H_
#define HAL_ADAPTER_CAMERA_MODULE_CALLBACKS_DELEGATE_H_

#include "hal_adapter/arc_camera3.mojom.h"
#include "hal_adapter/arc_camera3_mojo_utils.h"

namespace arc {

class CameraModuleCallbacksDelegate
    : public internal::MojoInterfaceDelegate<mojom::CameraModuleCallbacks>,
      public camera_module_callbacks_t {
 public:
  CameraModuleCallbacksDelegate(
      mojo::InterfacePtrInfo<mojom::CameraModuleCallbacks> callbacks_ptr_info);

  ~CameraModuleCallbacksDelegate() = default;

 private:
  // Interface for camera_module_callbacks_t.
  static void CameraDeviceStatusChange(
      const camera_module_callbacks_t* callbacks,
      int camera_id,
      int new_status);

  void CameraDeviceStatusChangeOnThread(int camera_id,
                                        int new_status,
                                        const base::Callback<void()>& cb);

  DISALLOW_COPY_AND_ASSIGN(CameraModuleCallbacksDelegate);
};

}  // namespace arc

#endif  // HAL_ADAPTER_CAMERA_MODULE_CALLBACKS_DELEGATE_H_
