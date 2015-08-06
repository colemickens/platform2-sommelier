// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/process_manager.h"

#include <string>
#include <vector>

#include <base/bind.h>
#include <chromeos/minijail/mock_minijail.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "shill/mock_event_dispatcher.h"

using base::Bind;
using base::Callback;
using base::CancelableClosure;
using base::Closure;
using base::Unretained;
using std::string;
using std::vector;
using testing::_;
using testing::DoAll;
using testing::Return;
using testing::SetArgumentPointee;
using testing::StrEq;

namespace shill {

class ProcessManagerTest : public testing::Test {
 public:
  ProcessManagerTest() : process_manager_(ProcessManager::GetInstance()) {}

  virtual void SetUp() {
    process_manager_->dispatcher_ = &dispatcher_;
    process_manager_->minijail_ = &minijail_;
  }

  virtual void TearDown() {
    process_manager_->watched_processes_.clear();
    process_manager_->pending_termination_processes_.clear();
  }

  void AddWatchedProcess(pid_t pid, const Callback<void(int)>& callback) {
    process_manager_->watched_processes_.emplace(pid, callback);
  }

  void AddTerminateProcess(pid_t pid,
                           std::unique_ptr<CancelableClosure> timeout_handler) {
    process_manager_->pending_termination_processes_.emplace(
        pid, std::move(timeout_handler));
  }

  void AssertEmptyWatchedProcesses() {
    EXPECT_TRUE(process_manager_->watched_processes_.empty());
  }

  void AssertNonEmptyWatchedProcesses() {
    EXPECT_FALSE(process_manager_->watched_processes_.empty());
  }

  void AssertEmptyTerminateProcesses() {
    EXPECT_TRUE(process_manager_->pending_termination_processes_.empty());
  }

  void OnProcessExited(pid_t pid, int exit_status) {
    siginfo_t info;
    info.si_status = exit_status;
    process_manager_->OnProcessExited(pid, info);
  }

  void OnTerminationTimeout(pid_t pid, bool kill_signal) {
    process_manager_->ProcessTerminationTimeoutHandler(pid, kill_signal);
  }

 protected:
  class CallbackObserver {
   public:
    CallbackObserver()
        : exited_callback_(
              Bind(&CallbackObserver::OnProcessExited, Unretained(this))),
          termination_timeout_callback_(
              Bind(&CallbackObserver::OnTerminationTimeout,
                   Unretained(this))) {}
    virtual ~CallbackObserver() {}

    MOCK_METHOD1(OnProcessExited, void(int exit_status));
    MOCK_METHOD0(OnTerminationTimeout, void());

    Callback<void(int)> exited_callback_;
    Closure termination_timeout_callback_;
  };

  MockEventDispatcher dispatcher_;
  chromeos::MockMinijail minijail_;
  ProcessManager* process_manager_;
};

MATCHER_P2(IsProcessArgs, program, args, "") {
  if (string(arg[0]) != program) {
    return false;
  }
  int index = 1;
  for (const auto& option : args) {
    if (string(arg[index++]) != option) {
      return false;
    }
  }
  return arg[index] == nullptr;
}

TEST_F(ProcessManagerTest, WatchedProcessExited) {
  const pid_t kPid = 123;
  const int kExitStatus = 1;
  CallbackObserver observer;
  AddWatchedProcess(kPid, observer.exited_callback_);

  EXPECT_CALL(observer, OnProcessExited(kExitStatus)).Times(1);
  OnProcessExited(kPid, kExitStatus);
  AssertEmptyWatchedProcesses();
}

TEST_F(ProcessManagerTest, TerminateProcessExited) {
  const pid_t kPid = 123;
  CallbackObserver observer;
  std::unique_ptr<CancelableClosure> timeout_handler(
      new CancelableClosure(observer.termination_timeout_callback_));
  AddTerminateProcess(kPid, std::move(timeout_handler));

  EXPECT_CALL(observer, OnTerminationTimeout()).Times(0);
  OnProcessExited(kPid, 1);
  AssertEmptyTerminateProcesses();
}

TEST_F(ProcessManagerTest, StartProcessInMinijail) {
  const string kProgram = "/usr/bin/dump";
  const vector<string> kArgs = { "-b", "-g" };
  const string kUser = "user";
  const string kGroup = "group";
  const uint64_t kCapMask = 1;
  const pid_t kPid = 123;

  EXPECT_CALL(minijail_, DropRoot(_, StrEq(kUser), StrEq(kGroup))).Times(1);
  EXPECT_CALL(minijail_, UseCapabilities(_, kCapMask)).Times(1);
  EXPECT_CALL(minijail_, RunAndDestroy(_, IsProcessArgs(kProgram, kArgs), _))
      .WillOnce(DoAll(SetArgumentPointee<2>(kPid), Return(true)));
  pid_t actual_pid =
      process_manager_->StartProcessInMinijail(FROM_HERE,
                                               base::FilePath(kProgram),
                                               kArgs,
                                               kUser,
                                               kGroup,
                                               kCapMask,
                                               Callback<void(int)>());
  EXPECT_EQ(kPid, actual_pid);
  AssertNonEmptyWatchedProcesses();
}

TEST_F(ProcessManagerTest, StartProcessInMinijailFailed) {
  const string kProgram = "/usr/bin/dump";
  const vector<string> kArgs = { "-b", "-g" };
  const string kUser = "user";
  const string kGroup = "group";
  const uint64_t kCapMask = 1;

  EXPECT_CALL(minijail_, DropRoot(_, StrEq(kUser), StrEq(kGroup))).Times(1);
  EXPECT_CALL(minijail_, UseCapabilities(_, kCapMask)).Times(1);
  EXPECT_CALL(minijail_, RunAndDestroy(_, IsProcessArgs(kProgram, kArgs), _))
      .WillOnce(Return(false));
  pid_t actual_pid =
      process_manager_->StartProcessInMinijail(FROM_HERE,
                                               base::FilePath(kProgram),
                                               kArgs,
                                               kUser,
                                               kGroup,
                                               kCapMask,
                                               Callback<void(int)>());
  EXPECT_EQ(-1, actual_pid);
  AssertEmptyWatchedProcesses();
}

}  // namespace shill
