/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef CAMERA_HAL_IP_IP_CAMERA_H_
#define CAMERA_HAL_IP_IP_CAMERA_H_

#include <base/task_runner.h>
#include <mojo/public/cpp/bindings/binding.h>

#include "mojo/ip/ip_camera.mojom.h"

namespace cros {

class IpCamera : public mojom::IpCameraDevice {
 public:
  IpCamera();
  ~IpCamera();

  int Init(mojom::IpCameraDeviceRequest request);

  virtual int32_t GetWidth() const = 0;
  virtual int32_t GetHeight() const = 0;
  virtual mojom::PixelFormat GetFormat() const = 0;
  virtual double GetFps() const = 0;

 protected:
  virtual int Init() = 0;
  mojom::IpCameraFrameListenerPtr listener_;
  scoped_refptr<base::TaskRunner> ipc_task_runner_;

 private:
  // IpCameraDevice interface
  void RegisterFrameListener(mojom::IpCameraFrameListenerPtr listener) override;
  void StartStreaming() override = 0;
  void StopStreaming() override = 0;

  void OnConnectionError();

  bool init_called_;
  mojo::Binding<mojom::IpCameraDevice> binding_;

  DISALLOW_COPY_AND_ASSIGN(IpCamera);
};

}  // namespace cros

#endif  // CAMERA_HAL_IP_IP_CAMERA_H_
