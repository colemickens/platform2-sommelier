// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/subprocess.h"

#include <algorithm>
#include <memory>
#include <vector>

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include <base/logging.h>
#include <base/posix/file_descriptor_shuffle.h>
#include <base/process/launch.h>
#include <base/time/time.h>

#include "login_manager/session_manager_service.h"
#include "login_manager/system_utils.h"

namespace login_manager {

Subprocess::Subprocess(uid_t uid, SystemUtils* system)
    : pid_(-1),
      desired_uid_(uid),
      new_mount_namespace_(false),
      system_(system) {}

Subprocess::~Subprocess() {}

void Subprocess::UseNewMountNamespace() {
  new_mount_namespace_ = true;
}

// The reason that this method looks complex is because it's doing a
// bunch of work to keep the code between fork() and exec/exit simple
// and mostly async signal safe. This is because using fork() in a
// multithreaded process can create a child with inconsistent state
// (e.g. locks held by other threads remain locked). While glibc
// generally handles this gracefully internally, other libs are not as
// reliable (including base).
bool Subprocess::ForkAndExec(const std::vector<std::string>& args,
                             const std::vector<std::string>& env_vars) {
  gid_t gid = 0;
  std::vector<gid_t> groups;
  if (desired_uid_ != 0 &&
      !system_->GetGidAndGroups(desired_uid_, &gid, &groups)) {
    LOG(ERROR) << "Can't get group info for " << desired_uid_;
    return false;
  }

  std::unique_ptr<char const* []> argv(new char const*[args.size() + 1]);
  for (size_t i = 0; i < args.size(); ++i)
    argv[i] = args[i].c_str();
  argv[args.size()] = 0;

  std::unique_ptr<char const* []> envp(new char const*[env_vars.size() + 1]);
  for (size_t i = 0; i < env_vars.size(); ++i)
    envp[i] = env_vars[i].c_str();
  envp[env_vars.size()] = 0;

  // The browser should not inherit FDs other than stdio, stdin and stdout,
  // including the logging FD. base::CloseSuperfluousFds() can do this, but
  // it takes a map of FDs to keep open, and creating this map requires
  // allocating memory in a way which is not safe to do after forking, so do it
  // up here in the parent.
  base::InjectiveMultimap saved_fds;
  saved_fds.push_back(base::InjectionArc(STDIN_FILENO, STDIN_FILENO, false));
  saved_fds.push_back(base::InjectionArc(STDOUT_FILENO, STDOUT_FILENO, false));
  saved_fds.push_back(base::InjectionArc(STDERR_FILENO, STDERR_FILENO, false));

  // Block all signals before the fork so that we can avoid a race in which the
  // child executes configured signal handlers before the default handlers are
  // installed, below. In the parent, we restore original signal blocks
  // immediately after fork.
  sigset_t new_sigset, old_sigset;
  sigfillset(&new_sigset);
  CHECK_EQ(0, sigprocmask(SIG_SETMASK, &new_sigset, &old_sigset));
  pid_t fork_ret = system_->fork();
  if (fork_ret == 0) {
    // Reset signal handlers to default and masks to none per 'man 7 daemon'.
    struct sigaction action = {};
    action.sa_handler = SIG_DFL;

    for (int i = 1; i < _NSIG; i++) {
      // Don't check rvalue because some signals can't have handlers (e.g.
      // KILL).
      sigaction(i, &action, nullptr);
    }

    CHECK_EQ(0, sigprocmask(SIG_UNBLOCK, &new_sigset, nullptr));

    if (new_mount_namespace_)
      CHECK(system_->EnterNewMountNamespace());

    // We try to set our UID/GID to the desired UID, and then exec
    // the command passed in.
    if (desired_uid_ != 0) {
      int exit_code = system_->SetIDs(desired_uid_, gid, groups);
      if (exit_code)
        _exit(exit_code);
    }
    system_->CloseSuperfluousFds(saved_fds);

    if (system_->execve(base::FilePath(argv[0]),
                        const_cast<char* const*>(argv.get()),
                        const_cast<char* const*>(envp.get()))) {
      // Should never get here, unless we couldn't exec the command.
      RAW_LOG(ERROR, "Error executing...");
      RAW_LOG(ERROR, argv[0]);
      _exit(errno == E2BIG ? ChildJobInterface::kCantSetEnv
                           : ChildJobInterface::kCantExec);
    }
    return false;  // To make the compiler happy.
  }
  CHECK_EQ(0, sigprocmask(SIG_SETMASK, &old_sigset, nullptr));
  if (fork_ret > 0) {
    pid_ = fork_ret;
  }
  return pid_.has_value();
}

void Subprocess::KillEverything(int signal) {
  DCHECK(pid_.has_value());
  if (system_->kill(-pid_.value(), desired_uid_, signal) == 0)
    return;

  // If we failed to kill the process group (maybe it doesn't exist yet because
  // the forked process hasn't had a chance to call setsid()), just kill the
  // child directly. If it hasn't called setsid() yet, then it hasn't called
  // setuid() either, so kill it as root instead of as |desired_uid_|.
  system_->kill(pid_.value(), 0, signal);
}

void Subprocess::Kill(int signal) {
  DCHECK(pid_.has_value());
  system_->kill(pid_.value(), desired_uid_, signal);
}

pid_t Subprocess::GetPid() const {
  return pid_.value_or(-1);
}

void Subprocess::ClearPid() {
  pid_.reset();
}

};  // namespace login_manager
