/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef CAMERA_HAL_IP_CAMERA_DEVICE_H_
#define CAMERA_HAL_IP_CAMERA_DEVICE_H_

#include <hardware/camera3.h>
#include <hardware/hardware.h>

#include "hal/ip/ip_camera_device.h"
#include "hal/ip/request_queue.h"

namespace cros {

class CameraDevice {
 public:
  CameraDevice(int id,
               IpCameraDevice* ip_device,
               const hw_module_t* module,
               hw_device_t** hw_device);
  ~CameraDevice();

  int GetId() const;

  // Implementations of camera3_device_ops_t
  int Initialize(const camera3_callback_ops_t* callback_ops);
  int ConfigureStreams(camera3_stream_configuration_t* stream_list);
  const camera_metadata_t* ConstructDefaultRequestSettings(int type);
  int ProcessCaptureRequest(camera3_capture_request_t* request);
  int Flush();

  void Close();

 private:
  bool ValidateStream(camera3_stream_t* stream);

  const int id_;
  IpCameraDevice* const ip_device_;
  camera3_device_t camera3_device_;

  const camera3_callback_ops_t* callback_ops_;

  RequestQueue* request_queue_;

  DISALLOW_COPY_AND_ASSIGN(CameraDevice);
};

}  // namespace cros

#endif  // CAMERA_HAL_IP_CAMERA_DEVICE_H_
