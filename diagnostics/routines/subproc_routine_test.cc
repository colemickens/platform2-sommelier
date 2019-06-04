// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "diagnostics/routines/subproc_routine.h"

#include <utility>
#include <vector>

#include <base/macros.h>
#include <base/command_line.h>
#include <base/test/simple_test_tick_clock.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "diagnostics/routines/diag_process_adapter.h"

namespace diagnostics {

namespace {

using ::testing::_;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::StrictMock;
using ::testing::Test;
using ::testing::AtMost;

class MockDiagProcessAdapter : public DiagProcessAdapter {
 public:
  MOCK_CONST_METHOD1(GetStatus,
                     base::TerminationStatus(const base::ProcessHandle&));
  MOCK_METHOD2(StartProcess,
               bool(const std::vector<std::string>& args,
                    base::ProcessHandle* handle));
  MOCK_METHOD1(KillProcess, bool(const base::ProcessHandle&));
};

class SubprocRoutineTest : public Test {
 protected:
  SubprocRoutineTest() {
    auto mock_adapter_ptr =
        std::make_unique<StrictMock<MockDiagProcessAdapter>>();
    mock_adapter_ = mock_adapter_ptr.get();
    auto tick_clock_ptr = std::make_unique<base::SimpleTestTickClock>();
    tick_clock_ = tick_clock_ptr.get();

    // We never actually run subprocesses in this unit test, because this module
    // is not actually responsible for process invocation, and we trust the
    // DiagProcessAdapter to do things appropriately.
    auto command_line = base::CommandLine({"/dev/null"});

    // For testing purposes, we will always pretend that subprocs are predicted
    // to take 10s.
    int predicted_duration_in_seconds = 10;

    routine_ = std::make_unique<SubprocRoutine>(
        std::move(mock_adapter_ptr), std::move(tick_clock_ptr), command_line,
        predicted_duration_in_seconds);
  }

  SubprocRoutine* routine() { return routine_.get(); }

  StrictMock<MockDiagProcessAdapter>* mock_adapter() { return mock_adapter_; }
  base::SimpleTestTickClock* tick_clock() { return tick_clock_; }

  void RunRoutineWithTerminationStatus(base::TerminationStatus status) {
    routine_->Start();
    PopulateStatusUpdateForRunningRoutine(status);
  }

  void RunRoutinePastCompletion() {
    routine_->Start();
    PopulateStatusUpdateForRunningRoutine(
        base::TERMINATION_STATUS_STILL_RUNNING);
  }

  void PopulateStatusUpdateForRunningRoutine(base::TerminationStatus status) {
    EXPECT_CALL(*mock_adapter_, GetStatus(_)).WillOnce(Return(status));
    routine_->PopulateStatusUpdate(&response_, true);
  }

  void PopulateStatusUpdate() {
    routine_->PopulateStatusUpdate(&response_, true);
  }

 private:
  StrictMock<MockDiagProcessAdapter>* mock_adapter_;  // Owned by |routine_|.
  base::SimpleTestTickClock* tick_clock_;             // Owned by |routine_|.
  std::unique_ptr<SubprocRoutine> routine_;
  grpc_api::GetRoutineUpdateResponse response_;
  grpc_api::UrandomRoutineParameters params_;

  DISALLOW_COPY_AND_ASSIGN(SubprocRoutineTest);
};

TEST_F(SubprocRoutineTest, InvokeSubprocWithSuccess) {
  EXPECT_CALL(*mock_adapter(), StartProcess(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(base::GetCurrentProcessHandle()),
                      Return(true)));

  EXPECT_CALL(*mock_adapter(), GetStatus(_))
      .WillOnce(Return(base::TERMINATION_STATUS_NORMAL_TERMINATION));
  routine()->Start();

  grpc_api::GetRoutineUpdateResponse response;
  routine()->PopulateStatusUpdate(&response, false);

  EXPECT_EQ(response.progress_percent(), 100);
  EXPECT_EQ(response.status_message(), kSubprocRoutineSucceededMessage);
  EXPECT_EQ(response.status(), grpc_api::ROUTINE_STATUS_PASSED);
}

TEST_F(SubprocRoutineTest, InvokeSubprocWithFailure) {
  EXPECT_CALL(*mock_adapter(), StartProcess(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(base::GetCurrentProcessHandle()),
                      Return(true)));
  EXPECT_CALL(*mock_adapter(), GetStatus(_))
      .WillOnce(Return(base::TERMINATION_STATUS_ABNORMAL_TERMINATION));
  routine()->Start();

  grpc_api::GetRoutineUpdateResponse response;
  routine()->PopulateStatusUpdate(&response, false);

  EXPECT_EQ(response.progress_percent(), 100);
  EXPECT_EQ(response.status_message(), kSubprocRoutineFailedMessage);
  EXPECT_EQ(response.status(), grpc_api::ROUTINE_STATUS_FAILED);
}

TEST_F(SubprocRoutineTest, FailInvokingSubproc) {
  EXPECT_CALL(*mock_adapter(), StartProcess(_, _)).WillOnce(Return(false));
  routine()->Start();

  EXPECT_EQ(routine()->GetStatus(), grpc_api::ROUTINE_STATUS_FAILED_TO_START);
}

TEST_F(SubprocRoutineTest, TestHalfProgressPercent) {
  EXPECT_CALL(*mock_adapter(), StartProcess(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(base::GetCurrentProcessHandle()),
                      Return(true)));
  EXPECT_CALL(*mock_adapter(), GetStatus(_))
      .Times(AtMost(2))
      .WillOnce(Return(base::TERMINATION_STATUS_STILL_RUNNING))
      .WillOnce(Return(base::TERMINATION_STATUS_NORMAL_TERMINATION));

  routine()->Start();

  tick_clock()->Advance(base::TimeDelta::FromSeconds(5));

  grpc_api::GetRoutineUpdateResponse response;
  routine()->PopulateStatusUpdate(&response, false);

  EXPECT_EQ(response.progress_percent(), 50);
  EXPECT_EQ(response.status_message(), kSubprocRoutineProcessRunningMessage);
  EXPECT_EQ(response.status(), grpc_api::ROUTINE_STATUS_RUNNING);
}

TEST_F(SubprocRoutineTest, TestHalfProgressThenCancel) {
  EXPECT_CALL(*mock_adapter(), StartProcess(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(base::GetCurrentProcessHandle()),
                      Return(true)));
  EXPECT_CALL(*mock_adapter(), KillProcess(_)).Times(AtMost(1));
  EXPECT_CALL(*mock_adapter(), GetStatus(_))
      .Times(AtMost(4))
      .WillOnce(Return(base::TERMINATION_STATUS_STILL_RUNNING))
      .WillOnce(Return(base::TERMINATION_STATUS_STILL_RUNNING))
      .WillOnce(Return(base::TERMINATION_STATUS_STILL_RUNNING))
      .WillOnce(Return(base::TERMINATION_STATUS_ABNORMAL_TERMINATION));

  routine()->Start();

  tick_clock()->Advance(base::TimeDelta::FromSeconds(5));
  grpc_api::GetRoutineUpdateResponse response;
  routine()->PopulateStatusUpdate(&response, false);

  EXPECT_EQ(response.progress_percent(), 50);
  EXPECT_EQ(response.status_message(), kSubprocRoutineProcessRunningMessage);
  EXPECT_EQ(response.status(), grpc_api::ROUTINE_STATUS_RUNNING);

  routine()->Cancel();

  tick_clock()->Advance(base::TimeDelta::FromSeconds(1));

  routine()->PopulateStatusUpdate(&response, false);

  EXPECT_EQ(response.progress_percent(), 50);
  EXPECT_EQ(response.status_message(), kSubprocRoutineProcessCancellingMessage);
  EXPECT_EQ(response.status(), grpc_api::ROUTINE_STATUS_CANCELLING);

  // Now the process should appear dead
  routine()->PopulateStatusUpdate(&response, false);

  EXPECT_EQ(response.progress_percent(), 50);
  EXPECT_EQ(response.status_message(), kSubprocRoutineCancelled);
  EXPECT_EQ(response.status(), grpc_api::ROUTINE_STATUS_CANCELLED);
}

}  // namespace
}  // namespace diagnostics
