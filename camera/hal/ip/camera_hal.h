/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef CAMERA_HAL_IP_CAMERA_HAL_H_
#define CAMERA_HAL_IP_CAMERA_HAL_H_

#include <map>
#include <memory>

#include <base/macros.h>
#include <base/synchronization/lock.h>
#include <base/synchronization/waitable_event.h>
#include <camera/camera_metadata.h>

#include <hardware/camera_common.h>
#include <sys/types.h>

#include "hal/ip/camera_device.h"
#include "hal/ip/ip_camera_detector.h"

namespace cros {

class CameraHal : public IpCameraDetector::Observer {
 public:
  CameraHal();
  ~CameraHal();

  static CameraHal& GetInstance();

  // Implementations of camera_module_t
  int OpenDevice(int id, const hw_module_t* module, hw_device_t** hw_device);
  int GetNumberOfCameras() const;
  int GetCameraInfo(int id, camera_info* info);
  int SetCallbacks(const camera_module_callbacks_t* callbacks);
  int Init();

  // Called from CameraDevice (camera3_device_t)
  int CloseDevice(int id);

 private:
  // IpCameraDetector::Observer interface
  int OnDeviceConnected(IpCameraDevice* device) override;
  void OnDeviceDisconnected(int id) override;

  int CloseDeviceLocked(int id);

  IpCameraDetector ip_camera_detector_;

  // The three maps, as well as |next_camera_id_| are protected by this lock
  base::Lock camera_map_lock_;
  std::map<int, IpCameraDevice*> cameras_;
  std::map<int, android::CameraMetadata> camera_static_info_;
  std::map<int, std::unique_ptr<CameraDevice>> open_cameras_;
  int next_camera_id_;

  // Any calls to OnDeviceConnected/OnDeviceDisconnected will block until
  // SetCallbacks has been called
  base::WaitableEvent callbacks_set_;
  const camera_module_callbacks_t* callbacks_;

  DISALLOW_COPY_AND_ASSIGN(CameraHal);
};

}  // namespace cros

extern camera_module_t HAL_MODULE_INFO_SYM;

#endif  // CAMERA_HAL_IP_CAMERA_HAL_H_
