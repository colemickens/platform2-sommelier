// Copyright (c) 2009-2010 The Chromium Authors. All rights reserved.
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

#include <base/basictypes.h>
#include <base/command_line.h>
#include <base/logging.h>
#include <base/string_util.h>

namespace login_manager {
// static
const char SetUidExecJob::kLoginManagerFlag[] = "--login-manager";

// static
const int SetUidExecJob::kCantSetuid = 127;
const int SetUidExecJob::kCantSetgid = 128;
const int SetUidExecJob::kCantSetgroups = 129;
const int SetUidExecJob::kCantExec = 255;

// static
const int SetUidExecJob::kRestartWindow = 1;

SetUidExecJob::SetUidExecJob(const CommandLine* command_line,
                             FileChecker* checker,  // Takes ownership.
                             const bool add_flag)
      : checker_(checker),
        argv_(NULL),
        num_args_passed_in_(0),
        desired_uid_(0),
        include_login_flag_(add_flag),
        desired_uid_is_set_(false),
        last_start_(0) {
    PopulateArgv(command_line);
  }

SetUidExecJob::~SetUidExecJob() {
  if (argv_) {
    // free the strings we copied from the passed-in args.
    for (uint32 i = 0; i < num_args_passed_in_; i++) {
      if (argv_[i])
        delete [] argv_[i];
    }
    delete [] argv_;
  }
}

void SetUidExecJob::PopulateArgv(const CommandLine* command_line) {
  // Grab the loose args to use as the command line.
  // We have to wstring->argv[][] manually. Ugh.
  std::vector<std::wstring> loose_wide_args = command_line->GetLooseValues();
  num_args_passed_in_ = loose_wide_args.size();

  // We might need one extra for kLoginManagerFlag, and we'll always
  // need one for NULL.
  int total_args = num_args_passed_in_ + 2;
  argv_ = new char const*[total_args];
  std::vector<std::wstring>::const_iterator arg_it = loose_wide_args.begin();
  char const* *argv_pointer = argv_;
  for (; arg_it != loose_wide_args.end(); ++arg_it) {
    std::string arg = WideToASCII(*arg_it);
    int needed_space = arg.length() + 1;
    char* space = new char[needed_space];
    strncpy(space, arg.c_str(), needed_space);
    *argv_pointer++ = space;
  }
  // Null out the rest of argv_.
  for (int i = num_args_passed_in_; i < total_args; i++) {
    *argv_pointer++ =NULL;
  }
}

void SetUidExecJob::UseLoginManagerFlagIfNeeded() {
  char const* *argv_pointer = argv_ + num_args_passed_in_;
  *argv_pointer++ = include_login_flag_ ? kLoginManagerFlag : NULL;
  *argv_pointer = NULL;
}

std::vector<std::string> SetUidExecJob::ExtractArgvForTest() {
  std::vector<std::string> to_return;
  for (uint32 i = 0; argv_[i] != NULL; i++) {
    to_return.push_back(argv_[i]);
  }
  return to_return;
}

int SetUidExecJob::SetIDs() {
  int to_return = 0;
  if (desired_uid_is_set_) {
    struct passwd* entry = getpwuid(desired_uid_);
    endpwent();
    if (initgroups(entry->pw_name, entry->pw_gid) == -1)
      to_return = kCantSetgroups;
    if (setgid(entry->pw_gid) == -1)
      to_return = kCantSetgid;
    if (setuid(desired_uid_) == -1)
      to_return = kCantSetuid;
  }
  if (setsid() == -1)
    LOG(ERROR) << "can't setsid: " << strerror(errno);
  return to_return;
}

bool SetUidExecJob::ShouldRun() {
  return !checker_->exists();
}

bool SetUidExecJob::ShouldStop() {
  return (time(NULL) - last_start_ < kRestartWindow);
}

void SetUidExecJob::RecordTime() {
  time(&last_start_);
}

void SetUidExecJob::Run() {
  UseLoginManagerFlagIfNeeded();
  // We try to set our UID/GID to the desired UID, and then exec
  // the command passed in.
  int exit_code = SetIDs();
  if (exit_code)
    exit(exit_code);

  execv(argv_[0], const_cast<char * const*>(argv_));

  // Should never get here, unless we couldn't exec the command.
  LOG(ERROR) << strerror(errno);
  exit(kCantExec);
}

}  // namespace login_manager
