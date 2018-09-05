// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/child_exit_dispatcher.h"

#include <signal.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <memory>
#include <string>
#include <vector>

#include <base/message_loop/message_loop.h>
#include <base/run_loop.h>
#include <brillo/asynchronous_signal_handler.h>
#include <brillo/message_loops/base_message_loop.h>
#include <gtest/gtest.h>

#include "login_manager/child_exit_handler.h"
#include "login_manager/system_utils_impl.h"

namespace login_manager {

// A fake child exit handler implementation for testing.
class FakeChildExitHandler : public ChildExitHandler {
 public:
  FakeChildExitHandler() = default;
  ~FakeChildExitHandler() override = default;

  const siginfo_t& last_status() { return last_status_; }

  // ChildExitHandler overrides.
  bool HandleExit(const siginfo_t& s) override {
    last_status_ = s;
    brillo::MessageLoop::current()->BreakLoop();
    return true;
  }

 private:
  siginfo_t last_status_;

  DISALLOW_COPY_AND_ASSIGN(FakeChildExitHandler);
};

class ChildExitDispatcherTest : public ::testing::Test {
 public:
  ChildExitDispatcherTest() = default;
  ~ChildExitDispatcherTest() override = default;

  void SetUp() override {
    brillo_loop_.SetAsCurrent();
    signal_handler_.Init();
    dispatcher_ = std::make_unique<ChildExitDispatcher>(
        &signal_handler_, std::vector<ChildExitHandler*>{&fake_handler_});
  }

 protected:
  base::MessageLoopForIO loop_;
  brillo::BaseMessageLoop brillo_loop_{&loop_};
  SystemUtilsImpl system_utils_;
  brillo::AsynchronousSignalHandler signal_handler_;
  FakeChildExitHandler fake_handler_;
  std::unique_ptr<ChildExitDispatcher> dispatcher_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChildExitDispatcherTest);
};

TEST_F(ChildExitDispatcherTest, ChildExit) {
  // Fork off a child process that exits immediately.
  pid_t child_pid = system_utils_.fork();
  if (child_pid == 0) {
    _Exit(EXIT_SUCCESS);
  }

  // Spin the message loop.
  brillo_loop_.Run();

  // Verify child termination has been reported to |fake_handler_|.
  EXPECT_EQ(child_pid, fake_handler_.last_status().si_pid);
  EXPECT_EQ(SIGCHLD, fake_handler_.last_status().si_signo);
  EXPECT_EQ(static_cast<int>(CLD_EXITED), fake_handler_.last_status().si_code);
  EXPECT_EQ(EXIT_SUCCESS, fake_handler_.last_status().si_status);
}

}  // namespace login_manager
