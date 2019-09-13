// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CROS_DISKS_SANDBOXED_INIT_H_
#define CROS_DISKS_SANDBOXED_INIT_H_

#include <memory>
#include <string>
#include <vector>

#include <sys/types.h>

#include <base/callback.h>
#include <base/files/scoped_file.h>
#include <base/macros.h>

namespace cros_disks {

// To run daemons in a PID namespace under minijail we need to provide
// an "init" process for the sandbox. As we rely on return code of the
// launcher of the daemonized process we must send it through a side
// channel back to the caller without waiting to the whole PID namespace
// to terminate.
class SandboxedInit {
 public:
  SandboxedInit();
  ~SandboxedInit();

  // To be run inside the jail. Never returns.
  void RunInsideSandboxNoReturn(base::OnceCallback<int()> launcher);

  // Returns side channel for reading the exit status of the launcher.
  base::ScopedFD TakeInitControlFD(base::ScopedFD* in_fd,
                                   base::ScopedFD* out_fd,
                                   base::ScopedFD* err_fd);

  static bool PollLauncherStatus(base::ScopedFD* ctrl_fd, int* wstatus);

  static int WStatusToStatus(int wstatus);

 private:
  int RunInitLoop(pid_t root_pid, base::ScopedFD ctrl_fd);
  pid_t StartLauncher(base::OnceCallback<int()> launcher);

  base::ScopedFD fds_[4][2];

  DISALLOW_COPY_AND_ASSIGN(SandboxedInit);
};

}  // namespace cros_disks

#endif  // CROS_DISKS_SANDBOXED_INIT_H_
