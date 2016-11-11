/*
 * Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef HAL_ADAPTER_ARC_CAMERA3_SERVICE_PROVIDER_H_
#define HAL_ADAPTER_ARC_CAMERA3_SERVICE_PROVIDER_H_

#include <base/files/scoped_file.h>

namespace arc {

// ArcCamera3ServiceProvider is a simple unix domain socket server. It accepts
// a new connection from container and forks a child process to do mojo
// connection to connect to container. The child process is run as mojo child.
// The child process exits when mojo connection is gone. When upstart stops
// arc-camera, SIGTERM is sent to the process group of the main process and
// all child processes will be killed as well.
class ArcCamera3ServiceProvider {
 public:
  ArcCamera3ServiceProvider();
  ~ArcCamera3ServiceProvider();

  // Create linux domain socket to build an IPC connection. Return -1 when
  // creating socket file failed. Otherwise, return fd for accepted connection.
  int Start();

 private:
  DISALLOW_COPY_AND_ASSIGN(ArcCamera3ServiceProvider);
};

}  // namespace arc

#endif  // HAL_ADAPTER_ARC_CAMERA3_SERVICE_PROVIDER_H_
