/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef CAMERA_HAL_IP_IP_CAMERA_DETECTOR_H_
#define CAMERA_HAL_IP_IP_CAMERA_DETECTOR_H_

#include <base/threading/thread.h>
#include <map>
#include <memory>
#include <mojo/public/cpp/bindings/binding.h>

#include "cros-camera/future.h"
#include "hal/ip/ip_camera.h"
#include "mojo/ip/ip_camera.mojom.h"

namespace cros {

// Tracks which IP cameras are available on the network and notifies the
// observer when they connect/disconnect.
class IpCameraDetectorImpl : public mojom::IpCameraDetector {
 public:
  IpCameraDetectorImpl();
  ~IpCameraDetectorImpl();

  int Init(mojom::IpCameraDetectorRequest request);

 private:
  void RegisterConnectionListener(
      mojom::IpCameraConnectionListenerPtr listener) override;
  void DestroyOnIpcThread(scoped_refptr<Future<void>> return_val);
  void MockFrameGeneratorConnect();
  void DeviceDisconnect(int32_t id);

  mojo::Binding<IpCameraDetector> binding_;
  scoped_refptr<base::TaskRunner> ipc_task_runner_;
  mojom::IpCameraConnectionListenerPtr listener_;

  int32_t next_camera_id_;
  std::map<int32_t, std::unique_ptr<IpCamera>> devices_;

  DISALLOW_COPY_AND_ASSIGN(IpCameraDetectorImpl);
};

}  // namespace cros

#endif  // CAMERA_HAL_IP_IP_CAMERA_DETECTOR_H_
