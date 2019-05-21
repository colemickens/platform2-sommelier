/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef CAMERA_HAL_IP_CAMERA_HAL_H_
#define CAMERA_HAL_IP_CAMERA_HAL_H_

#include <base/macros.h>
#include <base/synchronization/lock.h>
#include <base/synchronization/waitable_event.h>
#include <camera/camera_metadata.h>
#include <map>
#include <memory>
#include <sys/types.h>

#include "cros-camera/future.h"
#include "hal/ip/camera_device.h"
#include "hal/ip/ip_camera_detector.h"
#include "mojo/ip/ip_camera.mojom.h"

namespace cros {

class CameraHal : public mojom::IpCameraConnectionListener {
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

 private:
  // IpCameraConnectionListener interface
  void OnDeviceConnected(int32_t id,
                         mojom::IpCameraDevicePtr device_ptr,
                         mojom::IpCameraStreamPtr default_stream) override;
  void OnDeviceDisconnected(int32_t id) override;

  void InitOnIpcThread(scoped_refptr<Future<int>> return_val);
  void DestroyOnIpcThread();

  base::Thread ipc_thread_;
  std::unique_ptr<IpCameraDetectorImpl> detector_impl_;
  mojo::Binding<IpCameraConnectionListener> binding_;

  // The maps, as well as |next_camera_id_| are protected by this lock
  base::Lock camera_map_lock_;
  // Maps from detector id to HAL id
  std::map<int32_t, int> detector_ids_;
  std::map<int, std::unique_ptr<CameraDevice>> cameras_;
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
