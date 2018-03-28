/*
 * Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef COMMON_CAMERA_MOJO_CHANNEL_MANAGER_IMPL_H_
#define COMMON_CAMERA_MOJO_CHANNEL_MANAGER_IMPL_H_

#include <base/synchronization/lock.h>
#include <base/threading/thread.h>
#include <mojo/edk/embedder/process_delegate.h>

#include "cros-camera/camera_mojo_channel_manager.h"
#include "mojo/cros_camera_service.mojom.h"

namespace cros {

class CameraMojoChannelManagerImpl : public CameraMojoChannelManager,
                                     public mojo::edk::ProcessDelegate {
 public:
  CameraMojoChannelManagerImpl();

  ~CameraMojoChannelManagerImpl() final;

  bool Start();

  // Creates a new JpegDecodeAccelerator.
  // This API uses CameraHalDispatcher to pass |request| to another process to
  // create Mojo channel.
  void CreateJpegDecodeAccelerator(
      mojom::JpegDecodeAcceleratorRequest request) final;

  // Create a new CameraAlgorithmOpsPtr.
  // This API uses domain socket to connect to the Algo adapter as a parent to
  // create Mojo channel, and then return mojom::CameraAlgorithmOpsPtr.
  mojom::CameraAlgorithmOpsPtr CreateCameraAlgorithmOpsPtr() final;

  // Handle IPC shutdown completion
  void OnShutdownComplete() final {}

 private:
  // Ensure camera dispatcher Mojo channel connected.
  // It should be called for any public API that needs |dispatcher_|.
  void EnsureDispatcherConnectedOnIpcThread();

  void CreateJpegDecodeAcceleratorOnIpcThread(
      mojom::JpegDecodeAcceleratorRequest request);

  // Error handler for camera dispatcher Mojo channel.
  void OnDispatcherError();

  // Thread for IPC chores.
  base::Thread ipc_thread_;

  // The Mojo channel to CameraHalDispatcher in Chrome. All the Mojo
  // communication to |dispatcher_| happens on |ipc_thread_|.
  mojom::CameraHalDispatcherPtr dispatcher_;

  // A flag to indicate if someone called Start() before.
  bool is_started_;

  // A mutex to guard Start() operation.
  base::Lock start_lock_;

  DISALLOW_COPY_AND_ASSIGN(CameraMojoChannelManagerImpl);
};

}  // namespace cros
#endif  // COMMON_CAMERA_MOJO_CHANNEL_MANAGER_IMPL_H_
