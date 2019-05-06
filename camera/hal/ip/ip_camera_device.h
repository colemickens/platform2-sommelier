/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef CAMERA_HAL_IP_IP_CAMERA_DEVICE_H_
#define CAMERA_HAL_IP_IP_CAMERA_DEVICE_H_

#include <base/macros.h>

#include "hal/ip/request_queue.h"

namespace cros {

class IpCameraDevice {
 public:
  IpCameraDevice() : request_queue_() {}
  virtual ~IpCameraDevice() {}

  virtual int GetFormat() const = 0;
  virtual int GetWidth() const = 0;
  virtual int GetHeight() const = 0;
  virtual double GetFPS() const = 0;

  virtual RequestQueue* StartStreaming() = 0;
  virtual void StopStreaming() = 0;

 protected:
  RequestQueue request_queue_;

 private:
  DISALLOW_COPY_AND_ASSIGN(IpCameraDevice);
};

}  // namespace cros

#endif  // CAMERA_HAL_IP_IP_CAMERA_DEVICE_H_
