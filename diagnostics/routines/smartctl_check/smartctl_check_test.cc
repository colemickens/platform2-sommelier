// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <base/macros.h>
#include <base/process/kill.h>
#include <base/process/process_handle.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "diagnostics/routines/diag_process_adapter.h"
#include "diagnostics/routines/smartctl_check/smartctl_check.h"
#include "wilco_dtc_supportd.pb.h"  // NOLINT(build/include)

using testing::_;
using testing::DoAll;
using testing::Return;
using testing::SetArgPointee;
using testing::StrictMock;

namespace diagnostics {

namespace {

const std::vector<std::string> ConstructSmartctlCheckArgs() {
  return {kSmartctlCheckExecutableInstallLocation};
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

class SmartctlCheckRoutineTest : public testing::Test {
 protected:
  SmartctlCheckRoutineTest() {
    auto new_mock_adapter =
        std::make_unique<StrictMock<MockDiagProcessAdapter>>();
    mock_adapter_ = new_mock_adapter.get();
    routine_ = std::make_unique<SmartctlCheckRoutine>(
        params_, std::move(new_mock_adapter));
  }

  SmartctlCheckRoutine* routine() { return routine_.get(); }

  grpc_api::GetRoutineUpdateResponse* response() { return &response_; }

  StrictMock<MockDiagProcessAdapter>* mock_adapter() { return mock_adapter_; }

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
  std::unique_ptr<SmartctlCheckRoutine> routine_;
  grpc_api::GetRoutineUpdateResponse response_;
  grpc_api::SmartctlCheckRoutineParameters params_;

  DISALLOW_COPY_AND_ASSIGN(SmartctlCheckRoutineTest);
};

// Test that the smartctl_check routine fails if the smartctl_check executable
// fails to start.
TEST_F(SmartctlCheckRoutineTest, ExecutableFailsToStart) {
  EXPECT_CALL(*mock_adapter(), StartProcess(ConstructSmartctlCheckArgs(), _))
      .WillOnce(Return(false));
  routine()->Start();
  PopulateStatusUpdate();
  EXPECT_EQ(response()->status_message(),
            kSubprocRoutineFailedToLaunchProcessMessage);
  EXPECT_EQ(response()->status(), grpc_api::ROUTINE_STATUS_ERROR);
}

// Test that the smartctl_check routine fails if the executable fails.
TEST_F(SmartctlCheckRoutineTest, ExecutableReturnsFailure) {
  EXPECT_CALL(*mock_adapter(), StartProcess(ConstructSmartctlCheckArgs(), _))
      .WillOnce(DoAll(SetArgPointee<1>(base::GetCurrentProcessHandle()),
                      Return(true)));
  RunRoutineWithTerminationStatus(
      base::TERMINATION_STATUS_ABNORMAL_TERMINATION);
  EXPECT_EQ(response()->status_message(), kSubprocRoutineFailedMessage);
  EXPECT_EQ(response()->status(), grpc_api::ROUTINE_STATUS_FAILED);
}

// Test that the smartctl_check routine passes if the executable behaves as
// expected.
TEST_F(SmartctlCheckRoutineTest, NormalOperation) {
  EXPECT_CALL(*mock_adapter(), StartProcess(ConstructSmartctlCheckArgs(), _))
      .WillOnce(DoAll(SetArgPointee<1>(base::GetCurrentProcessHandle()),
                      Return(true)));
  RunRoutineWithTerminationStatus(base::TERMINATION_STATUS_STILL_RUNNING);
  EXPECT_EQ(response()->status_message(), kSubprocRoutineProcessRunningMessage);
  EXPECT_EQ(response()->status(), grpc_api::ROUTINE_STATUS_RUNNING);
  PopulateStatusUpdateForRunningRoutine(
      base::TERMINATION_STATUS_NORMAL_TERMINATION);
  EXPECT_EQ(response()->status_message(), kSubprocRoutineSucceededMessage);
  EXPECT_EQ(response()->status(), grpc_api::ROUTINE_STATUS_PASSED);
}

}  // namespace diagnostics
