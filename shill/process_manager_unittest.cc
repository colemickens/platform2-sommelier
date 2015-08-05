// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/process_manager.h"

#include <base/bind.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "shill/mock_event_dispatcher.h"

using base::Bind;
using base::Callback;
using base::CancelableClosure;
using base::Closure;
using base::Unretained;

namespace shill {

class ProcessManagerTest : public testing::Test {
 public:
  ProcessManagerTest() : process_manager_(ProcessManager::GetInstance()) {}

  virtual void SetUp() {
    process_manager_->dispatcher_ = &dispatcher_;
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
  ProcessManager* process_manager_;
};

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

}  // namespace shill
