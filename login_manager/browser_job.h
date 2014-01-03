// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_BROWSER_JOB_H_
#define LOGIN_MANAGER_BROWSER_JOB_H_

#include "login_manager/child_job.h"

#include <glib.h>
#include <gtest/gtest.h>
#include <time.h>
#include <unistd.h>

#include <string>
#include <queue>
#include <vector>

#include <base/basictypes.h>
#include <base/file_path.h>

namespace login_manager {

class LoginMetrics;
class SystemUtils;

class BrowserJobInterface : public ChildJobInterface {
 public:
  virtual ~BrowserJobInterface() {}

  // Overridden from ChildJobInterface
  virtual bool RunInBackground() OVERRIDE = 0;
  virtual void KillEverything(int signal,
                              const std::string& message) OVERRIDE = 0;
  virtual void Kill(int signal, const std::string& message) OVERRIDE = 0;
  virtual const std::string GetName() const OVERRIDE = 0;
  virtual pid_t CurrentPid() const OVERRIDE = 0;

  // If ShouldStop() returns true, this means that the parent should tear
  // everything down.
  virtual bool ShouldStop() const = 0;

  // Called when a session is started for a user, to update internal
  // bookkeeping wrt command-line flags.
  virtual void StartSession(const std::string& email,
                            const std::string& userhash) = 0;

  // Called when the session is ended.
  virtual void StopSession() = 0;

  // Sets command line arguments for the job from string vector.
  virtual void SetArguments(const std::vector<std::string>& arguments) = 0;

  // Sets extra command line arguments for the job from a string vector.
  virtual void SetExtraArguments(const std::vector<std::string>& arguments) = 0;

  // Throw away the pid of the currently-tracked browser job.
  virtual void ClearPid() = 0;

  // The flag to pass to chrome to tell it to behave as the login manager.
  static const char kLoginManagerFlag[];

  // The flag to pass to chrome to tell it which user has signed in.
  static const char kLoginUserFlag[];

  // The flag to pass to chrome to tell it the hash of the user who's signed in.
  static const char kLoginProfileFlag[];
};

class BrowserJob : public BrowserJobInterface {
 public:
  BrowserJob(const std::vector<std::string>& arguments,
             bool support_multi_profile,
             uid_t desired_uid,
             SystemUtils* utils);
  virtual ~BrowserJob();

  // Overridden from BrowserJobInterface
  virtual bool RunInBackground() OVERRIDE;
  virtual void KillEverything(int signal, const std::string& message) OVERRIDE;
  virtual void Kill(int signal, const std::string& message) OVERRIDE;
  virtual pid_t CurrentPid() const OVERRIDE { return subprocess_.pid(); }
  virtual bool ShouldStop() const OVERRIDE;
  virtual void StartSession(const std::string& email,
                            const std::string& userhash) OVERRIDE;
  virtual void StopSession() OVERRIDE;
  virtual const std::string GetName() const OVERRIDE;
  virtual void SetArguments(const std::vector<std::string>& arguments) OVERRIDE;
  virtual void SetExtraArguments(
      const std::vector<std::string>& arguments) OVERRIDE;
  virtual void ClearPid() OVERRIDE;

  virtual void set_login_metrics(LoginMetrics* metrics) {
    login_metrics_ = metrics;
  }

  // Stores the current time as the time when the job was started.
  void RecordTime();

  // Export a copy of the current argv.
  std::vector<std::string> ExportArgv();

  // Flag passed to Chrome the first time Chrome is started after the
  // system boots. Not passed when Chrome is restarted after signout.
  static const char kFirstExecAfterBootFlag[];

  // After kRestartTries in kRestartWindowSeconds, the BrowserJob will indicate
  // that it should be stopped.
  static const uint kRestartTries;
  static const time_t kRestartWindowSeconds;

 private:
  // Helper for CreateArgV() that copies a vector of arguments into argv.
  size_t CopyArgsToArgv(const std::vector<std::string>& arguments,
                        char const** argv) const;

  // Allocates and populates array of C strings to pass to exec call.
  char const** CreateArgv() const;

  // Arguments to pass to exec.
  std::vector<std::string> arguments_;

  // Login-related arguments to pass to exec.  Managed wholly by this class.
  std::vector<std::string> login_arguments_;

  // Extra arguments to pass to exec.
  std::vector<std::string> extra_arguments_;

  // Extra one time arguments.
  std::vector<std::string> extra_one_time_arguments_;

  // Wrapper for system library calls. Externally owned.
  SystemUtils* system_;

  // Wrapper for reading/writing metrics. Externally owned.
  LoginMetrics* login_metrics_;

  // FIFO of job-start timestamps. Used to determine if we've restarted too many
  // times too quickly.
  std::queue<time_t> start_times_;

  // Indicates if we removed login manager flag when session started so we
  // add it back when session stops.
  bool removed_login_manager_flag_;

  // Indicates that we already started a session.  Needed because the
  // browser requires us to track the _first_ user to start a session.
  // There is no issue filed to address this.
  bool session_already_started_;

  // Support multi-profile behavior in the browser.  Currently, this means
  // passing a user-hash instead of "user" for --login-profile when restarting
  // the browser.
  bool support_multi_profile_;

  // The subprocess tracked by this job.
  ChildJobInterface::Subprocess subprocess_;

  // Path to a magic file that the browser will read for a termination message
  // when it crashes.
  base::FilePath term_file_;

  FRIEND_TEST(BrowserJobTest, InitializationTest);
  FRIEND_TEST(BrowserJobTest, ShouldStopTest);
  FRIEND_TEST(BrowserJobTest, ShouldNotStopTest);
  FRIEND_TEST(BrowserJobTest, CreateArgv);
  DISALLOW_COPY_AND_ASSIGN(BrowserJob);
};

}  // namespace login_manager

#endif  // LOGIN_MANAGER_BROWSER_JOB_H_
