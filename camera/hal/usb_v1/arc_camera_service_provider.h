/*
 * Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef CAMERA_HAL_USB_V1_ARC_CAMERA_SERVICE_PROVIDER_H_
#define CAMERA_HAL_USB_V1_ARC_CAMERA_SERVICE_PROVIDER_H_

#include <base/macros.h>

namespace arc {

// ArcCameraServiceProvider is a simple unix domain socket server. It accepts
// a new connection from container and forks a child process to do mojo
// connection to connect to container. The child process is run as mojo child.
// The child process exits when mojo connection is gone. When upstart stops
// arc-camera, SIGTERM is sent to the process group of the main process and
// all child processes will be killed as well.
class ArcCameraServiceProvider {
 public:
  ArcCameraServiceProvider();
  ~ArcCameraServiceProvider();

  // Create linux domain socket to build an IPC connection. Return -1 when
  // creating socket file failed. Otherwise, return fd for accepted connection.
  int Start();

 private:
  DISALLOW_COPY_AND_ASSIGN(ArcCameraServiceProvider);
};

}  // namespace arc

#endif  // CAMERA_HAL_USB_V1_ARC_CAMERA_SERVICE_PROVIDER_H_
