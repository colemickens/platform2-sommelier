// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_CHILD_JOB_H_
#define LOGIN_MANAGER_CHILD_JOB_H_

#include <gtest/gtest.h>
#include <time.h>
#include <unistd.h>

#include <string>
#include <vector>

#include <base/basictypes.h>
#include <base/memory/scoped_ptr.h>

class CommandLine;

namespace login_manager {

// SessionManager takes a ChildJob object, forks and then calls the ChildJob's
// Run() method. I've created this interface so that I can create mocks for
// unittesting SessionManager.
class ChildJobInterface {
 public:
  virtual ~ChildJobInterface() {}

  // If ShouldStop() returns true, this means that the parent should tear
  // everything down.
  virtual bool ShouldStop() const = 0;

  // Should we avoid ever killing the process?  This is set for e.g. the window
  // manager, which needs to keep running until X dies since other clients'
  // windows will become visible onscreen when it exits.
  virtual bool ShouldNeverKill() const = 0;

  // Stores the current time as the time when the job was started.
  virtual void RecordTime() = 0;

  // Wraps up all the logic of what the job is meant to do. Should NOT return.
  virtual void Run() = 0;

  // Called when a session is started for a user with |email|.
  virtual void StartSession(const std::string& email) = 0;

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

  // Sets command line arguments for the job from string.
  virtual void SetArguments(const std::string& arguments) = 0;

  // Sets extra command line arguments for the job from a string vector.
  virtual void SetExtraArguments(const std::vector<std::string>& arguments) = 0;

  // Potential exit codes for Run().
  static const int kCantSetUid;
  static const int kCantSetGid;
  static const int kCantSetGroups;
  static const int kCantExec;
  static const int kBWSI;
};


class ChildJob : public ChildJobInterface {
 public:
  explicit ChildJob(const std::vector<std::string>& arguments);
  virtual ~ChildJob();

  // Overridden from ChildJobInterface
  virtual bool ShouldStop() const;
  virtual bool ShouldNeverKill() const;
  virtual void RecordTime();
  virtual void Run();
  virtual void StartSession(const std::string& email);
  virtual void StopSession();
  virtual uid_t GetDesiredUid() const;
  virtual void SetDesiredUid(uid_t uid);
  virtual bool IsDesiredUidSet() const;
  virtual const std::string GetName() const;
  virtual void SetArguments(const std::string& arguments);
  virtual void SetExtraArguments(const std::vector<std::string>& arguments);

  // The flag to pass to chrome to tell it to behave as the login manager.
  static const char kLoginManagerFlag[];
  // The flag to pass to chrome to tell it which user has logged in.
  static const char kLoginUserFlag[];
  // The flag to pass to chrome when starting "Browse Without Sign In" mode.
  static const char kBWSIFlag[];
  // Suffix matched against each job's argv[0] to determine if it's the window
  // manager.
  static const char kWindowManagerSuffix[];

  // Minimum amount of time (in seconds) that should pass since the job was
  // started for it to be restarted if it exits or crashes.
  static const int kRestartWindow;

 private:
  // Helper for CreateArgV() that copies a vector of arguments into argv.
  void CopyArgsToArgv(const std::vector<std::string>& arguments,
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

  // Extra arguments to pass to exec.
  std::vector<std::string> extra_arguments_;

  // UID to set for job's process before exec is called.
  uid_t desired_uid_;

  // Indicates if |desired_uid_| was initialized.
  bool is_desired_uid_set_;

  // The last time the job was run.
  time_t last_start_;

  // Indicates if we removed login manager flag when session started so we
  // add it back when session stops.
  bool removed_login_manager_flag_;

  FRIEND_TEST(ChildJobTest, InitializationTest);
  FRIEND_TEST(ChildJobTest, ShouldStopTest);
  FRIEND_TEST(ChildJobTest, ShouldNotStopTest);
  FRIEND_TEST(ChildJobTest, StartStopSessionTest);
  FRIEND_TEST(ChildJobTest, StartStopSessionFromLoginTest);
  FRIEND_TEST(ChildJobTest, SetArguments);
  FRIEND_TEST(ChildJobTest, SetExtraArguments);
  FRIEND_TEST(ChildJobTest, CreateArgv);
  DISALLOW_COPY_AND_ASSIGN(ChildJob);
};

}  // namespace login_manager

#endif  // LOGIN_MANAGER_CHILD_JOB_H_
