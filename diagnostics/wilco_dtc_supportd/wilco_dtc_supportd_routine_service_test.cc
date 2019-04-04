// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include <base/bind.h>
#include <base/callback.h>
#include <base/message_loop/message_loop.h>
#include <base/run_loop.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "diagnostics/wilco_dtc_supportd/fake_wilco_dtc_supportd_routine_factory.h"
#include "diagnostics/wilco_dtc_supportd/wilco_dtc_supportd_routine_service.h"

using testing::ElementsAreArray;

namespace diagnostics {

namespace {
constexpr char kInvalidRoutineOutput[] = "Specified routine does not exist.";

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

}  // namespace

// Tests for the WilcoDtcSupportdRoutineService class.
class WilcoDtcSupportdRoutineServiceTest : public testing::Test {
 protected:
  WilcoDtcSupportdRoutineServiceTest() = default;

  WilcoDtcSupportdRoutineService* service() { return &service_; }

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
  FakeWilcoDtcSupportdRoutineFactory routine_factory_;
  WilcoDtcSupportdRoutineService service_{&routine_factory_};
};

// Test that GetAvailableRoutines returns the expected list of routines.
TEST_F(WilcoDtcSupportdRoutineServiceTest, GetAvailableRoutines) {
  SetAvailableRoutines();
  auto reply = ExecuteGetAvailableRoutines();
  EXPECT_THAT(reply, ElementsAreArray(kAvailableRoutines));
}

// Test that getting the status of a routine that doesn't exist returns an
// error.
TEST_F(WilcoDtcSupportdRoutineServiceTest, BadRoutineStatus) {
  auto response = ExecuteGetRoutineUpdate(
      0 /* uuid */, grpc_api::GetRoutineUpdateRequest::GET_STATUS,
      false /* include_output */);
  EXPECT_EQ(response.status(), grpc_api::ROUTINE_STATUS_ERROR);
  EXPECT_EQ(response.status_message(), kInvalidRoutineOutput);
}

// Test that a routine can be run.
TEST_F(WilcoDtcSupportdRoutineServiceTest, RunRoutine) {
  auto response = ExecuteRunRoutine();
  EXPECT_EQ(response.status(), grpc_api::ROUTINE_STATUS_RUNNING);
}

// Test that after a routine has started, we can access its data.
TEST_F(WilcoDtcSupportdRoutineServiceTest, AccessRunningRoutine) {
  auto run_routine_response = ExecuteRunRoutine();
  auto update_response =
      ExecuteGetRoutineUpdate(run_routine_response.uuid(),
                              grpc_api::GetRoutineUpdateRequest::GET_STATUS,
                              false /* include_output */);
  EXPECT_EQ(update_response.status(), grpc_api::ROUTINE_STATUS_RUNNING);
}

// Test that after a routine has been removed, we cannot access its data.
TEST_F(WilcoDtcSupportdRoutineServiceTest, AccessStoppedRoutine) {
  auto run_routine_response = ExecuteRunRoutine();
  ExecuteGetRoutineUpdate(run_routine_response.uuid(),
                          grpc_api::GetRoutineUpdateRequest::REMOVE,
                          false /* include_output */);
  auto update_response = ExecuteGetRoutineUpdate(
      run_routine_response.uuid(),
      grpc_api::GetRoutineUpdateRequest::GET_STATUS, true /* include_output */);
  EXPECT_EQ(update_response.status(), grpc_api::ROUTINE_STATUS_ERROR);
  EXPECT_EQ(update_response.status_message(), kInvalidRoutineOutput);
}

}  // namespace diagnostics
