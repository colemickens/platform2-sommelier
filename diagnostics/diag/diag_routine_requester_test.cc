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
#include "wilco_dtc_supportd.pb.h"  // NOLINT(build/include)

using ::testing::_;
using ::testing::ElementsAreArray;
using ::testing::Invoke;
using ::testing::StrictMock;
using ::testing::WithArg;

namespace diagnostics {

namespace {

using GetAvailableRoutinesCallback = base::Callback<void(
    std::unique_ptr<grpc_api::GetAvailableRoutinesResponse> response)>;
using RunRoutineCallback = base::Callback<void(
    std::unique_ptr<grpc_api::RunRoutineResponse> response)>;
using GetRoutineUpdateCallback = base::Callback<void(
    std::unique_ptr<grpc_api::GetRoutineUpdateResponse> response)>;

constexpr grpc_api::DiagnosticRoutine kFakeAvailableRoutines[] = {
    grpc_api::ROUTINE_BATTERY, grpc_api::ROUTINE_BATTERY_SYSFS};

constexpr int kExpectedUuid = 89769;
constexpr grpc_api::DiagnosticRoutineStatus kExpectedStatus =
    grpc_api::ROUTINE_STATUS_RUNNING;
constexpr int kExpectedProgressPercent = 37;
constexpr grpc_api::DiagnosticRoutineUserMessage kExpectedUserMessage =
    grpc_api::ROUTINE_USER_MESSAGE_UNSET;
constexpr char kExpectedOutput[] = "Sample output.";

class MockDiagAsyncGrpcClientAdapter : public DiagAsyncGrpcClientAdapter {
 public:
  void Connect(const std::string& target_uri) override {}

  MOCK_CONST_METHOD0(IsConnected, bool());
  MOCK_METHOD1(Shutdown, void(const base::Closure& on_shutdown));
  MOCK_METHOD2(GetAvailableRoutines,
               void(const grpc_api::GetAvailableRoutinesRequest& request,
                    GetAvailableRoutinesCallback callback));
  MOCK_METHOD2(RunRoutine,
               void(const grpc_api::RunRoutineRequest& request,
                    RunRoutineCallback callback));
  MOCK_METHOD2(GetRoutineUpdate,
               void(const grpc_api::GetRoutineUpdateRequest& request,
                    GetRoutineUpdateCallback callback));
};

grpc_api::RunRoutineRequest ConstructRunBatteryRoutineRequest(int low_mah,
                                                              int high_mah) {
  grpc_api::RunRoutineRequest request;
  request.set_routine(grpc_api::ROUTINE_BATTERY);
  request.mutable_battery_params()->set_low_mah(low_mah);
  request.mutable_battery_params()->set_high_mah(high_mah);
  return request;
}

grpc_api::RunRoutineRequest ConstructRunBatterySysfsRoutineRequest(
    int maximum_cycle_count, int percent_battery_wear_allowed) {
  grpc_api::RunRoutineRequest request;
  request.set_routine(grpc_api::ROUTINE_BATTERY_SYSFS);
  request.mutable_battery_sysfs_params()->set_maximum_cycle_count(
      maximum_cycle_count);
  request.mutable_battery_sysfs_params()->set_percent_battery_wear_allowed(
      percent_battery_wear_allowed);
  return request;
}

grpc_api::RunRoutineRequest ConstructRunUrandomRoutineRequest(
    int length_seconds) {
  grpc_api::RunRoutineRequest request;
  request.set_routine(grpc_api::ROUTINE_URANDOM);
  request.mutable_urandom_params()->set_length_seconds(length_seconds);
  return request;
}

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
  void SetNullptrRunRoutineResponse() {
    EXPECT_CALL(mock_adapter_, RunRoutine(_, _))
        .WillOnce(WithArg<1>(Invoke(
            [](RunRoutineCallback callback) { callback.Run(nullptr); })));
  }
  void SetNullptrGetRoutineUpdateResponse() {
    EXPECT_CALL(mock_adapter_, GetRoutineUpdate(_, _))
        .WillOnce(WithArg<1>(Invoke(
            [](GetRoutineUpdateCallback callback) { callback.Run(nullptr); })));
  }

  // Sets the MockDiagAsyncGrpcClientAdapter to run the next callback with a
  // fake routine list as its GetAvailableRoutinesResponse.
  void SetAvailableRoutinesResponse() {
    EXPECT_CALL(mock_adapter_, GetAvailableRoutines(_, _))
        .WillRepeatedly(WithArg<1>(
            Invoke([](GetAvailableRoutinesCallback callback) mutable {
              std::unique_ptr<grpc_api::GetAvailableRoutinesResponse> reply =
                  std::make_unique<grpc_api::GetAvailableRoutinesResponse>();
              for (auto routine : kFakeAvailableRoutines)
                reply->add_routines(routine);
              callback.Run(std::move(reply));
            })));
  }

  // Sets the MockDiagAsyncGrpcClientAdapter to run the next callback with a
  // fake uuid and status as its RunRoutineResponse.
  void SetRunRoutineResponse(int uuid,
                             grpc_api::DiagnosticRoutineStatus status) {
    EXPECT_CALL(mock_adapter_, RunRoutine(_, _))
        .WillRepeatedly(WithArg<1>(
            Invoke([uuid, status](RunRoutineCallback callback) mutable {
              auto reply = std::make_unique<grpc_api::RunRoutineResponse>();
              reply->set_uuid(uuid);
              reply->set_status(status);
              callback.Run(std::move(reply));
            })));
  }

  // Sets the MockDiagAsyncGrpcClientAdapter to run the next callback with a
  // fake GetRoutineUpdateResponse.
  void SetGetRoutineUpdateResponse(
      int uuid,
      grpc_api::DiagnosticRoutineStatus status,
      int progress_percent,
      grpc_api::DiagnosticRoutineUserMessage user_message,
      const std::string& output) {
    EXPECT_CALL(mock_adapter_, GetRoutineUpdate(_, _))
        .WillRepeatedly(Invoke(
            [uuid, status, progress_percent, user_message, output](
                ::testing::Unused, GetRoutineUpdateCallback callback) mutable {
              auto reply =
                  std::make_unique<grpc_api::GetRoutineUpdateResponse>();
              reply->set_uuid(uuid);
              reply->set_status(status);
              reply->set_progress_percent(progress_percent);
              reply->set_user_message(user_message);
              reply->set_output(output);
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

// Test that we handle an empty GetAvailableTests response.
TEST_F(DiagRoutineRequesterTest, EmptyGetAvailableTestsResponse) {
  SetNullptrAvailableRoutinesResponse();

  auto response = routine_requester()->GetAvailableRoutines();
  EXPECT_EQ(response, base::nullopt);
}

// Test that we handle an empty RunRoutine response.
TEST_F(DiagRoutineRequesterTest, EmptyRunRoutineResponse) {
  SetNullptrRunRoutineResponse();

  grpc_api::RunRoutineRequest request;
  auto response = routine_requester()->RunRoutine(request);
  EXPECT_EQ(response, nullptr);
}

// Test that we handle an empty GetRoutineUpdate response.
TEST_F(DiagRoutineRequesterTest, EmptyGetRoutineUpdateResponse) {
  SetNullptrGetRoutineUpdateResponse();

  auto response = routine_requester()->GetRoutineUpdate(
      kExpectedUuid, grpc_api::GetRoutineUpdateRequest::GET_STATUS, true);
  EXPECT_EQ(response, nullptr);
}

// Test that we can retrieve the available routines.
TEST_F(DiagRoutineRequesterTest, GetAvailableRoutines) {
  SetAvailableRoutinesResponse();

  auto response = routine_requester()->GetAvailableRoutines();
  EXPECT_NE(response, base::nullopt);
  EXPECT_THAT(response.value(), ElementsAreArray(kFakeAvailableRoutines));
}

// Test that we can run the battery routine.
TEST_F(DiagRoutineRequesterTest, RunBatteryRoutine) {
  SetRunRoutineResponse(kExpectedUuid, kExpectedStatus);
  auto response =
      routine_requester()->RunRoutine(ConstructRunBatteryRoutineRequest(0, 10));
  ASSERT_TRUE(response);
  EXPECT_EQ(response->uuid(), kExpectedUuid);
  EXPECT_EQ(response->status(), kExpectedStatus);
}

// Test that we can run the battery_sysfs routine.
TEST_F(DiagRoutineRequesterTest, RunBatterySysfsRoutine) {
  SetRunRoutineResponse(kExpectedUuid, kExpectedStatus);
  auto response =
      routine_requester()->RunRoutine(ConstructRunBatterySysfsRoutineRequest(
          5 /* maximum_cycle_count */, 90 /* percent_battery_wear_allowed */));
  ASSERT_TRUE(response);
  EXPECT_EQ(response->uuid(), kExpectedUuid);
  EXPECT_EQ(response->status(), kExpectedStatus);
}

// Test that we can run the urandom routine.
TEST_F(DiagRoutineRequesterTest, RunUrandomRoutine) {
  SetRunRoutineResponse(kExpectedUuid, kExpectedStatus);
  auto response =
      routine_requester()->RunRoutine(ConstructRunUrandomRoutineRequest(10));
  ASSERT_TRUE(response);
  EXPECT_EQ(response->uuid(), kExpectedUuid);
  EXPECT_EQ(response->status(), kExpectedStatus);
}

// Test that we can send a command to an existing routine.
TEST_F(DiagRoutineRequesterTest, GetRoutineUpdate) {
  SetRunRoutineResponse(kExpectedUuid, kExpectedStatus);
  EXPECT_TRUE(routine_requester()->RunRoutine(
      ConstructRunBatteryRoutineRequest(0, 10)));
  SetGetRoutineUpdateResponse(kExpectedUuid, kExpectedStatus,
                              kExpectedProgressPercent, kExpectedUserMessage,
                              kExpectedOutput);
  auto reply = routine_requester()->GetRoutineUpdate(
      kExpectedUuid, grpc_api::GetRoutineUpdateRequest::GET_STATUS, true);
  ASSERT_TRUE(reply);
  EXPECT_EQ(reply->uuid(), kExpectedUuid);
  EXPECT_EQ(reply->status(), kExpectedStatus);
  EXPECT_EQ(reply->progress_percent(), kExpectedProgressPercent);
  EXPECT_EQ(reply->user_message(), kExpectedUserMessage);
  EXPECT_EQ(reply->output(), kExpectedOutput);
}

}  // namespace diagnostics
