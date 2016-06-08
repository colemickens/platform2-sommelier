// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_CONTAINER_MANAGER_INTERFACE_H_
#define LOGIN_MANAGER_CONTAINER_MANAGER_INTERFACE_H_

#include <string>

#include <base/files/file_path.h>

namespace login_manager {

// Provides methods for running and stopping containers.
//
// Containers can only be run from the verified rootfs.
class SessionContainersInterface {
 public:
  virtual ~SessionContainersInterface() {}

  // Starts a container with the given name.
  // Returns true on success.
  virtual bool StartContainer(const std::string& name) = 0;

  // Waits for a running container to exit.
  virtual bool WaitForContainerToExit(const std::string& name) = 0;

  // Kills the container and wait for it to exit.
  virtual bool KillContainer(const std::string& name) = 0;

  // Kills all the running containers and wait for them to exit.
  virtual bool KillAllContainers() = 0;

  // Gets the path of the rootfs of the container with the given name.
  virtual bool GetRootFsPath(const std::string& name,
                             base::FilePath* path_out) const = 0;

  // Gets the process ID of the container with the given name.
  virtual bool GetContainerPID(const std::string& name,
                               pid_t* pid_out) const = 0;
};

}  // namespace login_manager

#endif  // LOGIN_MANAGER_CONTAINER_MANAGER_INTERFACE_H_
