// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "germ/process_reaper.h"

#include <sys/wait.h>

#include <base/bind.h>
#include <base/location.h>
#include <base/message_loop/message_loop.h>
#include <base/run_loop.h>
#include <gtest/gtest.h>
#include <libchromeos/chromeos/asynchronous_signal_handler.h>

namespace germ {
namespace {

const int kExitCode = 0x80;
const int kNumToFork = 10;
const int kTestTimeout = 10;

class ScopedAlarm {
 public:
  explicit ScopedAlarm(unsigned int seconds) { alarm(seconds); }
  ~ScopedAlarm() { alarm(0); }
};

class TestProcessReaper : public ProcessReaper {
 public:
  TestProcessReaper()
      : num_to_reap_(1), expected_code_(-1), expected_status_(-1) {}
  ~TestProcessReaper() {}

  bool PostTask(const tracked_objects::Location& from_here,
                const base::Closure& task) {
    return message_loop_.task_runner()->PostTask(from_here, task);
  }

  void Run() {
    async_signal_handler_.Init();
    RegisterWithAsyncHandler(&async_signal_handler_);
    run_loop_.Run();
  }

  void set_num_to_reap(int num_to_reap) { num_to_reap_ = num_to_reap; }

  void set_expected_code(int expected_code) { expected_code_ = expected_code; }

  void set_expected_status(int expected_status) {
    expected_status_ = expected_status;
  }

 private:
  void HandleReapedChild(const siginfo_t& info) override {
    EXPECT_EQ(expected_code_, info.si_code);
    EXPECT_EQ(expected_status_, info.si_status);
    EXPECT_GT(num_to_reap_, 0);
    --num_to_reap_;
    if (num_to_reap_ == 0) {
      ASSERT_TRUE(PostTask(FROM_HERE, run_loop_.QuitClosure()));
    }
  }

  int num_to_reap_;
  int expected_code_;
  int expected_status_;
  base::MessageLoopForIO message_loop_;
  chromeos::AsynchronousSignalHandler async_signal_handler_;
  base::RunLoop run_loop_;
};

void ForkAndExit(int num_to_fork) {
  for (int i = 0; i < num_to_fork; ++i) {
    pid_t pid = fork();
    PCHECK(pid != -1);
    if (pid == 0) {
      _exit(kExitCode);
    }
  }
}

void ForkAndRaise(int num_to_fork) {
  for (int i = 0; i < num_to_fork; ++i) {
    pid_t pid = fork();
    PCHECK(pid != -1);
    if (pid == 0) {
      raise(SIGABRT);
    }
  }
}

TEST(ProcessReaper, ReapExitedChild) {
  ScopedAlarm time_out(kTestTimeout);
  TestProcessReaper test_process_reaper;
  test_process_reaper.set_num_to_reap(kNumToFork);
  test_process_reaper.set_expected_code(CLD_EXITED);
  test_process_reaper.set_expected_status(kExitCode);
  ASSERT_TRUE(test_process_reaper.PostTask(
      FROM_HERE, base::Bind(&ForkAndExit, kNumToFork)));
  test_process_reaper.Run();
}

TEST(ProcessReaper, ReapSignaledChild) {
  ScopedAlarm time_out(kTestTimeout);
  TestProcessReaper test_process_reaper;
  test_process_reaper.set_num_to_reap(kNumToFork);
  test_process_reaper.set_expected_code(CLD_DUMPED);
  test_process_reaper.set_expected_status(SIGABRT);
  ASSERT_TRUE(test_process_reaper.PostTask(
      FROM_HERE, base::Bind(&ForkAndRaise, kNumToFork)));
  test_process_reaper.Run();
}

}  // namespace
}  // namespace germ
