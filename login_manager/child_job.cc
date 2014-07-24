// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/child_job.h"

#include <errno.h>
#include <grp.h>
#include <pwd.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>  // For exit code defines (EX__MAX, etc).
#include <unistd.h>

#include <base/logging.h>
#include <base/memory/scoped_ptr.h>
#include <base/time/time.h>

#include "login_manager/session_manager_service.h"
#include "login_manager/system_utils.h"

namespace login_manager {
const int ChildJobInterface::kCantSetUid = EX__MAX + 1;
const int ChildJobInterface::kCantSetGid = EX__MAX + 2;
const int ChildJobInterface::kCantSetGroups = EX__MAX + 3;
const int ChildJobInterface::kCantSetEnv = EX__MAX + 4;
const int ChildJobInterface::kCantExec = EX_OSERR;

ChildJobInterface::Subprocess::Subprocess(uid_t desired_uid,
                                          SystemUtils* system)
    : pid_(-1),
      desired_uid_(desired_uid),
      system_(system) {
}

ChildJobInterface::Subprocess::~Subprocess() {}

bool ChildJobInterface::Subprocess::ForkAndExec(
    const std::vector<std::string>& args,
    const std::map<std::string, std::string>& environment_variables) {
  pid_ = system_->fork();
  if (pid_ == 0) {
    SessionManagerService::RevertHandlers();
    // We try to set our UID/GID to the desired UID, and then exec
    // the command passed in.
    int exit_code = SetIDs();
    if (exit_code)
      exit(exit_code);

    logging::CloseLogFile();  // So browser does not inherit logging FD.

    if (clearenv() != 0) {
      PLOG(ERROR) << "Error clearing environment";
      exit(kCantSetEnv);
    }
    for (std::map<std::string, std::string>::const_iterator it =
             environment_variables.begin();
         it != environment_variables.end(); ++it) {
      if (setenv(it->first.c_str(), it->second.c_str(), 1) != 0) {
        PLOG(ERROR) << "Error exporting " << it->first << "=" << it->second;
        exit(kCantSetEnv);
      }
    }

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
  // Since this program is single-threaded, it's fine to use getpwuid().
  struct passwd* entry = getpwuid(desired_uid_);  // NOLINT
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
