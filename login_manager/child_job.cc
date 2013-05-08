// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class is most definitely NOT re-entrant.

#include "login_manager/child_job.h"

#include <errno.h>
#include <glib.h>
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

#include "login_manager/system_utils.h"

namespace login_manager {

// static
const int ChildJobInterface::kCantSetUid = 127;
const int ChildJobInterface::kCantSetGid = 128;
const int ChildJobInterface::kCantSetGroups = 129;
const int ChildJobInterface::kCantExec = 255;

// static
const char ChildJob::kLoginManagerFlag[] = "--login-manager";
// static
const char ChildJob::kLoginUserFlag[] = "--login-user=";
// static
const char ChildJob::kLoginProfileFlag[] = "--login-profile=";
// static
const char ChildJob::kMultiProfileFlag[] = "--multi-profiles";

// static
const int ChildJob::kRestartWindow = 1;

ChildJob::ChildJob(const std::vector<std::string>& arguments,
                   bool support_multi_profile,
                   SystemUtils* utils)
      : arguments_(arguments),
        desired_uid_(0),
        is_desired_uid_set_(false),
        system(utils),
        last_start_(0),
        removed_login_manager_flag_(false),
        session_already_started_(false),
        support_multi_profile_(support_multi_profile) {
  if (support_multi_profile_)
    arguments_.push_back(kMultiProfileFlag);

  // Take over managing the kLoginManagerFlag.
  std::vector<std::string>::iterator to_erase = std::remove(arguments_.begin(),
                                                            arguments_.end(),
                                                            kLoginManagerFlag);
  if (to_erase != arguments_.end()) {
    arguments_.erase(to_erase, arguments_.end());
    removed_login_manager_flag_ = true;
    login_arguments_.push_back(kLoginManagerFlag);
  }
}

ChildJob::~ChildJob() {
}

bool ChildJob::ShouldStop() const {
  return (system->time(NULL) - last_start_ < kRestartWindow);
}

void ChildJob::RecordTime() {
  last_start_ = system->time(NULL);
}

void ChildJob::Run() {
  // We try to set our UID/GID to the desired UID, and then exec
  // the command passed in.
  int exit_code = SetIDs();
  if (exit_code)
    exit(exit_code);

  logging::CloseLogFile();  // So child does not inherit logging FD.

  char const** argv = CreateArgv();
  execv(argv[0], const_cast<char* const*>(argv));

  // Should never get here, unless we couldn't exec the command.
  PLOG(ERROR) << "Error executing " << argv[0];
  exit(kCantExec);
}

// When user logs in we want to restart chrome in browsing mode with
// user signed in. Hence we remove --login-manager flag and add
// --login-user and --login-profile flags.
// When supporting multiple simultaneous accounts, |userhash| is passed
// as the value for the latter.  When not, then the magic string "user"
// is passed.
// Chrome requires that we always pass the email (and maybe hash) of the
// first user who started a session, so this method handles that as well.
void ChildJob::StartSession(const std::string& email,
                            const std::string& userhash) {
  if (!session_already_started_) {
    login_arguments_.clear();
    login_arguments_.push_back(kLoginUserFlag);
    login_arguments_.back().append(email);
    login_arguments_.push_back(kLoginProfileFlag);
    if (support_multi_profile_)
      login_arguments_.back().append(userhash);
    else
      login_arguments_.back().append("user");
  }
  session_already_started_ = true;
}

void ChildJob::StopSession() {
  login_arguments_.clear();
  if (removed_login_manager_flag_) {
    login_arguments_.push_back(kLoginManagerFlag);
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

void ChildJob::SetArguments(const std::vector<std::string>& arguments) {
  // Ensure we preserve the program name to be executed, if we have one.
  std::string argv0;
  if (!arguments_.empty())
    argv0 = arguments_[0];

  arguments_ = arguments;

  if (!argv0.empty()) {
    if (arguments_.size())
      arguments_[0] = argv0;
    else
      arguments_.push_back(argv0);
  }
}

void ChildJob::SetExtraArguments(const std::vector<std::string>& arguments) {
  extra_arguments_ = arguments;
}

void ChildJob::AddOneTimeArgument(const std::string& argument) {
  extra_one_time_argument_ = argument;
}

void ChildJob::ClearOneTimeArgument() {
  extra_one_time_argument_.clear();
}

std::vector<std::string> ChildJob::ExportArgv() {
  std::vector<std::string> to_return;
  char const** argv = CreateArgv();
  for (size_t i = 0; argv[i]; ++i) {
    to_return.push_back(argv[i]);
    delete [] argv[i];
  }
  delete [] argv;
  return to_return;
}

size_t ChildJob::CopyArgsToArgv(const std::vector<std::string>& arguments,
                                char const** argv) const {
  for (size_t i = 0; i < arguments.size(); ++i) {
    size_t needed_space = arguments[i].length() + 1;
    char* space = new char[needed_space];
    strncpy(space, arguments[i].c_str(), needed_space);
    argv[i] = space;
  }
  return arguments.size();
}

char const** ChildJob::CreateArgv() const {
  size_t total_size = (arguments_.size() +
                       login_arguments_.size() +
                       extra_arguments_.size());
  if (!extra_one_time_argument_.empty())
    total_size++;

  char const** argv = new char const*[total_size + 1];
  size_t index = CopyArgsToArgv(arguments_, argv);
  index += CopyArgsToArgv(login_arguments_, argv + index);
  index += CopyArgsToArgv(extra_arguments_, argv + index);
  if (!extra_one_time_argument_.empty()) {
    std::vector<std::string> one_time_argument;
    one_time_argument.push_back(extra_one_time_argument_);
    CopyArgsToArgv(one_time_argument, argv + index);
  }
  // Need to append NULL at the end.
  argv[total_size] = NULL;
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
