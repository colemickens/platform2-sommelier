// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <base/macros.h>
#include <base/process/process_handle.h>
#include <base/test/simple_test_tick_clock.h>
#include <base/time/time.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "diagnostics/routines/diag_process_adapter.h"
#include "diagnostics/routines/urandom/urandom.h"
#include "wilco_dtc_supportd.pb.h"  // NOLINT(build/include)

using testing::_;
using testing::DoAll;
using testing::Return;
using testing::SetArgPointee;
using testing::StrictMock;

namespace diagnostics {

namespace {
constexpr int kLengthSeconds = 100;
constexpr int kPercentCompleted = 43;

const std::vector<std::string> ConstructUrandomArgs() {
  std::vector<std::string> args;
  args.push_back("/usr/libexec/diagnostics/urandom");  // Executable path.
  args.push_back("--time_delta_ms=" +
                 std::to_string(kLengthSeconds * 1000));  // Length in ms.
  args.push_back(
      "--urandom_path=/dev/urandom");  // Path to the urandom dev node.
  return args;
}

class MockDiagProcessAdapter : public DiagProcessAdapter {
 public:
  MOCK_CONST_METHOD1(GetStatus,
                     base::TerminationStatus(const base::ProcessHandle&));
  MOCK_METHOD2(StartProcess,
               bool(const std::vector<std::string>& args,
                    base::ProcessHandle* handle));
  MOCK_METHOD1(KillProcess, bool(const base::ProcessHandle&));
};

}  // namespace

class UrandomRoutineTest : public testing::Test {
 protected:
  UrandomRoutineTest() {
    auto new_tick_clock = std::make_unique<base::SimpleTestTickClock>();
    tick_clock_ = new_tick_clock.get();
    auto new_mock_adapter =
        std::make_unique<StrictMock<MockDiagProcessAdapter>>();
    mock_adapter_ = new_mock_adapter.get();
    params_.set_length_seconds(kLengthSeconds);
    routine_ = std::make_unique<UrandomRoutine>(
        params_, std::move(new_tick_clock), std::move(new_mock_adapter));
  }

  UrandomRoutine* routine() { return routine_.get(); }

  base::SimpleTestTickClock* tick_clock() { return tick_clock_; }

  grpc_api::GetRoutineUpdateResponse* response() { return &response_; }

  StrictMock<MockDiagProcessAdapter>* mock_adapter() { return mock_adapter_; }

  void RunRoutineWithTerminationStatus(base::TerminationStatus status) {
    routine_->Start();
    PopulateStatusUpdateForRunningRoutine(status);
  }

  void RunRoutineToPercent(int percent) {
    routine_->Start();
    tick_clock_->Advance(
        base::TimeDelta::FromSeconds(percent * kLengthSeconds / 100));
    PopulateStatusUpdateForRunningRoutine(
        base::TERMINATION_STATUS_STILL_RUNNING);
  }

  void RunRoutinePastCompletion() {
    routine_->Start();
    tick_clock_->Advance(base::TimeDelta::FromSeconds(kLengthSeconds + 1));
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
  std::unique_ptr<UrandomRoutine> routine_;
  grpc_api::GetRoutineUpdateResponse response_;
  grpc_api::UrandomRoutineParameters params_;

  DISALLOW_COPY_AND_ASSIGN(UrandomRoutineTest);
};

// Test that the urandom routine fails if the urandom executable fails to start.
TEST_F(UrandomRoutineTest, ExeFailsToStart) {
  EXPECT_CALL(*mock_adapter(), StartProcess(ConstructUrandomArgs(), _))
      .WillOnce(Return(false));
  routine()->Start();
  PopulateStatusUpdate();
  EXPECT_EQ(response()->status_message(), kUrandomFailedToLaunchProcessMessage);
  EXPECT_EQ(response()->status(), grpc_api::ROUTINE_STATUS_ERROR);
}

// Test that the urandom routine fails if the executable fails.
TEST_F(UrandomRoutineTest, ExeReturnsFailure) {
  EXPECT_CALL(*mock_adapter(), StartProcess(ConstructUrandomArgs(), _))
      .WillOnce(DoAll(SetArgPointee<1>(base::GetCurrentProcessHandle()),
                      Return(true)));
  RunRoutineWithTerminationStatus(
      base::TERMINATION_STATUS_ABNORMAL_TERMINATION);
  EXPECT_EQ(response()->status_message(), kUrandomRoutineFailedMessage);
  EXPECT_EQ(response()->status(), grpc_api::ROUTINE_STATUS_FAILED);
}

// Test that the urandom routine passes if the executable behaves as expected.
TEST_F(UrandomRoutineTest, NormalOperation) {
  EXPECT_CALL(*mock_adapter(), StartProcess(ConstructUrandomArgs(), _))
      .WillOnce(DoAll(SetArgPointee<1>(base::GetCurrentProcessHandle()),
                      Return(true)));
  RunRoutineWithTerminationStatus(base::TERMINATION_STATUS_STILL_RUNNING);
  EXPECT_EQ(response()->status_message(), kUrandomProcessRunningMessage);
  EXPECT_EQ(response()->status(), grpc_api::ROUTINE_STATUS_RUNNING);
  tick_clock()->Advance(base::TimeDelta::FromSeconds(kLengthSeconds));
  PopulateStatusUpdateForRunningRoutine(
      base::TERMINATION_STATUS_NORMAL_TERMINATION);
  EXPECT_EQ(response()->status_message(), kUrandomRoutineSucceededMessage);
  EXPECT_EQ(response()->status(), grpc_api::ROUTINE_STATUS_PASSED);
}

// Test that we can calculate the progress percentage correctly.
TEST_F(UrandomRoutineTest, ProgressPercentage) {
  EXPECT_CALL(*mock_adapter(), StartProcess(ConstructUrandomArgs(), _))
      .WillOnce(DoAll(SetArgPointee<1>(base::GetCurrentProcessHandle()),
                      Return(true)));
  RunRoutineToPercent(kPercentCompleted);
  EXPECT_EQ(response()->status(), grpc_api::ROUTINE_STATUS_RUNNING);
  EXPECT_EQ(response()->progress_percent(), kPercentCompleted);
  EXPECT_CALL(*mock_adapter(), GetStatus(_))
      .WillOnce(Return(base::TERMINATION_STATUS_STILL_RUNNING));
  EXPECT_CALL(*mock_adapter(), KillProcess(_)).WillOnce(Return(true));
}

// Test that we report the progress percent of a long-running test correctly.
TEST_F(UrandomRoutineTest, LongRunningProgressPercentage) {
  EXPECT_CALL(*mock_adapter(), StartProcess(ConstructUrandomArgs(), _))
      .WillOnce(DoAll(SetArgPointee<1>(base::GetCurrentProcessHandle()),
                      Return(true)));
  RunRoutinePastCompletion();
  EXPECT_EQ(response()->status(), grpc_api::ROUTINE_STATUS_RUNNING);
  EXPECT_EQ(response()->progress_percent(), kUrandomLongRunningProgress);
  EXPECT_CALL(*mock_adapter(), GetStatus(_))
      .WillOnce(Return(base::TERMINATION_STATUS_STILL_RUNNING));
  EXPECT_CALL(*mock_adapter(), KillProcess(_)).WillOnce(Return(true));
}

}  // namespace diagnostics
