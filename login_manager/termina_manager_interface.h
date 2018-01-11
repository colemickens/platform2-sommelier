// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_TERMINA_MANAGER_INTERFACE_H_
#define LOGIN_MANAGER_TERMINA_MANAGER_INTERFACE_H_

#include <string>

#include "login_manager/job_manager.h"

#include <base/files/file_path.h>

namespace login_manager {

// Provides methods for starting and stopping virtual machines.
class TerminaManagerInterface : public JobManagerInterface {
 public:
  ~TerminaManagerInterface() override {}

  // Starts the container image from |image_path| in a termina VM. Give the
  // running vm |name| so it can be referred to later.
  // |image_path| must be a disk image containing a valid open container
  // initiative spec configuration file and the container rootfs.
  // If |writable| is true, the container image will be writable within the VM.
  virtual bool StartVmContainer(const base::FilePath& image_path,
                                const std::string& name,
                                bool writable) = 0;

  // Stops a running VM named |name|.
  virtual bool StopVmContainer(const std::string& name) = 0;
};

}  // namespace login_manager

#endif  // LOGIN_MANAGER_TERMINA_MANAGER_INTERFACE_H_
