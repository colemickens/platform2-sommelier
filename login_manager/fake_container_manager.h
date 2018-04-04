// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_FAKE_CONTAINER_MANAGER_H_
#define LOGIN_MANAGER_FAKE_CONTAINER_MANAGER_H_

#include <string>
#include <vector>

#include <base/callback.h>

#include "login_manager/container_manager_interface.h"

namespace login_manager {

// Fake implementation used for tests.
class FakeContainerManager : public ContainerManagerInterface {
 public:
  explicit FakeContainerManager(pid_t pid);
  ~FakeContainerManager() override = default;

  bool running() const { return running_; }

  bool IsManagedJob(pid_t pid) override;
  void HandleExit(const siginfo_t& status) override;
  void RequestJobExit(ArcContainerStopReason reason) override;
  void EnsureJobExit(base::TimeDelta timeout) override;

  bool StartContainer(const std::vector<std::string>& env,
                      const ExitCallback& exit_callback) override;
  StatefulMode GetStatefulMode() const override;
  void SetStatefulMode(StatefulMode mode) override;
  bool GetContainerPID(pid_t* pid_out) const override;

  void SimulateCrash();

 private:
  // True if the container is running.
  bool running_ = false;

  StatefulMode stateful_mode_ = StatefulMode::STATELESS;

  const pid_t pid_;
  ExitCallback exit_callback_;

  DISALLOW_COPY_AND_ASSIGN(FakeContainerManager);
};

}  // namespace login_manager

#endif  // LOGIN_MANAGER_FAKE_CONTAINER_MANAGER_H_
