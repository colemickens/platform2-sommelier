// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_JOB_MANAGER_H_
#define LOGIN_MANAGER_JOB_MANAGER_H_

#include <signal.h>
#include <sys/types.h>

#include <base/time.h>

namespace login_manager {

// An interface for classes that manage background processes.
class JobManagerInterface {
 public:
  virtual ~JobManagerInterface() {}

  // Check if |pid| is the currently-managed job.
  virtual bool IsManagedJob(pid_t pid) = 0;

  // The job managed by this object exited, with |status|.
  virtual void HandleExit(const siginfo_t& status) = 0;

  // Ask the managed job to exit.
  virtual void RequestJobExit() = 0;

  // The job must be destroyed within the timeout.
  virtual void EnsureJobExit(base::TimeDelta timeout) = 0;
};
}  // namespace login_manager

#endif  // LOGIN_MANAGER_JOB_MANAGER_H_
