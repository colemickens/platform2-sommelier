// Copyright (c) 2009-2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class is most definitely NOT re-entrant.

#include "login_manager/child_job.h"

#include <errno.h>
#include <grp.h>
#include <pwd.h>
#include <signal.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include <algorithm>
#include <string>
#include <vector>

#include <base/basictypes.h>
#include <base/file_path.h>
#include <base/logging.h>
#include <base/string_util.h>

namespace login_manager {

// static
const int ChildJobInterface::kCantSetUid = 127;
const int ChildJobInterface::kCantSetGid = 128;
const int ChildJobInterface::kCantSetGroups = 129;
const int ChildJobInterface::kCantExec = 255;
const int ChildJobInterface::kBWSI = 1;

// static
const char ChildJob::kLoginManagerFlag[] = "--login-manager";
// static
const char ChildJob::kLoginUserFlag[] = "--login-user=";
// static
const char ChildJob::kBWSIFlag[] = "--bwsi";

// static
const int ChildJob::kRestartWindow = 1;

ChildJob::ChildJob(const std::vector<std::string>& arguments)
      : arguments_(arguments),
        desired_uid_(0),
        is_desired_uid_set_(false),
        last_start_(0),
        removed_login_manager_flag_(false) {
}

ChildJob::~ChildJob() {
}

bool ChildJob::ShouldStop() const {
  return (time(NULL) - last_start_ < kRestartWindow);
}

void ChildJob::RecordTime() {
  time(&last_start_);
}

void ChildJob::Run() {
  // We try to set our UID/GID to the desired UID, and then exec
  // the command passed in.
  int exit_code = SetIDs();
  if (exit_code)
    exit(exit_code);

  char const** argv = CreateArgv();
  int ret = execv(argv[0], const_cast<char* const*>(argv));

  // Should never get here, unless we couldn't exec the command.
  LOG(ERROR) <<
      StringPrintf(
          "Error (%d) executing %s: %s", ret, argv[0], strerror(errno));
  exit(kCantExec);
}

// When user logs in we want to restart chrome in browsing mode with user
// signed in. Hence we remove --login-manager flag and add --login-user
// flag. If it's BWSI mode, we add the corresponding flag as well.
// TODO(avayvod): Flags for jobs that correspond to all processes but
// Chromium should not be modified.
void ChildJob::StartSession(const std::string& email) {
  std::vector<std::string>::iterator to_erase =
      std::remove(arguments_.begin(),
                  arguments_.end(),
                  kLoginManagerFlag);
  if (to_erase != arguments_.end()) {
    arguments_.erase(to_erase, arguments_.end());
    removed_login_manager_flag_ = true;
  }

  arguments_.push_back(kLoginUserFlag);
  arguments_.back().append(email);
}

void ChildJob::StopSession() {
  // The last element for started session is always login user flag.
  arguments_.pop_back();
  if (removed_login_manager_flag_) {
    arguments_.push_back(kLoginManagerFlag);
    removed_login_manager_flag_ = false;
  }
}

uid_t ChildJob::GetDesiredUid() const {
  DCHECK(IsDesiredUidSet());
  return desired_uid_;
}

void ChildJob::SetDesiredUid(uid_t uid) {
  is_desired_uid_set_ = true;
  desired_uid_ = uid;
}

bool ChildJob::IsDesiredUidSet() const {
  return is_desired_uid_set_;
}

const std::string ChildJob::GetName() const {
  FilePath exec_file(arguments_[0]);
  return exec_file.BaseName().value();
}

void ChildJob::SetArguments(const std::string& arguments) {
  std::string argv0;
  if (!arguments_.empty())
    argv0 = arguments_[0];

  arguments_.clear();
  SplitString(arguments, ' ', &arguments_);

  if (!argv0.empty())
    arguments_[0] = argv0;
}

char const** ChildJob::CreateArgv() const {
  // Need to append NULL at the end.
  char const** argv = new char const*[arguments_.size() + 1];
  for (size_t i = 0; i < arguments_.size(); ++i) {
    const std::string& arg = arguments_[i];
    int needed_space = arg.length() + 1;
    char* space = new char[needed_space];
    strncpy(space, arg.c_str(), needed_space);
    argv[i] = space;
  }
  argv[arguments_.size()] = NULL;
  return argv;
}

int ChildJob::SetIDs() {
  int to_return = 0;
  if (IsDesiredUidSet()) {
    struct passwd* entry = getpwuid(GetDesiredUid());
    endpwent();
    if (initgroups(entry->pw_name, entry->pw_gid) == -1)
      to_return = kCantSetGroups;
    if (setgid(entry->pw_gid) == -1)
      to_return = kCantSetGid;
    if (setuid(desired_uid_) == -1)
      to_return = kCantSetUid;
  }
  if (setsid() == -1)
    LOG(ERROR) << "can't setsid: " << strerror(errno);
  return to_return;
}

}  // namespace login_manager
