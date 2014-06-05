// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_CHILD_JOB_H_
#define LOGIN_MANAGER_CHILD_JOB_H_

#include <unistd.h>

#include <map>
#include <string>
#include <vector>

#include <base/basictypes.h>
#include <base/time/time.h>

namespace login_manager {

class SystemUtils;

// An interface declaring the basic functionality of a job that can be managed
// by SessionManagerService.
class ChildJobInterface {
 public:
  // A class that provides functionality for creating/destroying a subprocess.
  // Intended to be embedded in an implementation of ChildJobInterface.
  class Subprocess {
   public:
    Subprocess(uid_t desired_uid, SystemUtils* system);
    virtual ~Subprocess();

    // fork(), export |environment_variables|, and exec(argv).
    // Returns false if fork() fails, true otherwise.
    bool ForkAndExec(
        const std::vector<std::string>& args,
        const std::map<std::string, std::string>& environment_variables);

    // Sends signal to pid_. No-op if there is no subprocess running.
    void Kill(int signal);

    // Sends signal to pid_'s entire process group.
    // No-op if there is no subprocess running.
    void KillEverything(int signal);

    pid_t pid() const { return pid_; }
    void clear_pid() { pid_ = -1; }

   private:
    // If the caller has provided a UID with set_desired_uid(), this method will
    // 1) try to setgid to that uid
    // 2) try to setgroups to that uid
    // 3) try to setuid to that uid
    //
    // Returns 0 on success, the appropriate exit code (defined above) if a
    // call fails.
    int SetIDs();

    // The pid of the managed subprocess, when running. Set to -1 when
    // cleared, or not yet set by ForkAndExec().
    pid_t pid_;
    // The uid the subprocess should be run as.
    const uid_t desired_uid_;
    SystemUtils* const system_;  // weak; owned by embedder.
    DISALLOW_COPY_AND_ASSIGN(Subprocess);
  };

  virtual ~ChildJobInterface() {}

  // Creates a background process and starts the job running in it. Does any
  // necessary bookkeeping.
  // Returns true if the process was created, false otherwise.
  virtual bool RunInBackground() = 0;

  // Attempt to kill the current instance of this job by sending
  // signal to the _entire process group_, sending message (if set) to
  // the instance to tell it why it must die.
  virtual void KillEverything(int signal, const std::string& message) = 0;

  // Attempt to kill the current instance of this job by sending
  // signal, sending message (if set) to the instance to tell it
  // why it must die.
  virtual void Kill(int signal, const std::string& message) = 0;

  // Waits |timeout| for current instance of this job to go away, then
  // aborts the entire process group if it's not gone.
  virtual void WaitAndAbort(base::TimeDelta timeout) = 0;

  // Returns the name of the job.
  virtual const std::string GetName() const = 0;

  // Returns the pid of the current instance of this job. May be -1.
  virtual pid_t CurrentPid() const = 0;

  // Potential exit codes for use in Subprocess::Run().
  static const int kCantSetUid;
  static const int kCantSetGid;
  static const int kCantSetGroups;
  static const int kCantSetEnv;
  static const int kCantExec;
};

}  // namespace login_manager

#endif  // LOGIN_MANAGER_CHILD_JOB_H_
