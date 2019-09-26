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

void SigTerm(int sig) {
  if (kill(-1, sig) == -1) {
    RAW_LOG(FATAL, "Can't broadcast signal");
    _exit(errno + 128);
  }
}

}  // namespace

Pipe::Pipe() {
  int fds[2];
  if (pipe(fds) < 0) {
    PLOG(FATAL) << "Cannot create pipe ";
  }

  read_fd.reset(fds[0]);
  write_fd.reset(fds[1]);
}

SandboxedInit::SandboxedInit() = default;

SandboxedInit::~SandboxedInit() = default;

base::ScopedFD SandboxedInit::TakeInitControlFD(base::ScopedFD* in_fd,
                                                base::ScopedFD* out_fd,
                                                base::ScopedFD* err_fd) {
  // Close "other" sides of the pipes. For the outside of sandbox
  // "their" stdin is for writing and all other are for reading.
  in_.read_fd.reset();
  out_.write_fd.reset();
  err_.write_fd.reset();
  ctrl_.write_fd.reset();

  *in_fd = std::move(in_.write_fd);
  *out_fd = std::move(out_.read_fd);
  *err_fd = std::move(err_.read_fd);

  return std::move(ctrl_.read_fd);
}

[[noreturn]] void SandboxedInit::RunInsideSandboxNoReturn(
    base::OnceCallback<int()> launcher) {
  // To run our custom init that handles daemonized processes inside the
  // sandbox we have to set up fork/exec ourselves. We do error-handling
  // the "minijail-style" - abort if something not right.

  // This performs as the init process in the jail PID namespace (PID 1).
  // Redirect in/out so logging can communicate assertions and children
  // to inherit right FDs.
  brillo::InitLog(brillo::kLogToSyslog | brillo::kLogToStderr);
  if (dup2(err_.write_fd.get(), STDERR_FILENO) < 0) {
    PLOG(FATAL) << "Can't dup2 " << STDERR_FILENO;
  }
  if (dup2(out_.write_fd.get(), STDOUT_FILENO) < 0) {
    PLOG(FATAL) << "Can't dup2 " << STDOUT_FILENO;
  }
  if (dup2(in_.read_fd.get(), STDIN_FILENO) < 0) {
    PLOG(FATAL) << "Can't dup2 " << STDIN_FILENO;
  }

  // Set an identifiable process name.
  if (prctl(PR_SET_NAME, "cros-disks-INIT") < 0) {
    PLOG(WARNING) << "Can't set init's process name";
  }

  // Close unused sides of the pipes.
  for (Pipe* const p : {&in_, &out_, &err_}) {
    p->read_fd.reset();
    p->write_fd.reset();
  }

  ctrl_.read_fd.reset();

  // PID of the launcher process inside the jail PID namespace (e.g. PID 2).
  pid_t root_pid = StartLauncher(std::move(launcher));
  CHECK_LT(0, root_pid);

  // By now it's unlikely something to go wrong here, so disconnect
  // from in/out.
  HANDLE_EINTR(close(STDIN_FILENO));
  HANDLE_EINTR(close(STDOUT_FILENO));
  HANDLE_EINTR(close(STDERR_FILENO));

  _exit(RunInitLoop(root_pid, std::move(ctrl_.write_fd)));
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
  int last_failure_code = 0;

  while (true) {
    // Wait for any child to terminate.
    int wstatus;
    const pid_t pid = HANDLE_EINTR(wait(&wstatus));

    if (pid < 0) {
      if (errno == ECHILD) {
        // No more child
        break;
      }

      PLOG(FATAL) << "Cannot wait for child processes";
    }

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

pid_t SandboxedInit::StartLauncher(base::OnceCallback<int()> launcher) {
  pid_t exec_child = fork();

  if (exec_child < 0) {
    PLOG(FATAL) << "Can't fork";
  }

  if (exec_child == 0) {
    // In child process
    // Launch the invoked program.
    _exit(std::move(launcher).Run());
    NOTREACHED();
  }

  // In parent process
  return exec_child;
}

bool SandboxedInit::PollLauncherStatus(base::ScopedFD* ctrl_fd, int* wstatus) {
  CHECK(ctrl_fd->is_valid());
  ssize_t read_bytes =
      HANDLE_EINTR(read(ctrl_fd->get(), wstatus, sizeof(*wstatus)));
  if (read_bytes != sizeof(*wstatus)) {
    return false;
  }

  ctrl_fd->reset();
  return true;
}

int SandboxedInit::WStatusToStatus(int wstatus) {
  if (WIFEXITED(wstatus)) {
    return WEXITSTATUS(wstatus);
  }

  if (WIFSIGNALED(wstatus)) {
    // Mirrors behavior of minijail_wait().
    const int signum = WTERMSIG(wstatus);
    return signum == SIGSYS ? MINIJAIL_ERR_JAIL : 128 + signum;
  }

  return -1;
}

}  // namespace cros_disks
