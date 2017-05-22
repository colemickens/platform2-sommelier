/*
 * Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef INCLUDE_ARC_CAMERA_ALGORITHM_ADAPTER_H_
#define INCLUDE_ARC_CAMERA_ALGORITHM_ADAPTER_H_

#include "base/threading/thread.h"
#include "mojo/edk/embedder/platform_handle.h"
#include "mojo/edk/embedder/process_delegate.h"
#include "mojo/edk/embedder/scoped_platform_handle.h"

#include "arc/future.h"
#include "common/camera_algorithm_ops_impl.h"

namespace arc {

// This class loads and adapts the functions of camera algorithm. It runs in
// the sandboxed camera algorithm process.

class CameraAlgorithmAdapter : public mojo::edk::ProcessDelegate {
 public:
  CameraAlgorithmAdapter();

  // Build up IPC and load the camera algorithm library. This method returns
  // when the IPC connection is lost.
  void Run(std::string mojo_token,
           mojo::edk::ScopedPlatformHandle channel_handle);

 private:
  void InitializeOnIpcThread(std::string mojo_token,
                             mojo::edk::ScopedPlatformHandle channel_handle);

  void DestroyOnIpcThread();

  void OnShutdownComplete() {}

  // Pointer to CameraAlgorithmOps interface implementation.
  CameraAlgorithmOpsImpl* algo_impl_;

  // Handle of the camera algorithm library.
  void* algo_dll_handle_;

  // Thread for IPC chores
  base::Thread ipc_thread_;

  // Callback to handle IPC channel lost event
  base::Callback<void(void)> ipc_lost_cb_;

  // Store observers for future locks
  internal::CancellationRelay relay_;

  DISALLOW_COPY_AND_ASSIGN(CameraAlgorithmAdapter);
};

}  // namespace arc

#endif  // INCLUDE_ARC_CAMERA_ALGORITHM_ADAPTER_H_
