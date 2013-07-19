// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_CHILD_JOB_H_
#define LOGIN_MANAGER_CHILD_JOB_H_

#include <glib.h>
#include <gtest/gtest.h>
#include <time.h>
#include <unistd.h>

#include <string>
#include <queue>
#include <vector>

#include <base/basictypes.h>
#include <base/memory/scoped_ptr.h>

namespace login_manager {

class SystemUtils;

// SessionManager takes a ChildJob object, forks and then calls the ChildJob's
// Run() method. I've created this interface so that I can create mocks for
// unittesting SessionManager.
class ChildJobInterface {
 public:
  virtual ~ChildJobInterface() {}

  // If ShouldStop() returns true, this means that the parent should tear
  // everything down.
  virtual bool ShouldStop() const = 0;

  // Stores the current time as the time when the job was started.
  virtual void RecordTime() = 0;

  // Wraps up all the logic of what the job is meant to do. Should NOT return.
  virtual void Run() = 0;

  // Called when a session is started for a user, to update internal
  // bookkeeping wrt command-line flags.
  virtual void StartSession(const std::string& email,
                            const std::string& userhash) = 0;

  // Called when the session is ended.
  virtual void StopSession() = 0;

  // Returns the desired UID for the job. -1 means that UID wasn't set.
  virtual uid_t GetDesiredUid() const = 0;

  // Sets the desired UID for the job. This UID (if not equal to -1)  is
  // attempted to be set on job's process before job actually runs.
  virtual void SetDesiredUid(uid_t uid) = 0;

  // Indicates if UID was set for the job.
  virtual bool IsDesiredUidSet() const = 0;

  // Returns the name of the job.
  virtual const std::string GetName() const = 0;

  // Sets command line arguments for the job from string vector.
  virtual void SetArguments(const std::vector<std::string>& arguments) = 0;

  // Sets extra command line arguments for the job from a string vector.
  virtual void SetExtraArguments(const std::vector<std::string>& arguments) = 0;

  // Adds a single extra argument that may be cleared once needed.
  virtual void AddOneTimeArgument(const std::string& argument) = 0;

  // Clears one time extra argument.
  virtual void ClearOneTimeArgument() = 0;

  // Potential exit codes for Run().
  static const int kCantSetUid;
  static const int kCantSetGid;
  static const int kCantSetGroups;
  static const int kCantExec;
};


class ChildJob : public ChildJobInterface {
 public:
  struct Spec {
   public:
    Spec() : job(NULL), pid(-1), watcher(0) {}
    explicit Spec(scoped_ptr<ChildJobInterface> j)
        : job(j.Pass()),
          pid(-1),
          watcher(0) {
    }
    scoped_ptr<ChildJobInterface> job;
    pid_t pid;
    guint watcher;
  };

  ChildJob(const std::vector<std::string>& arguments,
           bool support_multi_profile,
           SystemUtils* utils);
  virtual ~ChildJob();

  // Overridden from ChildJobInterface
  virtual bool ShouldStop() const OVERRIDE;
  virtual void RecordTime() OVERRIDE;
  virtual void Run() OVERRIDE;
  virtual void StartSession(const std::string& email,
                            const std::string& userhash) OVERRIDE;
  virtual void StopSession() OVERRIDE;
  virtual uid_t GetDesiredUid() const OVERRIDE;
  virtual void SetDesiredUid(uid_t uid) OVERRIDE;
  virtual bool IsDesiredUidSet() const OVERRIDE;
  virtual const std::string GetName() const OVERRIDE;
  virtual void SetArguments(const std::vector<std::string>& arguments) OVERRIDE;
  virtual void SetExtraArguments(
      const std::vector<std::string>& arguments) OVERRIDE;
  virtual void AddOneTimeArgument(const std::string& argument) OVERRIDE;
  virtual void ClearOneTimeArgument() OVERRIDE;

  // Export a copy of the current argv.
  std::vector<std::string> ExportArgv();

  // The flag to pass to chrome to tell it to behave as the login manager.
  static const char kLoginManagerFlag[];

  // The flag to pass to chrome to tell it which user has signed in.
  static const char kLoginUserFlag[];

  // The flag to pass to chrome to tell it the hash of the user who's signed in.
  static const char kLoginProfileFlag[];

  // The flag to pass to chrome to tell it to support simultaneous active
  // sessions.
  static const char kMultiProfileFlag[];

  // After kRestartTries in kRestartWindowSeconds, the ChildJob will indicate
  // that it should be stopped.
  static const uint kRestartTries;
  static const time_t kRestartWindowSeconds;

 private:
  // Helper for CreateArgV() that copies a vector of arguments into argv.
  size_t CopyArgsToArgv(const std::vector<std::string>& arguments,
                        char const** argv) const;

  // Allocates and populates array of C strings to pass to exec call.
  char const** CreateArgv() const;

  // If the caller has provided a UID with set_desired_uid(), this method will:
  // 1) try to setgid to that uid
  // 2) try to setgroups to that uid
  // 3) try to setuid to that uid
  //
  // Returns 0 on success, the appropriate exit code (defined above) if a
  // call fails.
  int SetIDs();

  // Arguments to pass to exec.
  std::vector<std::string> arguments_;

  // Login-related arguments to pass to exec.  Managed wholly by this class.
  std::vector<std::string> login_arguments_;

  // Extra arguments to pass to exec.
  std::vector<std::string> extra_arguments_;

  // Extra one time argument.
  std::string extra_one_time_argument_;

  // UID to set for job's process before exec is called.
  uid_t desired_uid_;

  // Indicates if |desired_uid_| was initialized.
  bool is_desired_uid_set_;

  // Wrapper for system library calls.  Owned by the owner of this object.
  SystemUtils* system;

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

  FRIEND_TEST(ChildJobTest, InitializationTest);
  FRIEND_TEST(ChildJobTest, ShouldStopTest);
  FRIEND_TEST(ChildJobTest, ShouldNotStopTest);
  FRIEND_TEST(ChildJobTest, CreateArgv);
  DISALLOW_COPY_AND_ASSIGN(ChildJob);
};

}  // namespace login_manager

#endif  // LOGIN_MANAGER_CHILD_JOB_H_
