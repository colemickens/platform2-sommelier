// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cros-disks/sandboxed_init.h"

#include <utility>
#include <stdlib.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <unistd.h>

#include <base/files/file_util.h>
#include <base/logging.h>
#include <brillo/syslog_logging.h>
#include <chromeos/libminijail.h>

namespace cros_disks {

namespace {
constexpr size_t kCtrlFileIndex = 3;
constexpr size_t kReadEnd = 0;
constexpr size_t kWriteEnd = 1;

void SigTerm(int sig) {
  if (kill(-1, sig) == -1) {
    RAW_LOG(FATAL, "Can't broadcast signal");
    _exit(errno + 128);
  }
}

}  // namespace

SandboxedInit::SandboxedInit() {
  for (size_t i = STDIN_FILENO; i <= kCtrlFileIndex; ++i) {
    int p[2];
    if (pipe(p) == -1) {
      PLOG(FATAL) << "Can't create pipe " << i;
    }
    fds_[i][0].reset(p[0]);
    fds_[i][1].reset(p[1]);
  }
}

SandboxedInit::~SandboxedInit() = default;

base::ScopedFD SandboxedInit::TakeInitControlFD(base::ScopedFD* in_fd,
                                                base::ScopedFD* out_fd,
                                                base::ScopedFD* err_fd) {
  // Close "other" sides of the pipes. For the outside of sandbox
  // "their" stdin is for writing and all other are for reading.
  fds_[STDIN_FILENO][kReadEnd].reset();
  fds_[STDOUT_FILENO][kWriteEnd].reset();
  fds_[STDERR_FILENO][kWriteEnd].reset();
  fds_[kCtrlFileIndex][kWriteEnd].reset();

  *in_fd = std::move(fds_[STDIN_FILENO][kWriteEnd]);
  *out_fd = std::move(fds_[STDOUT_FILENO][kReadEnd]);
  *err_fd = std::move(fds_[STDERR_FILENO][kReadEnd]);

  return std::move(fds_[kCtrlFileIndex][kReadEnd]);
}

void SandboxedInit::RunInsideSandboxNoReturn(base::Callback<int()> launcher) {
  // To run our custom init that handles daemonized processes inside the
  // sandbox we have to set up fork/exec ourselves. We do error-handling
  // the "minijail-style" - abort if something not right.

  // This performs as the init process in the jail PID namespace (PID 1).
  // Redirect in/out so logging can communicate assertions and children
  // to inherit right FDs.
  brillo::InitLog(brillo::kLogToSyslog | brillo::kLogToStderr);
  if (dup2(fds_[STDERR_FILENO][kWriteEnd].get(), STDERR_FILENO) !=
      STDERR_FILENO) {
    PLOG(FATAL) << "Can't dup2 " << STDERR_FILENO;
  }
  if (dup2(fds_[STDOUT_FILENO][kWriteEnd].get(), STDOUT_FILENO) !=
      STDOUT_FILENO) {
    PLOG(FATAL) << "Can't dup2 " << STDOUT_FILENO;
  }
  if (dup2(fds_[STDIN_FILENO][kReadEnd].get(), STDIN_FILENO) != STDIN_FILENO) {
    PLOG(FATAL) << "Can't dup2 " << STDIN_FILENO;
  }

  // Set an identifyable process name.
  if (prctl(PR_SET_NAME, "cros-disks-INIT") == -1) {
    PLOG(WARNING) << "Can't set init's process name";
  }

  // Close unused sides of the pipes.
  for (int i = STDIN_FILENO; i <= STDERR_FILENO; ++i) {
    fds_[i][kReadEnd].reset();
    fds_[i][kWriteEnd].reset();
  }
  fds_[kCtrlFileIndex][kReadEnd].reset();

  // PID of the launcher process inside the jail PID namespace (e.g. PID 2).
  pid_t root_pid = StartLauncher(std::move(launcher));
  CHECK_LT(0, root_pid);

  // By now it's unlikely something to go wrong here, so disconnect
  // from in/out.
  HANDLE_EINTR(close(STDIN_FILENO));
  HANDLE_EINTR(close(STDOUT_FILENO));
  HANDLE_EINTR(close(STDERR_FILENO));

  _exit(RunInitLoop(root_pid, std::move(fds_[kCtrlFileIndex][kWriteEnd])));
  NOTREACHED();
}

int SandboxedInit::RunInitLoop(pid_t root_pid, base::ScopedFD ctrl_fd) {
  CHECK(base::SetNonBlocking(ctrl_fd.get()));

  // Most of this is mirroring minijail's embedded "init" (exit status handling)
  // with addition of piping the "root" status code to the calling process.

  // Forward SIGTERM to all children instead of handling it directly.
  if (signal(SIGTERM, SigTerm) == SIG_ERR) {
    PLOG(FATAL) << "Can't install signal handler";
  }

  // This loop will only end when either there are no processes left inside
  // our PID namespace or we get a signal.
  pid_t pid;
  int wstatus;
  int last_failure_code = 0;

  // Wait for any child to terminate.
  while ((pid = wait(&wstatus)) > 0) {
    if (WIFEXITED(wstatus) || WIFSIGNALED(wstatus)) {
      // Something quit.
      const int exit_code = WStatusToStatus(wstatus);
      if (exit_code) {
        last_failure_code = exit_code;
      }

      // Check if that's the launcher.
      if (pid == root_pid) {
        ssize_t written =
            HANDLE_EINTR(write(ctrl_fd.get(), &wstatus, sizeof(wstatus)));
        if (written != sizeof(wstatus)) {
          PLOG(ERROR) << "Failed to write exit code";
          return MINIJAIL_ERR_INIT;
        }
        ctrl_fd.reset();

        // Mark launcher finished.
        root_pid = -1;
      }
    }
  }

  if (root_pid != -1) {
    return MINIJAIL_ERR_INIT;
  }
  return last_failure_code;
}

pid_t SandboxedInit::StartLauncher(base::Callback<int()> launcher) {
  pid_t exec_child = fork();

  if (exec_child == -1) {
    PLOG(FATAL) << "Can't fork";
  }

  if (exec_child == 0) {
    // Launch the invoked program.
    exit(std::move(launcher).Run());
    NOTREACHED();
  }

  return exec_child;
}

bool SandboxedInit::PollLauncherStatus(base::ScopedFD* ctrl_fd, int* wstatus) {
  CHECK(ctrl_fd->is_valid());
  ssize_t read_bytes =
      HANDLE_EINTR(read(ctrl_fd->get(), wstatus, sizeof(*wstatus)));
  if (read_bytes == sizeof(*wstatus)) {
    ctrl_fd->reset();
  }
  return read_bytes == sizeof(*wstatus);
}

int SandboxedInit::WStatusToStatus(int wstatus) {
  if (WIFEXITED(wstatus)) {
    return WEXITSTATUS(wstatus);
  } else if (WIFSIGNALED(wstatus)) {
    // Mirrors minijail_wait().
    return 128 + WTERMSIG(wstatus);
  }
  return -1;
}

}  // namespace cros_disks
