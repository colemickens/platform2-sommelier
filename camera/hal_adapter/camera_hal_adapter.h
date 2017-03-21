/*
 * Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef HAL_ADAPTER_CAMERA_HAL_ADAPTER_H_
#define HAL_ADAPTER_CAMERA_HAL_ADAPTER_H_

#include <map>
#include <memory>
#include <string>

#include <base/threading/thread.h>
#include <mojo/edk/embedder/process_delegate.h>

#include "hal_adapter/mojo/arc_camera3.mojom.h"
#include "hardware/camera3.h"

namespace arc {

class CameraDeviceAdapter;

class CameraModuleDelegate;

class CameraModuleCallbacksDelegate;

class CameraHalAdapter : public mojo::edk::ProcessDelegate {
 public:
  CameraHalAdapter(camera_module_t* camera_module,
                   int socket_fd,
                   base::Closure quit_cb);

  ~CameraHalAdapter();

  // Create a mojo connection to container.
  bool Start();

  // ProcessDelegate implementation.
  void OnShutdownComplete() override;

  // Callback interface for CameraModuleDelegate.
  // These methods are callbacks for |module_delegate_| and are executed on
  // the mojo IPC handler thread in |module_delegate_|.
  int32_t OpenDevice(int32_t device_id, mojom::Camera3DeviceOpsPtr* device_ops);

  int32_t GetNumberOfCameras();

  int32_t GetCameraInfo(int32_t device_id, mojom::CameraInfoPtr* camera_info);

  int32_t SetCallbacks(mojom::CameraModuleCallbacksPtr callbacks);

  // A callback for the camera devices opened in OpenDevice().  Used to run
  // CloseDevice() on the same thread that OpenDevice() runs on.
  void CloseDeviceCallback(base::TaskRunner* runner, int32_t device_id);

 private:
  // Clean up the camera device specified by |device_id| in |device_adapters_|.
  void CloseDevice(int32_t device_id);

  // The handle to the camera HAL dlopen()'d on process start.
  camera_module_t* camera_module_;

  // The unix domain socket used to establish the mojo IPC channel.
  base::ScopedFD socket_fd_;

  // Thread used in mojo to send and receive IPC messages.
  base::Thread ipc_thread_;

  // The delegate that handles the CameraModule mojo IPC.
  std::unique_ptr<CameraModuleDelegate> module_delegate_;

  // The delegate that handles the CameraModuleCallbacks mojo IPC.
  std::unique_ptr<CameraModuleCallbacksDelegate> callbacks_delegate_;

  // The handles to the opened camera devices.  |device_adapters_| is accessed
  // only in OpenDevice() and CloseDevice().  In order to do lock-free access to
  // |device_adapters_|, we run OpenDevice() and CloseDevice() on the same
  // thread (i.e. the mojo IPC handler thread in |module_delegate_|).
  std::map<int32_t, std::unique_ptr<CameraDeviceAdapter>> device_adapters_;

  DISALLOW_COPY_AND_ASSIGN(CameraHalAdapter);
};

}  // namespace arc

#endif  // HAL_ADAPTER_CAMERA_HAL_ADAPTER_H_
