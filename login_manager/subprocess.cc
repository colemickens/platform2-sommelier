// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/subprocess.h"

#include <algorithm>
#include <memory>
#include <vector>

#include <errno.h>
#include <grp.h>
#include <pwd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sysexits.h>  // For exit code defines (EX__MAX, etc).
#include <unistd.h>

#include <base/logging.h>
#include <base/posix/file_descriptor_shuffle.h>
#include <base/process/launch.h>
#include <base/time/time.h>

#include "login_manager/session_manager_service.h"
#include "login_manager/system_utils.h"

namespace login_manager {

namespace {

bool GetGroupInfo(uid_t uid, gid_t* gid_out, std::vector<gid_t>* groups_out) {
  DCHECK(gid_out);
  DCHECK(groups_out);
  // First, get the pwent for uid, which gives us the related gid and username.
  ssize_t buf_len = std::max(sysconf(_SC_GETPW_R_SIZE_MAX), 16384L);
  passwd pwd_buf = {};
  passwd* pwd = nullptr;
  std::vector<char> buf(buf_len);
  for (int i : {1, 2, 3, 4}) {
    buf_len = buf_len * i;
    buf.resize(buf_len);
    if (getpwuid_r(uid, &pwd_buf, buf.data(), buf_len, &pwd) == 0 ||
        errno != ERANGE) {
      break;
    }
  }
  if (!pwd) {
    PLOG(ERROR) << "Unable to find user " << uid;
    return false;
  }
  *gid_out = pwd->pw_gid;

  // Now, use the gid and username to find the list of all uid's groups.
  // Calling getgrouplist() with ngroups=0 causes it to set ngroups to the
  // number of groups available for the given username, including the provided
  // gid. So do that first, then reserve the right amount of space in
  // groups_out, then call getgrouplist() for realz.
  int ngroups = 0;
  CHECK_EQ(getgrouplist(pwd->pw_name, pwd->pw_gid, nullptr, &ngroups), -1);
  groups_out->resize(ngroups, pwd->pw_gid);
  int actual_ngroups =
      getgrouplist(pwd->pw_name, pwd->pw_gid, groups_out->data(), &ngroups);
  if (actual_ngroups == -1) {
    PLOG(ERROR) << "Even after querying number of groups, still failed!";
    return false;
  } else if (actual_ngroups < ngroups) {
    LOG(WARNING) << "Oddly, found fewer groups than initial call to"
                 << "getgrouplist() indicated.";
    groups_out->resize(actual_ngroups);
  }
  return true;
}

// This function will:
// 1) try to setgid to |gid|
// 2) try to setgroups to |gids|
// 3) try to setuid to |uid|
// 4) try to make a new session, with the current process as leader.
//
// Returns 0 on success, the appropriate exit code (defined above) if a
// call fails.
int SetIDs(uid_t uid, gid_t gid, const std::vector<gid_t>& gids) {
  if (setgroups(gids.size(), gids.data()) == -1)
    return ChildJobInterface::kCantSetGroups;
  if (setgid(gid) == -1)
    return ChildJobInterface::kCantSetGid;
  if (setuid(uid) == -1)
    return ChildJobInterface::kCantSetUid;
  if (setsid() == -1)
    RAW_LOG(ERROR, "Can't setsid");
  return 0;
}

}  // namespace

Subprocess::Subprocess(uid_t desired_uid, SystemUtils* system)
    : pid_(-1), desired_uid_(desired_uid), system_(system) {}

Subprocess::~Subprocess() {}

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
  if (desired_uid_ != 0 && !GetGroupInfo(desired_uid_, &gid, &groups)) {
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
  pid_ = system_->fork();
  if (pid_ == 0) {
    // Reset signal handlers to default and masks to none per 'man 7 daemon'.
    struct sigaction action = {};
    action.sa_handler = SIG_DFL;

    for (int i = 1; i < _NSIG; i++) {
      // Don't check rvalue because some signals can't have handlers (e.g.
      // KILL).
      sigaction(i, &action, nullptr);
    }

    CHECK_EQ(0, sigprocmask(SIG_UNBLOCK, &new_sigset, nullptr));

    // We try to set our UID/GID to the desired UID, and then exec
    // the command passed in.
    if (desired_uid_ != 0) {
      int exit_code = SetIDs(desired_uid_, gid, groups);
      if (exit_code)
        _exit(exit_code);
    }
    base::CloseSuperfluousFds(saved_fds);

    execve(argv[0], const_cast<char* const*>(argv.get()),
           const_cast<char* const*>(envp.get()));

    // Should never get here, unless we couldn't exec the command.
    RAW_LOG(ERROR, "Error executing...");
    RAW_LOG(ERROR, argv[0]);
    _exit(errno == E2BIG ? ChildJobInterface::kCantSetEnv
                         : ChildJobInterface::kCantExec);
    return false;  // To make the compiler happy.
  }
  CHECK_EQ(0, sigprocmask(SIG_SETMASK, &old_sigset, nullptr));
  return pid_ > 0;
}

void Subprocess::KillEverything(int signal) {
  DCHECK_GT(pid_, 0);
  if (system_->kill(-pid_, desired_uid_, signal) == 0)
    return;

  // If we failed to kill the process group (maybe it doesn't exist yet because
  // the forked process hasn't had a chance to call setsid()), just kill the
  // child directly. If it hasn't called setsid() yet, then it hasn't called
  // setuid() either, so kill it as root instead of as |desired_uid_|.
  system_->kill(pid_, 0, signal);
}

void Subprocess::Kill(int signal) {
  DCHECK_GT(pid_, 0);
  system_->kill(pid_, desired_uid_, signal);
}

pid_t Subprocess::GetPid() const {
  return pid_;
}

void Subprocess::ClearPid() {
  pid_ = -1;
}

};  // namespace login_manager
