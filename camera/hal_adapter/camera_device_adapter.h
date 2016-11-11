/*
 * Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef HAL_ADAPTER_CAMERA_DEVICE_ADAPTER_H_
#define HAL_ADAPTER_CAMERA_DEVICE_ADAPTER_H_

#include <map>
#include <memory>

#include <base/threading/thread.h>
#include <mojo/public/cpp/bindings/binding.h>

#include "hal_adapter/arc_camera3.mojom.h"
#include "hal_adapter/arc_camera3_mojo_utils.h"
#include "hardware/camera3.h"

namespace arc {

class Camera3DeviceOpsDelegate;

class Camera3CallbackOpsDelegate;

class CameraDeviceAdapter {
 public:
  explicit CameraDeviceAdapter(camera3_device_t* camera_device);

  ~CameraDeviceAdapter();

  mojom::Camera3DeviceOpsPtr GetDeviceOpsPtr();

  int Close();

  // Callback interface for Camera3DeviceOpsDelegate.
  int32_t Initialize(mojom::Camera3CallbackOpsPtr callback_ops);

  mojom::Camera3StreamConfigurationPtr ConfigureStreams(
      mojom::Camera3StreamConfigurationPtr config);

  mojom::CameraMetadataPtr ConstructDefaultRequestSettings(int32_t type);

  int32_t ProcessCaptureRequest(mojom::Camera3CaptureRequestPtr request);

  void Dump(mojo::ScopedHandle fd);

  int32_t Flush();

  // Callback interface for Camera3CallbackOpsDelegate.
  mojom::Camera3CaptureResultPtr ProcessCaptureResult(
      const camera3_capture_result_t* result);

  mojom::Camera3NotifyMsgPtr Notify(const camera3_notify_msg_t* msg);

 private:
  std::unique_ptr<Camera3DeviceOpsDelegate> device_ops_delegate_;

  std::unique_ptr<Camera3CallbackOpsDelegate> callback_ops_delegate_;

  // The real camera device.
  camera3_device_t* camera_device_;

  base::Lock streams_lock_;

  // A mapping from Andoird HAL for all the configured streams.
  internal::UniqueStreams streams_;

  DISALLOW_COPY_AND_ASSIGN(CameraDeviceAdapter);
};

}  // namespace arc

#endif  // HAL_ADAPTER_CAMERA_DEVICE_ADAPTER_H_
