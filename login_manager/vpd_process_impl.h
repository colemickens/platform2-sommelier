// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_VPD_PROCESS_IMPL_H_
#define LOGIN_MANAGER_VPD_PROCESS_IMPL_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <base/callback.h>

#include "login_manager/child_job.h"
#include "login_manager/job_manager.h"
#include "login_manager/system_utils.h"
#include "login_manager/vpd_process.h"

namespace login_manager {

class VpdProcessImpl
    : public VpdProcess,
      public JobManagerInterface {
 public:
  explicit VpdProcessImpl(SystemUtils* system_utils);

  // Implementation of VpdProcess.
  bool RunInBackground(const KeyValuePairs& updates,
                       bool sync_cache,
                       const CompletionCallback& completion) override;

  // Implementation of JobManagerInterface.
  bool IsManagedJob(pid_t pid) override;
  void HandleExit(const siginfo_t& status) override;
  void RequestJobExit() override;
  void EnsureJobExit(base::TimeDelta timeout) override;

 private:
  // The subprocess tracked by this job.
  std::unique_ptr<ChildJobInterface::Subprocess> subprocess_;
  SystemUtils* system_utils_;  // Owned by the caller.
  CompletionCallback completion_;
};

}  // namespace login_manager

#endif  // LOGIN_MANAGER_VPD_PROCESS_IMPL_H_
