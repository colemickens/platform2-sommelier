// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class is most definitely NOT re-entrant.

#include "login_manager/browser_job.h"

#include <errno.h>
#include <inttypes.h>
#include <signal.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include <algorithm>
#include <deque>
#include <queue>
#include <string>
#include <vector>

#include <base/files/file_path.h>
#include <base/logging.h>
#include <base/macros.h>
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
const uint32_t BrowserJob::kRestartTries = 4;
// static
const time_t BrowserJob::kRestartWindowSeconds = 60;

namespace {

constexpr const char kVmoduleFlag[] = "--vmodule=";
constexpr const char kEnableFeaturesFlag[] = "--enable-features=";
constexpr const char kDisableFeaturesFlag[] = "--disable-features=";

// Erases all occurrences of |arg| within |args|. Returns true if any entries
// were removed or false otherwise.
bool RemoveArgs(std::vector<std::string>* args, const std::string& arg) {
  std::vector<std::string>::iterator new_end =
      std::remove(args->begin(), args->end(), arg);
  if (new_end == args->end())
    return false;

  args->erase(new_end, args->end());
  return true;
}

// Joins the values of all switches in |args| prefixed by |prefix| using
// |separator| and appends a merged version of the switch. If |keep_existing| is
// true, all earlier occurrences of the switch are preserved; otherwise, they
// are removed.
void MergeSwitches(std::vector<std::string>* args,
                   const std::string& prefix,
                   const std::string& separator,
                   bool keep_existing) {
  std::string values;
  auto head = args->begin();
  for (const auto arg : *args) {
    bool match = base::StartsWith(arg, prefix, base::CompareCase::SENSITIVE);
    if (match) {
      if (!values.empty())
        values += separator;
      values += arg.substr(prefix.size());
    }
    if (!match || keep_existing) {
      *head++ = arg;
    }
  }
  if (head != args->end())
    args->erase(head, args->end());
  if (!values.empty())
    args->push_back(prefix + values);
}

}  // namespace

BrowserJob::BrowserJob(
    const std::vector<std::string>& arguments,
    const std::map<std::string, std::string>& environment_variables,
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
      subprocess_(desired_uid, system_) {
  // Convert map of env vars into a vector of strings.
  for (const auto& it : environment_variables)
    environment_variables_.push_back(it.first + "=" + it.second);

  // Take over managing kLoginManagerFlag.
  if (RemoveArgs(&arguments_, kLoginManagerFlag)) {
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
  if (first_boot)
    extra_one_time_arguments_.push_back(kFirstExecAfterBootFlag);

  const std::vector<std::string> argv(ExportArgv());
  LOG(INFO) << "Running child " << base::JoinString(argv, " ");
  RecordTime();
  return subprocess_.ForkAndExec(argv, environment_variables_);
}

void BrowserJob::KillEverything(int signal, const std::string& message) {
  if (subprocess_.pid() < 0)
    return;

  LOG(INFO) << "Terminating process group: " << message;
  subprocess_.KillEverything(signal);
}

void BrowserJob::Kill(int signal, const std::string& message) {
  if (subprocess_.pid() < 0)
    return;

  LOG(INFO) << "Terminating process: " << message;
  subprocess_.Kill(signal);
}

void BrowserJob::WaitAndAbort(base::TimeDelta timeout) {
  if (subprocess_.pid() < 0)
    return;
  if (!system_->ProcessGroupIsGone(subprocess_.pid(), timeout)) {
    LOG(WARNING) << "Aborting child process " << subprocess_.pid()
                 << "'s process group " << timeout.InSeconds()
                 << " seconds after sending signal";
    std::string message = base::StringPrintf("Browser took more than %" PRId64
                                             " seconds to exit after signal.",
                                             timeout.InSeconds());
    KillEverything(SIGABRT, message);
  } else {
    DLOG(INFO) << "Cleaned up child " << subprocess_.pid();
  }
}

// When user logs in we want to restart chrome in browsing mode with
// user signed in. Hence we remove --login-manager flag and add
// --login-user=|account_id| and --login-profile=|userhash| flags.
void BrowserJob::StartSession(const std::string& account_id,
                              const std::string& userhash) {
  if (!session_already_started_) {
    login_arguments_.clear();
    login_arguments_.push_back(kLoginUserFlag + account_id);
    login_arguments_.push_back(kLoginProfileFlag + userhash);
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

std::vector<std::string> BrowserJob::ExportArgv() const {
  std::vector<std::string> to_return(arguments_.begin(), arguments_.end());
  to_return.insert(to_return.end(), login_arguments_.begin(),
                   login_arguments_.end());
  to_return.insert(to_return.end(), extra_arguments_.begin(),
                   extra_arguments_.end());

  if (!extra_one_time_arguments_.empty()) {
    to_return.insert(to_return.end(), extra_one_time_arguments_.begin(),
                     extra_one_time_arguments_.end());
  }

  // Chrome doesn't support repeated switches. Merge switches containing
  // comma-separated values that may be supplied via multiple sources (e.g.
  // chrome_setup.cc, chrome://flags, Telemetry).
  //
  // --enable-features and --disable-features may be placed within sentinel
  // values (--flag-switches-begin/end, --policy-switches-begin/end). To
  // preserve those positions, keep the existing flags while also appending
  // merged versions at the end of the command line. Chrome will use the final,
  // merged flags: https://crbug.com/767266
  MergeSwitches(&to_return, kVmoduleFlag, ",", false /* keep_existing */);
  MergeSwitches(&to_return, kEnableFeaturesFlag, ",", true /* keep_existing */);
  MergeSwitches(&to_return, kDisableFeaturesFlag, ",",
                true /* keep_existing */);

  return to_return;
}

}  // namespace login_manager
