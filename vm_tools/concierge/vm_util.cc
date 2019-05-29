// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "vm_tools/concierge/vm_util.h"

#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <utility>

#include <base/logging.h>
#include <base/sys_info.h>
#include <brillo/process.h>

namespace vm_tools {
namespace concierge {

std::string GetVmMemoryMiB() {
  int64_t vm_memory_mb = base::SysInfo::AmountOfPhysicalMemoryMB();
  vm_memory_mb /= 4;
  vm_memory_mb *= 3;

  return std::to_string(vm_memory_mb);
}

bool SetPgid() {
  if (setpgid(0, 0) != 0) {
    PLOG(ERROR) << "Failed to change process group id";
    return false;
  }

  return true;
}

bool WaitForChild(pid_t child, base::TimeDelta timeout) {
  sigset_t set;
  sigemptyset(&set);
  sigaddset(&set, SIGCHLD);

  const base::Time deadline = base::Time::Now() + timeout;
  while (true) {
    pid_t ret = waitpid(child, nullptr, WNOHANG);
    if (ret == child || (ret < 0 && errno == ECHILD)) {
      // Either the child exited or it doesn't exist anymore.
      return true;
    }

    // ret == 0 means that the child is still alive
    if (ret < 0) {
      PLOG(ERROR) << "Failed to wait for child process";
      return false;
    }

    base::Time now = base::Time::Now();
    if (deadline <= now) {
      // Timed out.
      return false;
    }

    const struct timespec ts = (deadline - now).ToTimeSpec();
    if (sigtimedwait(&set, nullptr, &ts) < 0 && errno == EAGAIN) {
      // Timed out.
      return false;
    }
  }
}

bool CheckProcessExists(pid_t pid) {
  // kill() with a signal value of 0 is explicitly documented as a way to
  // check for the existence of a process.
  return pid != 0 && (kill(pid, 0) >= 0 || errno != ESRCH);
}

void RunCrosvmCommand(std::string command, std::string socket_path) {
  constexpr char kCrosvmBin[] = "/usr/bin/crosvm";

  brillo::ProcessImpl crosvm;
  crosvm.AddArg(kCrosvmBin);
  crosvm.AddArg(std::move(command));
  crosvm.AddArg(std::move(socket_path));

  // This must be synchronous as we may do things after calling this function
  // that depend on the crosvm command being completed (like suspending the
  // device).
  crosvm.Run();
}

}  // namespace concierge
}  // namespace vm_tools
