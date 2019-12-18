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
#include "diagnostics/routines/routine_test_utils.h"

namespace diagnostics {
namespace mojo_ipc = ::chromeos::cros_healthd::mojom;

namespace {

using ::testing::_;
using ::testing::AtMost;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::StrictMock;
using ::testing::Test;

void CheckRoutineUpdate(int progress_percent,
                        std::string status_message,
                        mojo_ipc::DiagnosticRoutineStatusEnum status,
                        const mojo_ipc::RoutineUpdate& update) {
  EXPECT_EQ(update.progress_percent, progress_percent);
  VerifyNonInteractiveUpdate(update.routine_update_union, status,
                             status_message);
}

class MockDiagProcessAdapter : public DiagProcessAdapter {
 public:
  MOCK_METHOD(base::TerminationStatus,
              GetStatus,
              (const base::ProcessHandle&),
              (const, override));
  MOCK_METHOD(bool,
              StartProcess,
              (const std::vector<std::string>&, base::ProcessHandle*),
              (override));
  MOCK_METHOD(bool, KillProcess, (const base::ProcessHandle&), (override));
};

}  // namespace

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
    routine_->PopulateStatusUpdate(&update_, true);
  }

  void PopulateStatusUpdate() {
    routine_->PopulateStatusUpdate(&update_, true);
  }

 private:
  StrictMock<MockDiagProcessAdapter>* mock_adapter_;  // Owned by |routine_|.
  base::SimpleTestTickClock* tick_clock_;             // Owned by |routine_|.
  std::unique_ptr<SubprocRoutine> routine_;
  mojo_ipc::RoutineUpdate update_{0, mojo::ScopedHandle(),
                                  mojo_ipc::RoutineUpdateUnion::New()};

  DISALLOW_COPY_AND_ASSIGN(SubprocRoutineTest);
};

TEST_F(SubprocRoutineTest, InvokeSubprocWithSuccess) {
  EXPECT_CALL(*mock_adapter(), StartProcess(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(base::GetCurrentProcessHandle()),
                      Return(true)));

  EXPECT_CALL(*mock_adapter(), GetStatus(_))
      .WillOnce(Return(base::TERMINATION_STATUS_NORMAL_TERMINATION));
  routine()->Start();

  mojo_ipc::RoutineUpdate update{0, mojo::ScopedHandle(),
                                 mojo_ipc::RoutineUpdateUnion::New()};
  routine()->PopulateStatusUpdate(&update, false);

  CheckRoutineUpdate(100, kSubprocRoutineSucceededMessage,
                     mojo_ipc::DiagnosticRoutineStatusEnum::kPassed, update);
}

TEST_F(SubprocRoutineTest, InvokeSubprocWithFailure) {
  EXPECT_CALL(*mock_adapter(), StartProcess(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(base::GetCurrentProcessHandle()),
                      Return(true)));
  EXPECT_CALL(*mock_adapter(), GetStatus(_))
      .WillOnce(Return(base::TERMINATION_STATUS_ABNORMAL_TERMINATION));
  routine()->Start();

  mojo_ipc::RoutineUpdate update{0, mojo::ScopedHandle(),
                                 mojo_ipc::RoutineUpdateUnion::New()};
  routine()->PopulateStatusUpdate(&update, false);

  CheckRoutineUpdate(100, kSubprocRoutineFailedMessage,
                     mojo_ipc::DiagnosticRoutineStatusEnum::kFailed, update);
}

TEST_F(SubprocRoutineTest, FailInvokingSubproc) {
  EXPECT_CALL(*mock_adapter(), StartProcess(_, _)).WillOnce(Return(false));
  routine()->Start();

  EXPECT_EQ(routine()->GetStatus(),
            mojo_ipc::DiagnosticRoutineStatusEnum::kFailedToStart);
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

  mojo_ipc::RoutineUpdate update{0, mojo::ScopedHandle(),
                                 mojo_ipc::RoutineUpdateUnion::New()};
  routine()->PopulateStatusUpdate(&update, false);

  CheckRoutineUpdate(50, kSubprocRoutineProcessRunningMessage,
                     mojo_ipc::DiagnosticRoutineStatusEnum::kRunning, update);
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
  mojo_ipc::RoutineUpdate update{0, mojo::ScopedHandle(),
                                 mojo_ipc::RoutineUpdateUnion::New()};
  routine()->PopulateStatusUpdate(&update, false);

  CheckRoutineUpdate(50, kSubprocRoutineProcessRunningMessage,
                     mojo_ipc::DiagnosticRoutineStatusEnum::kRunning, update);

  routine()->Cancel();

  tick_clock()->Advance(base::TimeDelta::FromSeconds(1));

  routine()->PopulateStatusUpdate(&update, false);

  CheckRoutineUpdate(50, kSubprocRoutineProcessCancellingMessage,
                     mojo_ipc::DiagnosticRoutineStatusEnum::kCancelling,
                     update);

  // Now the process should appear dead
  routine()->PopulateStatusUpdate(&update, false);

  CheckRoutineUpdate(50, kSubprocRoutineCancelled,
                     mojo_ipc::DiagnosticRoutineStatusEnum::kCancelled, update);
}

}  // namespace diagnostics
