// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>
#include <sys/capability.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <base/logging.h>
#include <brillo/syslog_logging.h>
#include <libminijail.h>
#include <scoped_minijail.h>

namespace {

// Sets up the minijail sandbox.
//
// crash_sender currently needs to run as root:
// - System crash reports in /var/spool/crash are owned by root.
// - User crash reports in /home/chronos/ are owned by chronos.
//
// crash_sender needs network access in order to upload things.
//
void SetUpSandbox(struct minijail* jail) {
  // Keep CAP_DAC_OVERRIDE in order to access non-root paths.
  minijail_use_caps(jail, CAP_TO_MASK(CAP_DAC_OVERRIDE));
  // Set ambient capabilities because crash_sender runs other programs.
  // TODO(satorux): Remove this once the code is entirely C++.
  minijail_set_ambient_caps(jail);
  minijail_no_new_privs(jail);
  minijail_namespace_ipc(jail);
  minijail_namespace_pids(jail);
  minijail_remount_proc_readonly(jail);
  minijail_namespace_vfs(jail);
  minijail_mount_tmp(jail);
  minijail_namespace_uts(jail);
  minijail_forward_signals(jail);
}

}  // namespace

int main(int argc, char* argv[]) {
  // Log to syslog (/var/log/messages), and stderr if stdin is a tty.
  brillo::InitLog(brillo::kLogToSyslog | brillo::kLogToStderrIfTty);

  // Set up a sandbox, and jail the child process.
  ScopedMinijail jail(minijail_new());
  SetUpSandbox(jail.get());
  const pid_t pid = minijail_fork(jail.get());

  if (pid == 0) {
    // Child process.
    char shell_script_path[] = "/sbin/crash_sender.sh";
    argv[0] = shell_script_path;
    execve(argv[0], argv, environ);
    return EXIT_FAILURE;  // execve() failed.
  }

  // We rely on the child handling its own exit status, and a non-zero status
  // isn't necessarily a bug (e.g. if mocked out that way).  Only warn for an
  // internal error.
  const int status = minijail_wait(jail.get());
  LOG_IF(ERROR, status < 0)
      << "Child process " << pid << " did not finish cleanly: " << status;
  return status;
}
