// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class is most definitely NOT re-entrant.

#include "login_manager/browser_job.h"

#include <errno.h>
#include <inttypes.h>
#include <signal.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include <algorithm>
#include <deque>
#include <string>
#include <queue>
#include <vector>

#include <base/basictypes.h>
#include <base/files/file_path.h>
#include <base/logging.h>
#include <base/strings/string_util.h>
#include <chromeos/switches/chrome_switches.h>

#include "login_manager/file_checker.h"
#include "login_manager/login_metrics.h"
#include "login_manager/system_utils.h"

namespace login_manager {
// static
const char BrowserJobInterface::kLoginManagerFlag[] = "--login-manager";
// static
const char BrowserJobInterface::kLoginUserFlag[] = "--login-user=";
// static
const char BrowserJobInterface::kLoginProfileFlag[] = "--login-profile=";

// static
const char BrowserJob::kFirstExecAfterBootFlag[] = "--first-exec-after-boot";
// static
const uint BrowserJob::kRestartTries = 4;
// static
const time_t BrowserJob::kRestartWindowSeconds = 60;

BrowserJob::BrowserJob(const std::vector<std::string>& arguments,
                       bool support_multi_profile,
                       uid_t desired_uid,
                       FileChecker* checker,
                       LoginMetrics* metrics,
                       SystemUtils* utils)
      : arguments_(arguments),
        file_checker_(checker),
        login_metrics_(metrics),

        system_(utils),
        start_times_(std::deque<time_t>(kRestartTries, 0)),
        removed_login_manager_flag_(false),
        session_already_started_(false),
        support_multi_profile_(support_multi_profile),
        subprocess_(desired_uid, system_) {
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

BrowserJob::~BrowserJob() {}

bool BrowserJob::ShouldRunBrowser() {
  return !file_checker_ || !file_checker_->exists();
}

bool BrowserJob::ShouldStop() const {
  return (system_->time(NULL) - start_times_.front() < kRestartWindowSeconds);
}

void BrowserJob::RecordTime() {
  start_times_.push(system_->time(NULL));
  start_times_.pop();
  DCHECK(start_times_.size() == kRestartTries);
}

bool BrowserJob::RunInBackground() {
  CHECK(login_metrics_);
  bool first_boot = !login_metrics_->HasRecordedChromeExec();
  login_metrics_->RecordStats("chrome-exec");

  extra_one_time_arguments_.clear();

  system_->RemoveFile(term_file_);
  if (system_->CreateReadOnlyFileInTempDir(&term_file_)) {
    extra_one_time_arguments_.push_back(
        base::StringPrintf("--%s=%s",
                           chromeos::switches::kTerminationMessageFile,
                           term_file_.value().c_str()));
  }
  if (first_boot)
    extra_one_time_arguments_.push_back(kFirstExecAfterBootFlag);

  LOG(INFO) << "Running child " << GetName() << "...";
  RecordTime();
  return subprocess_.ForkAndExec(CreateArgv());
}

void BrowserJob::KillEverything(int signal, const std::string& message) {
  if (subprocess_.pid() < 0)
    return;

  LOG(INFO) << "Sending termination message: " << message;
  system_->AtomicFileWrite(term_file_, message.c_str(), message.size());
  subprocess_.KillEverything(signal);
}

void BrowserJob::Kill(int signal, const std::string& message) {
  if (subprocess_.pid() < 0)
    return;

  LOG(INFO) << "Sending termination message: " << message;
  system_->AtomicFileWrite(term_file_, message.c_str(), message.size());
  subprocess_.Kill(signal);
}

void BrowserJob::WaitAndAbort(base::TimeDelta timeout) {
  if (subprocess_.pid() < 0)
    return;
  if (!system_->ChildIsGone(subprocess_.pid(), timeout)) {
    LOG(WARNING) << "Aborting child process " << subprocess_.pid()
                 << "'s process group " << timeout.InSeconds()
                 << " seconds after sending TERM signal";
    std::string message = base::StringPrintf(
        "Browser took more than %" PRId64 " seconds to exit after TERM.",
        timeout.InSeconds());
    KillEverything(SIGABRT, message);
  } else {
    DLOG(INFO) << "Cleaned up child " << subprocess_.pid();
  }
}

// When user logs in we want to restart chrome in browsing mode with
// user signed in. Hence we remove --login-manager flag and add
// --login-user and --login-profile flags.
// When supporting multiple simultaneous accounts, |userhash| is passed
// as the value for the latter.  When not, then the magic string "user"
// is passed.
// Chrome requires that we always pass the email (and maybe hash) of the
// first user who started a session, so this method handles that as well.
void BrowserJob::StartSession(const std::string& email,
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

void BrowserJob::StopSession() {
  login_arguments_.clear();
  if (removed_login_manager_flag_) {
    login_arguments_.push_back(kLoginManagerFlag);
    removed_login_manager_flag_ = false;
  }
}

const std::string BrowserJob::GetName() const {
  base::FilePath exec_file(arguments_[0]);
  return exec_file.BaseName().value();
}

void BrowserJob::SetArguments(const std::vector<std::string>& arguments) {
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

void BrowserJob::SetExtraArguments(const std::vector<std::string>& arguments) {
  extra_arguments_ = arguments;
}

void BrowserJob::ClearPid() {
  subprocess_.clear_pid();
}

std::vector<std::string> BrowserJob::ExportArgv() {
  std::vector<std::string> to_return;
  char const** argv = CreateArgv();
  for (size_t i = 0; argv[i]; ++i) {
    to_return.push_back(argv[i]);
    delete [] argv[i];
  }
  delete [] argv;
  return to_return;
}

size_t BrowserJob::CopyArgsToArgv(const std::vector<std::string>& arguments,
                                char const** argv) const {
  for (size_t i = 0; i < arguments.size(); ++i) {
    size_t needed_space = arguments[i].length() + 1;
    char* space = new char[needed_space];
    strncpy(space, arguments[i].c_str(), needed_space);
    argv[i] = space;
  }
  return arguments.size();
}

char const** BrowserJob::CreateArgv() const {
  size_t total_size = (arguments_.size() +
                       login_arguments_.size() +
                       extra_arguments_.size());
  if (!extra_one_time_arguments_.empty())
    total_size += extra_one_time_arguments_.size();

  char const** argv = new char const*[total_size + 1];
  size_t index = CopyArgsToArgv(arguments_, argv);
  index += CopyArgsToArgv(login_arguments_, argv + index);
  index += CopyArgsToArgv(extra_arguments_, argv + index);
  if (!extra_one_time_arguments_.empty())
    index += CopyArgsToArgv(extra_one_time_arguments_, argv + index);
  // Need to append NULL at the end.
  argv[total_size] = NULL;
  return argv;
}

}  // namespace login_manager
