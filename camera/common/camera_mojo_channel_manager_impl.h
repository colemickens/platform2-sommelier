/*
 * Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef CAMERA_COMMON_CAMERA_MOJO_CHANNEL_MANAGER_IMPL_H_
#define CAMERA_COMMON_CAMERA_MOJO_CHANNEL_MANAGER_IMPL_H_

#include <base/no_destructor.h>
#include <base/synchronization/lock.h>
#include <base/threading/thread.h>

#include "cros-camera/camera_mojo_channel_manager.h"
#include "mojo/cros_camera_service.mojom.h"

namespace cros {

class CameraMojoChannelManagerImpl : public CameraMojoChannelManager {
 public:
  CameraMojoChannelManagerImpl();
  ~CameraMojoChannelManagerImpl() final;

  // Creates a new MjpegDecodeAccelerator.
  // This API uses CameraHalDispatcher to pass |request| to another process to
  // create Mojo channel.
  void CreateMjpegDecodeAccelerator(
      mojom::MjpegDecodeAcceleratorRequest request) final;

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
  bool InitializeMojoEnv();

  // Ensure camera dispatcher Mojo channel connected.
  // It should be called for any public API that needs |dispatcher_|.
  void EnsureDispatcherConnectedOnIpcThread();

  void CreateMjpegDecodeAcceleratorOnIpcThread(
      mojom::MjpegDecodeAcceleratorRequest request);

  void CreateJpegEncodeAcceleratorOnIpcThread(
      mojom::JpegEncodeAcceleratorRequest request);

  static void TearDownMojoEnv();

  static void TearDownMojoEnvLockedOnThread();

  // Error handler for camera dispatcher Mojo channel.
  static void OnDispatcherError();

  // The Mojo channel to CameraHalDispatcher in Chrome. All the Mojo
  // communication to |dispatcher_| happens on |ipc_thread_|.
  static mojom::CameraHalDispatcherPtr dispatcher_;

  // Thread for IPC chores.
  static base::Thread* ipc_thread_;
  // A mutex to guard static variable.
  static base::NoDestructor<base::Lock> static_lock_;
  static bool mojo_initialized_;

  DISALLOW_COPY_AND_ASSIGN(CameraMojoChannelManagerImpl);
};

}  // namespace cros
#endif  // CAMERA_COMMON_CAMERA_MOJO_CHANNEL_MANAGER_IMPL_H_
