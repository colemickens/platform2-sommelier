// Copyright (c) 2009-2010 The Chromium OS Authors. All rights reserved.
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
#include <base/command_line.h>
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
const char ChildJob::kWindowManagerSuffix[] = "window-manager-session.sh";

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

bool ChildJob::ShouldNeverKill() const {
  if (arguments_.empty())
    return false;

  // Avoid killing the window manager process -- see http://crosbug.com/7901.
  return EndsWith(arguments_[0], kWindowManagerSuffix, true);
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

  logging::CloseLogFile();  // So child does not inherit logging FD.

  char const** argv = CreateArgv();
  int ret = execv(argv[0], const_cast<char* const*>(argv));

  // Should never get here, unless we couldn't exec the command.
  PLOG(ERROR) << "Error executing " << argv[0];
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

  GError *error = NULL;
  gchar **argv = NULL;
  gint argc = 0;
  if (!g_shell_parse_argv(arguments.c_str(), &argc, &argv, &error)) {
    LOG(ERROR) << "Could not parse command: " << error->message;
    g_error_free(error);
    g_strfreev(argv);
    if (!argv0.empty())
      arguments_.push_back(argv0);
    return;
  }
  CommandLine new_command_line(argc, argv);
  arguments_.assign(new_command_line.argv().begin(),
                    new_command_line.argv().end());
  g_strfreev(argv);

  if (!argv0.empty()) {
    if (arguments_.size())
      arguments_[0] = argv0;
    else
      arguments_.push_back(argv0);
  }
}

void ChildJob::SetExtraArguments(const std::vector<std::string>& arguments) {
  extra_arguments_.assign(arguments.begin(), arguments.end());
}

void ChildJob::CopyArgsToArgv(const std::vector<std::string>& arguments,
                              char const** argv) const {
  for (size_t i = 0; i < arguments.size(); ++i) {
    size_t needed_space = arguments[i].length() + 1;
    char* space = new char[needed_space];
    strncpy(space, arguments[i].c_str(), needed_space);
    argv[i] = space;
  }
}

char const** ChildJob::CreateArgv() const {
  size_t total_size = arguments_.size() + extra_arguments_.size();
  char const** argv = new char const*[total_size + 1];
  CopyArgsToArgv(arguments_, argv);
  CopyArgsToArgv(extra_arguments_, argv + arguments_.size());
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
