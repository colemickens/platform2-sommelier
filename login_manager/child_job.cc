// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/child_job.h"

#include <errno.h>
#include <grp.h>
#include <pwd.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <base/logging.h>
#include <base/time/time.h>
#include <base/memory/scoped_ptr.h>

#include "login_manager/session_manager_service.h"
#include "login_manager/system_utils.h"

namespace login_manager {
const int ChildJobInterface::kCantSetUid = 127;
const int ChildJobInterface::kCantSetGid = 128;
const int ChildJobInterface::kCantSetGroups = 129;
const int ChildJobInterface::kCantExec = 255;

ChildJobInterface::Subprocess::Subprocess(uid_t desired_uid,
                                          SystemUtils* system)
    : pid_(-1),
      desired_uid_(desired_uid),
      system_(system) {
}

ChildJobInterface::Subprocess::~Subprocess() {}

bool ChildJobInterface::Subprocess::ForkAndExec(
    const std::vector<std::string>& args) {
  pid_ = system_->fork();
  if (pid_ == 0) {
    SessionManagerService::RevertHandlers();
    // We try to set our UID/GID to the desired UID, and then exec
    // the command passed in.
    int exit_code = SetIDs();
    if (exit_code)
      exit(exit_code);

    logging::CloseLogFile();  // So browser does not inherit logging FD.

    scoped_ptr<char const*[]> argv(new char const*[args.size() + 1]);
    for (size_t i = 0; i < args.size(); ++i)
      argv[i] = args[i].c_str();
    argv[args.size()] = 0;

    execv(argv[0], const_cast<char* const*>(argv.get()));

    // Should never get here, unless we couldn't exec the command.
    PLOG(ERROR) << "Error executing " << argv[0];
    exit(kCantExec);
  } else if (pid_ < 0) {
    return false;
  }
  return true;
}

void ChildJobInterface::Subprocess::KillEverything(int signal) {
  DCHECK_GT(pid_, 0);
  system_->kill(-pid_, desired_uid_, signal);
}

void ChildJobInterface::Subprocess::Kill(int signal) {
  DCHECK_GT(pid_, 0);
  system_->kill(pid_, desired_uid_, signal);
}

int ChildJobInterface::Subprocess::SetIDs() {
  int to_return = 0;
  struct passwd* entry = getpwuid(desired_uid_);
  endpwent();
  if (initgroups(entry->pw_name, entry->pw_gid) == -1)
    to_return = kCantSetGroups;
  if (setgid(entry->pw_gid) == -1)
    to_return = kCantSetGid;
  if (setuid(desired_uid_) == -1)
    to_return = kCantSetUid;
  if (setsid() == -1)
    LOG(ERROR) << "can't setsid: " << strerror(errno);
  return to_return;
}

};  // namespace login_manager
