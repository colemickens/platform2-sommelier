// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_FAKE_JOB_MANAGER_H_
#define LOGIN_MANAGER_FAKE_JOB_MANAGER_H_

#include "login_manager/job_manager.h"

#include <signal.h>
#include <sys/types.h>

#include <base/time/time.h>

namespace login_manager {

// An interface for classes that manage background processes.
class FakeJobManager : public JobManagerInterface {
 public:
  explicit FakeJobManager(pid_t pid_to_manage) : managed_pid_(pid_to_manage) {}
  virtual ~FakeJobManager() {}

  pid_t managed_pid() { return managed_pid_; }
  const siginfo_t& last_status() { return last_status_; }

  // Implementation of JobManagerInterface.
  virtual bool IsManagedJob(pid_t pid) OVERRIDE { return pid == managed_pid_; }
  virtual void HandleExit(const siginfo_t& s) OVERRIDE { last_status_ = s; }
  virtual void RequestJobExit() OVERRIDE {}
  virtual void EnsureJobExit(base::TimeDelta timeout) OVERRIDE {}

 private:
  const pid_t managed_pid_;
  siginfo_t last_status_;
  DISALLOW_COPY_AND_ASSIGN(FakeJobManager);
};
}  // namespace login_manager

#endif  // LOGIN_MANAGER_FAKE_JOB_MANAGER_H_
