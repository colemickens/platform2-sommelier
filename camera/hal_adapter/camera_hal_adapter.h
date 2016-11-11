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

#include "hal_adapter/arc_camera3.mojom.h"
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
  void OnShutdownComplete() override {}

  // Callback interface for CameraModuleDelegate..
  mojom::OpenDeviceResultPtr OpenDevice(int32_t device_id);

  int32_t CloseDevice(int32_t device_id);

  int32_t GetNumberOfCameras();

  mojom::GetCameraInfoResultPtr GetCameraInfo(int32_t device_id);

  int32_t SetCallbacks(mojom::CameraModuleCallbacksPtr callbacks);

 private:
  camera_module_t* camera_module_;

  base::ScopedFD socket_fd_;

  // Thread used in mojo to send and receive IPC messages.
  base::Thread ipc_thread_;

  std::unique_ptr<CameraModuleDelegate> module_delegate_;

  std::unique_ptr<CameraModuleCallbacksDelegate> callbacks_delegate_;

  std::map<int32_t, std::unique_ptr<CameraDeviceAdapter>> device_adapters_;

  DISALLOW_COPY_AND_ASSIGN(CameraHalAdapter);
};

}  // namespace arc

#endif  // HAL_ADAPTER_CAMERA_HAL_ADAPTER_H_
