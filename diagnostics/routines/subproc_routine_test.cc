// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "diagnostics/routines/subproc_routine.h"

#include <utility>
#include <vector>

#include <base/macros.h>
#include <base/command_line.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "diagnostics/routines/diag_process_adapter.h"

namespace diagnostics {

namespace {

using ::testing::_;
using ::testing::Return;
using ::testing::StrictMock;
using ::testing::SetArgPointee;
using ::testing::Test;

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
  class FakeSubprocRoutineImpl : public SubprocRoutine {
   public:
    explicit FakeSubprocRoutineImpl(
        std::unique_ptr<DiagProcessAdapter> process_adapter)
        : SubprocRoutine(std::move(process_adapter)) {}

    base::CommandLine GetCommandLine() const override {
      return base::CommandLine({"/dev/null"});
    }
  };

 protected:
  SubprocRoutineTest() {
    auto new_mock_adapter =
        std::make_unique<StrictMock<MockDiagProcessAdapter>>();
    mock_adapter_ = new_mock_adapter.get();
    routine_ =
        std::make_unique<FakeSubprocRoutineImpl>(std::move(new_mock_adapter));
  }

  SubprocRoutine* routine() { return routine_.get(); }

  StrictMock<MockDiagProcessAdapter>* mock_adapter() { return mock_adapter_; }

 private:
  StrictMock<MockDiagProcessAdapter>* mock_adapter_;  // Owned by |routine_|.
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

  EXPECT_EQ(response.progress_percent(), kSubprocRoutineFakeProgressPercent);
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

  EXPECT_EQ(response.progress_percent(), kSubprocRoutineFakeProgressPercent);
  EXPECT_EQ(response.status_message(), kSubprocRoutineFailedMessage);
  EXPECT_EQ(response.status(), grpc_api::ROUTINE_STATUS_FAILED);
}

TEST_F(SubprocRoutineTest, FailInvokingSubproc) {
  EXPECT_CALL(*mock_adapter(), StartProcess(_, _)).WillOnce(Return(false));
  routine()->Start();

  EXPECT_EQ(routine()->GetStatus(), grpc_api::ROUTINE_STATUS_ERROR);
}

}  // namespace
}  // namespace diagnostics
