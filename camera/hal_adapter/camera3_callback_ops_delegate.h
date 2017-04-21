/*
 * Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef HAL_ADAPTER_CAMERA3_CALLBACK_OPS_DELEGATE_H_
#define HAL_ADAPTER_CAMERA3_CALLBACK_OPS_DELEGATE_H_

#include "hal_adapter/arc_camera3_mojo_utils.h"
#include "hal_adapter/mojo/arc_camera3.mojom.h"
#include "hardware/camera3.h"

namespace arc {

class CameraDeviceAdapter;

class Camera3CallbackOpsDelegate
    : public internal::MojoChannel<mojom::Camera3CallbackOps>,
      public camera3_callback_ops_t {
 public:
  Camera3CallbackOpsDelegate(
      CameraDeviceAdapter* camera_device_adapter,
      mojo::InterfacePtrInfo<mojom::Camera3CallbackOps> callback_ops_ptr_info,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  ~Camera3CallbackOpsDelegate() = default;

 private:
  // Interface for camera3_callback_ops_t.
  static void ProcessCaptureResult(const camera3_callback_ops_t* ops,
                                   const camera3_capture_result_t* result);

  static void Notify(const camera3_callback_ops_t* ops,
                     const camera3_notify_msg_t* msg);

  void ProcessCaptureResultOnThread(const camera3_capture_result_t* result,
                                    const base::Callback<void()>& cb);

  void NotifyOnThread(const camera3_notify_msg_t* msg,
                      const base::Callback<void()>& cb);

  CameraDeviceAdapter* camera_device_adapter_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(Camera3CallbackOpsDelegate);
};

}  // end of namespace arc

#endif  // HAL_ADAPTER_CAMERA3_CALLBACK_OPS_DELEGATE_H_
