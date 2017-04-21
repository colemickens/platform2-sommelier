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
    : public internal::MojoChannel<mojom::Camera3CallbackOps> {
 public:
  Camera3CallbackOpsDelegate(
      CameraDeviceAdapter* camera_device_adapter,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  ~Camera3CallbackOpsDelegate() = default;

  void ProcessCaptureResult(mojom::Camera3CaptureResultPtr result);

  void Notify(mojom::Camera3NotifyMsgPtr msg);

 private:
  void ProcessCaptureResultOnThread(mojom::Camera3CaptureResultPtr result);

  void NotifyOnThread(mojom::Camera3NotifyMsgPtr msg);

  CameraDeviceAdapter* camera_device_adapter_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(Camera3CallbackOpsDelegate);
};

}  // end of namespace arc

#endif  // HAL_ADAPTER_CAMERA3_CALLBACK_OPS_DELEGATE_H_
