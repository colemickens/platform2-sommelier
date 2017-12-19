// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_TERMINA_MANAGER_IMPL_H_
#define LOGIN_MANAGER_TERMINA_MANAGER_IMPL_H_

#include "login_manager/job_manager.h"
#include "login_manager/termina_manager_interface.h"

#include <string>

#include <base/files/file_util.h>

namespace login_manager {

class SystemUtils;

// Provides methods for starting and stopping virtual machines.
//
// This class is a thin wrapper for interfacing with the termina_launcher tool
// that manages virtual machines.
class TerminaManagerImpl : public TerminaManagerInterface {
 public:
  explicit TerminaManagerImpl(SystemUtils* system_utils);
  ~TerminaManagerImpl() override = default;

  // TerminaManagerInterface:
  bool StartVmContainer(const base::FilePath& image_path,
                        const std::string& name,
                        bool writable) override;
  bool StopVmContainer(const std::string& name) override;

  // JobManagerInterface:
  bool IsManagedJob(pid_t pid) override;
  void HandleExit(const siginfo_t& status) override;
  void RequestJobExit(const std::string& reason) override;
  void EnsureJobExit(base::TimeDelta timeout) override;

 private:
  // Owned by the caller.
  SystemUtils* const system_utils_;

  DISALLOW_COPY_AND_ASSIGN(TerminaManagerImpl);
};

}  // namespace login_manager

#endif  // LOGIN_MANAGER_TERMINA_MANAGER_IMPL_H_
