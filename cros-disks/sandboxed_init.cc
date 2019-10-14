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

SubprocessPipe::SubprocessPipe(const Direction direction) {
  int fds[2];
  PCHECK(pipe(fds) >= 0);
  child_fd.reset(fds[1 - direction]);
  parent_fd.reset(fds[direction]);
  PCHECK(base::SetCloseOnExec(parent_fd.get()));
}

SandboxedInit::SandboxedInit()
    : in_(SubprocessPipe::kParentToChild),
      out_(SubprocessPipe::kChildToParent),
      err_(SubprocessPipe::kChildToParent),
      ctrl_(SubprocessPipe::kChildToParent) {}

SandboxedInit::~SandboxedInit() = default;

base::ScopedFD SandboxedInit::TakeInitControlFD(base::ScopedFD* in_fd,
                                                base::ScopedFD* out_fd,
                                                base::ScopedFD* err_fd) {
  // Close "child" sides of the pipes. For the outside of sandbox
  // "their" stdin is for writing and all other are for reading.
  in_.child_fd.reset();
  out_.child_fd.reset();
  err_.child_fd.reset();
  ctrl_.child_fd.reset();

  *in_fd = std::move(in_.parent_fd);
  *out_fd = std::move(out_.parent_fd);
  *err_fd = std::move(err_.parent_fd);

  return std::move(ctrl_.parent_fd);
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
  if (dup2(err_.child_fd.get(), STDERR_FILENO) < 0) {
    PLOG(FATAL) << "Can't dup2 " << STDERR_FILENO;
  }
  if (dup2(out_.child_fd.get(), STDOUT_FILENO) < 0) {
    PLOG(FATAL) << "Can't dup2 " << STDOUT_FILENO;
  }
  if (dup2(in_.child_fd.get(), STDIN_FILENO) < 0) {
    PLOG(FATAL) << "Can't dup2 " << STDIN_FILENO;
  }

  // Set an identifiable process name.
  if (prctl(PR_SET_NAME, "cros-disks-INIT") < 0) {
    PLOG(WARNING) << "Can't set init's process name";
  }

  // Close unused sides of the pipes.
  for (SubprocessPipe* const p : {&in_, &out_, &err_}) {
    p->child_fd.reset();
    p->parent_fd.reset();
  }

  ctrl_.parent_fd.reset();

  // Avoid leaking file descriptor into launcher process.
  PCHECK(base::SetCloseOnExec(ctrl_.child_fd.get()));

  // PID of the launcher process inside the jail PID namespace (e.g. PID 2).
  pid_t root_pid = StartLauncher(std::move(launcher));
  CHECK_LT(0, root_pid);

  _exit(RunInitLoop(root_pid, std::move(ctrl_.child_fd)));
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

  // By now it's unlikely something to go wrong here, so disconnect
  // from in/out.
  HANDLE_EINTR(close(STDIN_FILENO));
  HANDLE_EINTR(close(STDOUT_FILENO));
  HANDLE_EINTR(close(STDERR_FILENO));

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
        CHECK(!ctrl_fd.is_valid());
        return last_failure_code;
      }

      PLOG(FATAL) << "Cannot wait for child processes";
    }

    // Convert wait status to exit code.
    const int exit_code = WStatusToStatus(wstatus);
    if (exit_code >= 0) {
      // A child process finished.
      if (exit_code) {
        last_failure_code = exit_code;
      }

      // Was it the launcher?
      if (pid == root_pid) {
        // Write the launcher's exit code to the control pipe.
        const ssize_t written =
            HANDLE_EINTR(write(ctrl_fd.get(), &exit_code, sizeof(exit_code)));
        if (written != sizeof(exit_code)) {
          PLOG(ERROR) << "Cannot write exit code";
          return MINIJAIL_ERR_INIT;
        }

        ctrl_fd.reset();
      }
    }
  }
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

bool SandboxedInit::PollLauncherStatus(base::ScopedFD* ctrl_fd,
                                       int* exit_code) {
  CHECK(ctrl_fd->is_valid());
  ssize_t read_bytes =
      HANDLE_EINTR(read(ctrl_fd->get(), exit_code, sizeof(*exit_code)));
  if (read_bytes != sizeof(*exit_code)) {
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
