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

class MojoShutdownImpl : public mojo::edk::ProcessDelegate {
  // Handle IPC shutdown completion
  void OnShutdownComplete() final {}
};

class CameraMojoChannelManagerImpl : public CameraMojoChannelManager {
 public:
  CameraMojoChannelManagerImpl();
  ~CameraMojoChannelManagerImpl() final;

  // Creates a new JpegDecodeAccelerator.
  // This API uses CameraHalDispatcher to pass |request| to another process to
  // create Mojo channel.
  void CreateJpegDecodeAccelerator(
      mojom::JpegDecodeAcceleratorRequest request) final;

  // Creates a new JpegEncodeAccelerator.
  // This API uses CameraHalDispatcher to pass |request| to another process to
  // create Mojo channel.
  void CreateJpegEncodeAccelerator(
      mojom::JpegEncodeAcceleratorRequest request) final;

  // Create a new CameraAlgorithmOpsPtr.
  // This API uses domain socket to connect to the Algo adapter as a parent to
  // create Mojo channel, and then return mojom::CameraAlgorithmOpsPtr.
  mojom::CameraAlgorithmOpsPtr CreateCameraAlgorithmOpsPtr() final;

 private:
  bool Start();

  // Ensure camera dispatcher Mojo channel connected.
  // It should be called for any public API that needs |dispatcher_|.
  void EnsureDispatcherConnectedOnIpcThread();

  void CreateJpegDecodeAcceleratorOnIpcThread(
      mojom::JpegDecodeAcceleratorRequest request);

  void CreateJpegEncodeAcceleratorOnIpcThread(
      mojom::JpegEncodeAcceleratorRequest request);

  // This function will use the lock protect member |reference_count_| but it
  // won't use |static_lock_| first. It is locked in destructor.
  // It reset |dispatcher_| on |ipc_thread_| thread.
  void DestroyOnIpcThreadLocked();

  // Error handler for camera dispatcher Mojo channel.
  static void OnDispatcherError();

  // The Mojo channel to CameraHalDispatcher in Chrome. All the Mojo
  // communication to |dispatcher_| happens on |ipc_thread_|.
  static mojom::CameraHalDispatcherPtr dispatcher_;

  // Thread for IPC chores.
  static base::Thread* ipc_thread_;
  // A mutex to guard static variable.
  static base::Lock static_lock_;
  static int reference_count_;
  static MojoShutdownImpl* mojo_shutdown_impl_;

  DISALLOW_COPY_AND_ASSIGN(CameraMojoChannelManagerImpl);
};

}  // namespace cros
#endif  // COMMON_CAMERA_MOJO_CHANNEL_MANAGER_IMPL_H_
