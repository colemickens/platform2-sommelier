// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>
#include <vector>

#include <base/message_loop/message_loop.h>
#include <base/time/time.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "diagnostics/diag/diag_async_grpc_client_adapter.h"
#include "diagnostics/diag/diag_routine_requester.h"
#include "diagnosticsd.pb.h"  // NOLINT(build/include)

using ::testing::_;
using ::testing::ElementsAreArray;
using ::testing::Invoke;
using ::testing::StrictMock;
using ::testing::WithArg;

namespace diagnostics {

namespace {

using GetAvailableRoutinesCallback = base::Callback<void(
    std::unique_ptr<grpc_api::GetAvailableRoutinesResponse> response)>;

constexpr grpc_api::GetAvailableRoutinesResponse::Routine
    kFakeAvailableRoutines[] = {
        grpc_api::GetAvailableRoutinesResponse::BATTERY,
        grpc_api::GetAvailableRoutinesResponse::BATTERY_SYSFS};

class MockDiagAsyncGrpcClientAdapter : public DiagAsyncGrpcClientAdapter {
 public:
  void Connect(const std::string& target_uri) override {}

  MOCK_CONST_METHOD0(IsConnected, bool());
  MOCK_METHOD1(Shutdown, void(const base::Closure& on_shutdown));
  MOCK_METHOD2(GetAvailableRoutines,
               void(const grpc_api::GetAvailableRoutinesRequest& request,
                    GetAvailableRoutinesCallback callback));
};

}  // namespace

class DiagRoutineRequesterTest : public ::testing::Test {
 protected:
  DiagRoutineRequesterTest() {
    EXPECT_CALL(mock_adapter_, Shutdown(_))
        .WillOnce(Invoke(
            [](const base::Closure& on_shutdown) { on_shutdown.Run(); }));
  }

  // Sets the MockDiagAsyncGrpcClientAdapter to run the next callback with
  // a null pointer as its response.
  void SetNullptrAvailableRoutinesResponse() {
    EXPECT_CALL(mock_adapter_, GetAvailableRoutines(_, _))
        .WillOnce(WithArg<1>(Invoke([](GetAvailableRoutinesCallback callback) {
          callback.Run(nullptr);
        })));
  }

  // Sets the MockDiagAsyncGrpcClientAdapter to run the next callback with a
  // fake routine list as its GetAvailableRoutinesResponse.
  void SetAvailableRoutinesResponse(
      const grpc_api::GetAvailableRoutinesResponse::Routine
          available_routines[],
      const int num_routines) {
    EXPECT_CALL(mock_adapter_, GetAvailableRoutines(_, _))
        .WillRepeatedly(
            Invoke([available_routines, num_routines](
                       ::testing::Unused,
                       GetAvailableRoutinesCallback callback) mutable {
              std::unique_ptr<grpc_api::GetAvailableRoutinesResponse> reply =
                  std::make_unique<grpc_api::GetAvailableRoutinesResponse>();
              for (int i = 0; i < num_routines; i++)
                reply->add_routines(available_routines[i]);
              callback.Run(std::move(reply));
            }));
  }

  DiagRoutineRequester* routine_requester() { return routine_requester_.get(); }

 private:
  base::MessageLoopForIO message_loop_;
  StrictMock<MockDiagAsyncGrpcClientAdapter> mock_adapter_;
  std::unique_ptr<DiagRoutineRequester> routine_requester_ =
      std::make_unique<DiagRoutineRequester>(&mock_adapter_);

  DISALLOW_COPY_AND_ASSIGN(DiagRoutineRequesterTest);
};

// Test that we catch an error response.
TEST_F(DiagRoutineRequesterTest, EmptyResponse) {
  SetNullptrAvailableRoutinesResponse();

  auto response = routine_requester()->GetAvailableRoutines();
  EXPECT_EQ(response.size(), 0);
}

// Test that we can retrieve the available routines.
TEST_F(DiagRoutineRequesterTest, GetAvailableRoutines) {
  SetAvailableRoutinesResponse(
      kFakeAvailableRoutines,
      sizeof(kFakeAvailableRoutines) / sizeof(kFakeAvailableRoutines[0]));

  auto response = routine_requester()->GetAvailableRoutines();
  EXPECT_THAT(response, ElementsAreArray(kFakeAvailableRoutines));
}

}  // namespace diagnostics
