// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_CONTAINER_MANAGER_INTERFACE_H_
#define LOGIN_MANAGER_CONTAINER_MANAGER_INTERFACE_H_

#include <string>

#include <base/files/file_path.h>

#include "login_manager/job_manager.h"

namespace login_manager {

// Provides methods for running and stopping containers.
//
// Containers can only be run from the verified rootfs.
class ContainerManagerInterface : public JobManagerInterface {
 public:
  virtual ~ContainerManagerInterface() {}

  // Starts the container. Returns true on success.
  virtual bool StartContainer() = 0;

  // Gets the path of the rootfs of the container.
  virtual bool GetRootFsPath(base::FilePath* path_out) const = 0;

  // Gets the process ID of the container.
  virtual bool GetContainerPID(pid_t* pid_out) const = 0;
};

}  // namespace login_manager

#endif  // LOGIN_MANAGER_CONTAINER_MANAGER_INTERFACE_H_
