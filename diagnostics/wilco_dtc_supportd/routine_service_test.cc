// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <tuple>
#include <vector>

#include <base/bind.h>
#include <base/callback.h>
#include <base/message_loop/message_loop.h>
#include <base/run_loop.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "diagnostics/wilco_dtc_supportd/fake_routine_factory.h"
#include "diagnostics/wilco_dtc_supportd/routine_service.h"
#include "mojo/cros_healthd_diagnostics.mojom.h"

using testing::ElementsAreArray;

namespace diagnostics {
namespace mojo_ipc = ::chromeos::cros_healthd::mojom;

namespace {
constexpr char kRoutineDoesNotExistOutput[] =
    "Specified routine does not exist.";

constexpr grpc_api::DiagnosticRoutine kAvailableRoutines[] = {
    grpc_api::ROUTINE_BATTERY, grpc_api::ROUTINE_BATTERY_SYSFS,
    grpc_api::ROUTINE_SMARTCTL_CHECK, grpc_api::ROUTINE_URANDOM};

void CopyAvailableRoutines(
    base::Closure callback,
    std::vector<grpc_api::DiagnosticRoutine>* routines,
    const std::vector<grpc_api::DiagnosticRoutine>& returned_routines) {
  routines->assign(returned_routines.begin(), returned_routines.end());
  callback.Run();
}

void SaveRunRoutineResponse(base::Closure callback,
                            grpc_api::RunRoutineResponse* response,
                            int uuid,
                            grpc_api::DiagnosticRoutineStatus status) {
  response->set_uuid(uuid);
  response->set_status(status);
  callback.Run();
}

void SaveGetRoutineUpdateResponse(
    base::Closure callback,
    grpc_api::GetRoutineUpdateResponse* response,
    int uuid,
    grpc_api::DiagnosticRoutineStatus status,
    int progress_percent,
    grpc_api::DiagnosticRoutineUserMessage user_message,
    const std::string& output,
    const std::string& status_message) {
  response->set_uuid(uuid);
  response->set_status(status);
  response->set_progress_percent(progress_percent);
  response->set_user_message(user_message);
  response->set_output(output);
  response->set_status_message(status_message);
  callback.Run();
}

// Tests for the RoutineService class.
class RoutineServiceTest : public testing::Test {
 protected:
  RoutineServiceTest() = default;

  RoutineService* service() { return &service_; }

  FakeRoutineFactory* routine_factory() { return &routine_factory_; }

  void SetAvailableRoutines() {
    std::vector<grpc_api::DiagnosticRoutine> routines_to_add;
    for (auto routine : kAvailableRoutines)
      routines_to_add.push_back(routine);
    service_.SetAvailableRoutinesForTesting(routines_to_add);
  }

  std::vector<grpc_api::DiagnosticRoutine> ExecuteGetAvailableRoutines() {
    std::vector<grpc_api::DiagnosticRoutine> routines;
    base::RunLoop run_loop;
    service_.GetAvailableRoutines(
        base::Bind(&CopyAvailableRoutines, run_loop.QuitClosure(), &routines));
    run_loop.Run();
    return routines;
  }

  grpc_api::RunRoutineResponse ExecuteRunRoutine() {
    base::RunLoop run_loop;
    grpc_api::RunRoutineRequest request;
    grpc_api::RunRoutineResponse response;
    service_.RunRoutine(request, base::Bind(&SaveRunRoutineResponse,
                                            run_loop.QuitClosure(), &response));
    run_loop.Run();
    return response;
  }

  grpc_api::GetRoutineUpdateResponse ExecuteGetRoutineUpdate(
      const int uuid,
      const grpc_api::GetRoutineUpdateRequest::Command command,
      const bool include_output) {
    base::RunLoop run_loop;
    grpc_api::GetRoutineUpdateResponse response;
    service_.GetRoutineUpdate(uuid, command, include_output,
                              base::Bind(&SaveGetRoutineUpdateResponse,
                                         run_loop.QuitClosure(), &response));
    run_loop.Run();
    return response;
  }

 private:
  base::MessageLoop message_loop_;
  FakeRoutineFactory routine_factory_;
  RoutineService service_{&routine_factory_};
};

// Test that GetAvailableRoutines returns the expected list of routines.
TEST_F(RoutineServiceTest, GetAvailableRoutines) {
  SetAvailableRoutines();
  auto reply = ExecuteGetAvailableRoutines();
  EXPECT_THAT(reply, ElementsAreArray(kAvailableRoutines));
}

// Test that getting the status of a routine that doesn't exist returns an
// error.
TEST_F(RoutineServiceTest, NonExistingStatus) {
  auto response = ExecuteGetRoutineUpdate(
      0 /* uuid */, grpc_api::GetRoutineUpdateRequest::GET_STATUS,
      false /* include_output */);
  EXPECT_EQ(response.status(), grpc_api::ROUTINE_STATUS_ERROR);
  EXPECT_EQ(response.status_message(), kRoutineDoesNotExistOutput);
}

// Test that a routine can be run.
TEST_F(RoutineServiceTest, RunRoutine) {
  routine_factory()->SetNonInteractiveStatus(
      mojo_ipc::DiagnosticRoutineStatusEnum::kRunning, "" /* status_message */,
      50 /* progress_percent */, "" /* output */);
  auto response = ExecuteRunRoutine();
  EXPECT_EQ(response.status(), grpc_api::ROUTINE_STATUS_RUNNING);
}

// Test that the routine service handles the routine factory failing to create a
// routine.
TEST_F(RoutineServiceTest, BadRoutineFactory) {
  auto response = ExecuteRunRoutine();
  EXPECT_EQ(response.status(), grpc_api::ROUTINE_STATUS_FAILED_TO_START);
  EXPECT_EQ(response.uuid(), 0);
}

// Test that a routine reporting an invalid user message is handled sanely.
TEST_F(RoutineServiceTest, InvalidUserMessage) {
  routine_factory()->SetInteractiveStatus(
      static_cast<mojo_ipc::DiagnosticRoutineUserMessageEnum>(
          -1) /* user_message */,
      0 /* progress_percent */, "" /* output */);
  auto run_routine_response = ExecuteRunRoutine();
  auto update_response =
      ExecuteGetRoutineUpdate(run_routine_response.uuid(),
                              grpc_api::GetRoutineUpdateRequest::GET_STATUS,
                              false /* include_output */);
  EXPECT_EQ(update_response.status(), grpc_api::ROUTINE_STATUS_ERROR);
}

// Test that a routine reporting an invalid status is handled sanely.
TEST_F(RoutineServiceTest, InvalidStatus) {
  routine_factory()->SetNonInteractiveStatus(
      static_cast<mojo_ipc::DiagnosticRoutineStatusEnum>(-1) /* status */,
      "" /* status_message */, 0 /* progress_percent */, "" /* output */);
  auto run_routine_response = ExecuteRunRoutine();
  auto update_response =
      ExecuteGetRoutineUpdate(run_routine_response.uuid(),
                              grpc_api::GetRoutineUpdateRequest::GET_STATUS,
                              false /* include_output */);
  EXPECT_EQ(update_response.status(), grpc_api::ROUTINE_STATUS_ERROR);
}

// Test that an invalid command passed to the routine service is handled sanely.
TEST_F(RoutineServiceTest, InvalidCommand) {
  routine_factory()->SetNonInteractiveStatus(
      mojo_ipc::DiagnosticRoutineStatusEnum::kReady /* status */,
      "" /* status_message */, 0 /* progress_percent */, "" /* output */);
  auto run_routine_response = ExecuteRunRoutine();
  auto update_response = ExecuteGetRoutineUpdate(
      run_routine_response.uuid(),
      static_cast<grpc_api::GetRoutineUpdateRequest::Command>(-1),
      false /* include_output */);
  EXPECT_EQ(update_response.status(), grpc_api::ROUTINE_STATUS_ERROR);
}

// Test that after a routine has been removed, we cannot access its data.
TEST_F(RoutineServiceTest, AccessStoppedRoutine) {
  routine_factory()->SetNonInteractiveStatus(
      mojo_ipc::DiagnosticRoutineStatusEnum::kRunning, "" /* status_message */,
      50 /* progress_percent */, "" /* output */);
  auto run_routine_response = ExecuteRunRoutine();
  ExecuteGetRoutineUpdate(run_routine_response.uuid(),
                          grpc_api::GetRoutineUpdateRequest::REMOVE,
                          false /* include_output */);
  auto update_response = ExecuteGetRoutineUpdate(
      run_routine_response.uuid(),
      grpc_api::GetRoutineUpdateRequest::GET_STATUS, true /* include_output */);
  EXPECT_EQ(update_response.status(), grpc_api::ROUTINE_STATUS_ERROR);
  EXPECT_EQ(update_response.status_message(), kRoutineDoesNotExistOutput);
}

// Tests for the GetRoutineUpdate() method of RoutineService when an
// interactive update is returned.
//
// This is a parameterized test with the following parameters:
// * |mojo_message| - mojo's DiagnosticRoutineUserMessageEnum returned in the
//                    routine's update.
// * |grpc_message| - gRPC's DiagnosticRoutineUserMessage expected to be
//                    returned by the routine service.
class GetInteractiveUpdateTest
    : public RoutineServiceTest,
      public testing::WithParamInterface<
          std::tuple<mojo_ipc::DiagnosticRoutineUserMessageEnum,
                     grpc_api::DiagnosticRoutineUserMessage>> {
 protected:
  // Accessors to individual test parameters from the test parameter tuple
  // returned by gtest's GetParam():

  mojo_ipc::DiagnosticRoutineUserMessageEnum mojo_message() const {
    return std::get<0>(GetParam());
  }

  grpc_api::DiagnosticRoutineUserMessage grpc_message() const {
    return std::get<1>(GetParam());
  }
};

// Test that after a routine has started, we can access its interactive data.
TEST_P(GetInteractiveUpdateTest, AccessInteractiveRunningRoutine) {
  constexpr uint32_t kExpectedProgressPercent = 17;
  constexpr char kExpectedOutput[] = "Expected output.";
  routine_factory()->SetInteractiveStatus(
      mojo_message(), kExpectedProgressPercent, kExpectedOutput);
  auto run_routine_response = ExecuteRunRoutine();
  auto update_response = ExecuteGetRoutineUpdate(
      run_routine_response.uuid(),
      grpc_api::GetRoutineUpdateRequest::GET_STATUS, true /* include_output */);
  EXPECT_EQ(update_response.user_message(), grpc_message());
  EXPECT_EQ(update_response.progress_percent(), kExpectedProgressPercent);
  EXPECT_EQ(update_response.output(), kExpectedOutput);
}

INSTANTIATE_TEST_CASE_P(
    ,
    GetInteractiveUpdateTest,
    testing::Values(std::make_tuple(
        mojo_ipc::DiagnosticRoutineUserMessageEnum::kUnplugACPower,
        grpc_api::ROUTINE_USER_MESSAGE_UNPLUG_AC_POWER)));

// Tests for the GetRoutineUpdate() method of RoutineService when a
// noninteractive update is returned.
//
// This is a parameterized test with the following parameters:
// * |mojo_status| - mojo's DiagnosticRoutineStatusEnum returned in the
//                   routine's update.
// * |grpc_status| - gRPC's DiagnosticRoutineStatus expected to be
//                   returned by the routine service.
class GetNoninteractiveUpdateTest
    : public RoutineServiceTest,
      public testing::WithParamInterface<
          std::tuple<mojo_ipc::DiagnosticRoutineStatusEnum,
                     grpc_api::DiagnosticRoutineStatus>> {
 protected:
  // Accessors to individual test parameters from the test parameter tuple
  // returned by gtest's GetParam():

  mojo_ipc::DiagnosticRoutineStatusEnum mojo_status() const {
    return std::get<0>(GetParam());
  }

  grpc_api::DiagnosticRoutineStatus grpc_status() const {
    return std::get<1>(GetParam());
  }
};

// Test that after a routine has started, we can access its noninteractive data.
TEST_P(GetNoninteractiveUpdateTest, AccessNoninteractiveRunningRoutine) {
  constexpr char kExpectedStatusMessage[] = "Expected status message.";
  constexpr uint32_t kExpectedProgressPercent = 18;
  constexpr char kExpectedOutput[] = "Expected output.";
  routine_factory()->SetNonInteractiveStatus(
      mojo_status(), kExpectedStatusMessage, kExpectedProgressPercent,
      kExpectedOutput);
  auto run_routine_response = ExecuteRunRoutine();
  auto update_response = ExecuteGetRoutineUpdate(
      run_routine_response.uuid(),
      grpc_api::GetRoutineUpdateRequest::GET_STATUS, true /* include_output */);
  EXPECT_EQ(update_response.status(), grpc_status());
  EXPECT_EQ(update_response.status_message(), kExpectedStatusMessage);
  EXPECT_EQ(update_response.progress_percent(), kExpectedProgressPercent);
  EXPECT_EQ(update_response.output(), kExpectedOutput);
}

INSTANTIATE_TEST_CASE_P(
    ,
    GetNoninteractiveUpdateTest,
    testing::Values(
        std::make_tuple(mojo_ipc::DiagnosticRoutineStatusEnum::kReady,
                        grpc_api::ROUTINE_STATUS_READY),
        std::make_tuple(mojo_ipc::DiagnosticRoutineStatusEnum::kRunning,
                        grpc_api::ROUTINE_STATUS_RUNNING),
        std::make_tuple(mojo_ipc::DiagnosticRoutineStatusEnum::kWaiting,
                        grpc_api::ROUTINE_STATUS_WAITING),
        std::make_tuple(mojo_ipc::DiagnosticRoutineStatusEnum::kPassed,
                        grpc_api::ROUTINE_STATUS_PASSED),
        std::make_tuple(mojo_ipc::DiagnosticRoutineStatusEnum::kFailed,
                        grpc_api::ROUTINE_STATUS_FAILED),
        std::make_tuple(mojo_ipc::DiagnosticRoutineStatusEnum::kError,
                        grpc_api::ROUTINE_STATUS_ERROR),
        std::make_tuple(mojo_ipc::DiagnosticRoutineStatusEnum::kCancelled,
                        grpc_api::ROUTINE_STATUS_CANCELLED),
        std::make_tuple(mojo_ipc::DiagnosticRoutineStatusEnum::kFailedToStart,
                        grpc_api::ROUTINE_STATUS_FAILED_TO_START),
        std::make_tuple(mojo_ipc::DiagnosticRoutineStatusEnum::kRemoved,
                        grpc_api::ROUTINE_STATUS_REMOVED),
        std::make_tuple(mojo_ipc::DiagnosticRoutineStatusEnum::kCancelling,
                        grpc_api::ROUTINE_STATUS_CANCELLING)));

// Tests for the GetRoutineUpdate() method of RoutineService with different
// commands.
//
// This is a parameterized test with the following parameters:
// * |command| - gRPC's GetRoutineUpdateRequest::Command included in the
//               GetRoutineUpdateRequest.
class GetRoutineUpdateCommandTest
    : public RoutineServiceTest,
      public testing::WithParamInterface<
          grpc_api::GetRoutineUpdateRequest::Command> {
 protected:
  // Accessors to the test parameter returned by gtest's GetParam():

  grpc_api::GetRoutineUpdateRequest::Command command() const {
    return GetParam();
  }
};

// Test that we can send the given command.
TEST_P(GetRoutineUpdateCommandTest, SendCommand) {
  constexpr char kExpectedStatusMessage[] = "Expected status message.";
  constexpr uint32_t kExpectedProgressPercent = 19;
  constexpr char kExpectedOutput[] = "Expected output.";
  constexpr mojo_ipc::DiagnosticRoutineStatusEnum kMojoStatus =
      mojo_ipc::DiagnosticRoutineStatusEnum::kRunning;
  constexpr grpc_api::DiagnosticRoutineStatus kExpectedStatus =
      grpc_api::ROUTINE_STATUS_RUNNING;
  routine_factory()->SetNonInteractiveStatus(
      kMojoStatus, kExpectedStatusMessage, kExpectedProgressPercent,
      kExpectedOutput);
  auto run_routine_response = ExecuteRunRoutine();
  auto update_response = ExecuteGetRoutineUpdate(
      run_routine_response.uuid(), command(), true /* include_output */);
  EXPECT_EQ(update_response.status(), kExpectedStatus);
  EXPECT_EQ(update_response.status_message(), kExpectedStatusMessage);
  EXPECT_EQ(update_response.progress_percent(), kExpectedProgressPercent);
  EXPECT_EQ(update_response.output(), kExpectedOutput);
}

INSTANTIATE_TEST_CASE_P(
    ,
    GetRoutineUpdateCommandTest,
    testing::Values(grpc_api::GetRoutineUpdateRequest::RESUME,
                    grpc_api::GetRoutineUpdateRequest::CANCEL));

}  // namespace

}  // namespace diagnostics
