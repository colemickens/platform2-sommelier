// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/child_exit_handler.h"

#include <signal.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <string>
#include <vector>

#include <base/message_loop/message_loop.h>
#include <base/run_loop.h>
#include <brillo/asynchronous_signal_handler.h>
#include <brillo/message_loops/base_message_loop.h>
#include <gtest/gtest.h>

#include "login_manager/job_manager.h"
#include "login_manager/system_utils_impl.h"

namespace login_manager {

// A fake job manager implementation for testing.
class FakeJobManager : public JobManagerInterface {
 public:
  FakeJobManager() = default;
  ~FakeJobManager() override = default;

  pid_t managed_pid() { return 0; }
  const siginfo_t& last_status() { return last_status_; }

  // Implementation of JobManagerInterface.
  bool IsManagedJob(pid_t pid) override { return true; }
  void HandleExit(const siginfo_t& s) override {
    last_status_ = s;
    brillo::MessageLoop::current()->BreakLoop();
  }
  void RequestJobExit(const std::string& reason) override {}
  void EnsureJobExit(base::TimeDelta timeout) override {}

 private:
  siginfo_t last_status_;

  DISALLOW_COPY_AND_ASSIGN(FakeJobManager);
};

class ChildExitHandlerTest : public ::testing::Test {
 public:
  ChildExitHandlerTest() = default;
  ~ChildExitHandlerTest() override = default;

  void SetUp() override {
    brillo_loop_.SetAsCurrent();
    std::vector<JobManagerInterface*> managers;
    managers.push_back(&fake_manager_);
    signal_handler_.Init();
    handler_.Init(&signal_handler_, managers);
  }

 protected:
  base::MessageLoopForIO loop_;
  brillo::BaseMessageLoop brillo_loop_{&loop_};
  SystemUtilsImpl system_utils_;
  brillo::AsynchronousSignalHandler signal_handler_;
  ChildExitHandler handler_;
  FakeJobManager fake_manager_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChildExitHandlerTest);
};

TEST_F(ChildExitHandlerTest, ChildExit) {
  // Fork off a child process that exits immediately.
  pid_t child_pid = system_utils_.fork();
  if (child_pid == 0) {
    _Exit(EXIT_SUCCESS);
  }

  // Spin the message loop.
  brillo_loop_.Run();

  // Verify child termination has been reported to |fake_manager|.
  EXPECT_EQ(child_pid, fake_manager_.last_status().si_pid);
  EXPECT_EQ(SIGCHLD, fake_manager_.last_status().si_signo);
  EXPECT_EQ(static_cast<int>(CLD_EXITED), fake_manager_.last_status().si_code);
  EXPECT_EQ(EXIT_SUCCESS, fake_manager_.last_status().si_status);
}

}  // namespace login_manager
