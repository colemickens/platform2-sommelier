/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef HAL_USB_CAMERA_HAL_H_
#define HAL_USB_CAMERA_HAL_H_

#include <map>
#include <memory>
#include <vector>

#include <base/macros.h>
#include <base/single_thread_task_runner.h>
#include <base/threading/thread_checker.h>
#include <hardware/camera_common.h>

#include "arc/future.h"
#include "hal/usb/camera_client.h"
#include "hal/usb/common_types.h"

namespace arc {

// This class is not thread-safe. All functions in camera_module_t are called by
// one mojo thread which is in hal adapter. The hal adapter makes sure these
// functions are not called concurrently. The hal adapter also has different
// dedicated threads to handle camera_module_callbacks_t, camera3_device_ops_t,
// and camera3_callback_ops_t.
class CameraHal {
 public:
  CameraHal();
  ~CameraHal();

  static CameraHal& GetInstance();

  // Implementations for camera_module_t.
  int OpenDevice(int id, const hw_module_t* module, hw_device_t** hw_device);
  int GetNumberOfCameras() const { return device_infos_.size(); }
  // GetCameraInfo can be called before camera is opened when module api
  // version <= 2.3.
  int GetCameraInfo(int id, camera_info* info);
  int SetCallbacks(const camera_module_callbacks_t* callbacks);

  // Runs on device ops thread. Post a task to the thread which is used for
  // OpenDevice.
  void CloseDeviceOnOpsThread(int id);

 private:
  void CloseDevice(int id, scoped_refptr<internal::Future<void>> future);

  // Cache device information because querying the information is very slow.
  DeviceInfos device_infos_;

  // The key is camera id.
  std::map<int, std::unique_ptr<CameraClient>> cameras_;

  const camera_module_callbacks_t* callbacks_;

  // All methods of this class should be run on the same thread.
  base::ThreadChecker thread_checker_;

  // Used to report camera info at anytime.
  std::vector<CameraMetadataUniquePtr> static_infos_;

  // Used to post CloseDevice to run on the same thread.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  DISALLOW_COPY_AND_ASSIGN(CameraHal);
};

// Callback for camera_device.common.close().
int camera_device_close(struct hw_device_t* hw_device);

}  // namespace arc

extern camera_module_t HAL_MODULE_INFO_SYM;

#endif  // HAL_USB_CAMERA_HAL_H_
