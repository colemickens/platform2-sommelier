/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef CAMERA_HAL_IP_IP_CAMERA_DETECTOR_H_
#define CAMERA_HAL_IP_IP_CAMERA_DETECTOR_H_

#include <map>

#include <base/macros.h>
#include <base/threading/platform_thread.h>

#include "hal/ip/ip_camera_device.h"

namespace cros {

// Tracks which IP cameras are available on the network and notifies the
// observer when they connect/disconnect.
class IpCameraDetector : base::PlatformThread::Delegate {
 public:
  class Observer {
   public:
    virtual ~Observer() = default;
    virtual int OnDeviceConnected(IpCameraDevice* device);
    virtual void OnDeviceDisconnected(int id);
  };

  explicit IpCameraDetector(Observer* observer);
  ~IpCameraDetector();

  bool Start();
  void Stop();

 private:
  void ThreadMain() override;
  Observer* observer_;

  base::PlatformThreadHandle thread_handle_;

  std::map<int, IpCameraDevice*> cameras_;

  DISALLOW_COPY_AND_ASSIGN(IpCameraDetector);
};

}  // namespace cros

#endif  // CAMERA_HAL_IP_IP_CAMERA_DETECTOR_H_
