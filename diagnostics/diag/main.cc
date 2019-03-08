// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>
#include <iostream>
#include <iterator>
#include <map>
#include <vector>

#include <base/logging.h>
#include <base/message_loop/message_loop.h>
#include <base/no_destructor.h>
#include <base/threading/platform_thread.h>
#include <base/time/time.h>
#include <brillo/flag_helper.h>

#include "diagnostics/constants/grpc_constants.h"
#include "diagnostics/diag/diag_routine_requester.h"
#include "wilco_dtc_supportd.pb.h"  // NOLINT(build/include)

namespace {
// Poll interval while waiting for a routine to finish.
constexpr base::TimeDelta kRoutinePollIntervalTimeDelta =
    base::TimeDelta::FromMilliseconds(100);
// Maximum time we're willing to wait for a routine to finish.
constexpr base::TimeDelta kMaximumRoutineExecutionTimeDelta =
    base::TimeDelta::FromSeconds(60);

const struct {
  const char* switch_name;
  diagnostics::grpc_api::DiagnosticRoutine routine;
} kDiagnosticRoutineSwitches[] = {
    {"battery", diagnostics::grpc_api::ROUTINE_BATTERY},
    {"urandom", diagnostics::grpc_api::ROUTINE_URANDOM}};

const struct {
  const char* readable_status;
  diagnostics::grpc_api::DiagnosticRoutineStatus status;
} kDiagnosticRoutineReadableStatuses[] = {
    {"Ready", diagnostics::grpc_api::ROUTINE_STATUS_READY},
    {"Running", diagnostics::grpc_api::ROUTINE_STATUS_RUNNING},
    {"Waiting", diagnostics::grpc_api::ROUTINE_STATUS_WAITING},
    {"Passed", diagnostics::grpc_api::ROUTINE_STATUS_PASSED},
    {"Failed", diagnostics::grpc_api::ROUTINE_STATUS_FAILED},
    {"Error", diagnostics::grpc_api::ROUTINE_STATUS_ERROR},
    {"Cancelled", diagnostics::grpc_api::ROUTINE_STATUS_CANCELLED},
    {"Failed to start", diagnostics::grpc_api::ROUTINE_STATUS_FAILED_TO_START},
    {"Removed", diagnostics::grpc_api::ROUTINE_STATUS_REMOVED}};

std::string GetSwitchFromRoutine(
    diagnostics::grpc_api::DiagnosticRoutine routine) {
  static base::NoDestructor<
      std::map<diagnostics::grpc_api::DiagnosticRoutine, std::string>>
      diagnostic_routine_to_switch;

  if (diagnostic_routine_to_switch->empty()) {
    for (const auto& item : kDiagnosticRoutineSwitches) {
      diagnostic_routine_to_switch->insert(
          std::make_pair(item.routine, item.switch_name));
    }
  }

  auto routine_itr = diagnostic_routine_to_switch->find(routine);
  LOG_IF(FATAL, routine_itr == diagnostic_routine_to_switch->end())
      << "Invalid routine to switch lookup with routine: " << routine;

  return routine_itr->second;
}

bool RunRoutineWithRequest(
    const diagnostics::grpc_api::RunRoutineRequest& request) {
  diagnostics::DiagRoutineRequester routine_requester;
  routine_requester.Connect(diagnostics::kWilcoDtcSupportdGrpcUri);

  auto routine_info = routine_requester.RunRoutine(request);

  if (!routine_info) {
    std::cout << "No RunRoutineResponse received." << std::endl;
    return false;
  }

  auto response = routine_requester.GetRoutineUpdate(
      routine_info->uuid(),
      diagnostics::grpc_api::GetRoutineUpdateRequest::GET_STATUS,
      true /* include_output */);
  const base::TimeTicks start_time = base::TimeTicks::Now();
  while (response &&
         response->status() == diagnostics::grpc_api::ROUTINE_STATUS_RUNNING &&
         base::TimeTicks::Now() <
             start_time + kMaximumRoutineExecutionTimeDelta) {
    base::PlatformThread::Sleep(kRoutinePollIntervalTimeDelta);
    std::cerr << "Progress: " << response->progress_percent() << std::endl;

    response = routine_requester.GetRoutineUpdate(
        routine_info->uuid(),
        diagnostics::grpc_api::GetRoutineUpdateRequest::GET_STATUS,
        true /* include_output */);
  }

  if (!response) {
    std::cout << "No GetRoutineUpdateResponse received." << std::endl;
    return false;
  }

  std::cout << "Routine: " << GetSwitchFromRoutine(request.routine())
            << std::endl;

  bool status_found = false;
  diagnostics::grpc_api::DiagnosticRoutineStatus status = response->status();
  for (const auto& item : kDiagnosticRoutineReadableStatuses) {
    if (item.status == status) {
      status_found = true;
      std::cout << "Status: " << item.readable_status << std::endl;
      break;
    }
  }
  LOG_IF(FATAL, !status_found)
      << "Invalid readable status lookup with status: " << status;

  std::cout << "Output: " << response->output() << std::endl;
  std::cout << "Progress: " << response->progress_percent() << std::endl;

  if (status != diagnostics::grpc_api::ROUTINE_STATUS_FAILED_TO_START) {
    response = routine_requester.GetRoutineUpdate(
        routine_info->uuid(),
        diagnostics::grpc_api::GetRoutineUpdateRequest::REMOVE,
        false /* include_output */);

    if (!response ||
        response->status() != diagnostics::grpc_api::ROUTINE_STATUS_REMOVED) {
      std::cout << "Failed to removed routine." << std::endl;
      return false;
    }
  }

  return true;
}

bool ActionGetRoutines() {
  diagnostics::DiagRoutineRequester routine_requester;
  routine_requester.Connect(diagnostics::kWilcoDtcSupportdGrpcUri);

  auto reply = routine_requester.GetAvailableRoutines();
  for (auto routine : reply) {
    std::cout << "Available routine: " << GetSwitchFromRoutine(routine)
              << std::endl;
  }

  return true;
}

bool ActionRunBatteryRoutine(int low_mah, int high_mah) {
  diagnostics::grpc_api::RunRoutineRequest request;
  request.set_routine(diagnostics::grpc_api::ROUTINE_BATTERY);
  request.mutable_battery_params()->set_low_mah(low_mah);
  request.mutable_battery_params()->set_high_mah(high_mah);
  return RunRoutineWithRequest(request);
}

bool ActionRunUrandomRoutine(int length_seconds) {
  diagnostics::grpc_api::RunRoutineRequest request;
  request.set_routine(diagnostics::grpc_api::ROUTINE_URANDOM);
  request.mutable_urandom_params()->set_length_seconds(length_seconds);
  return RunRoutineWithRequest(request);
}

}  // namespace

// 'diag' command-line tool:
//
// Test driver for libdiag. Only supports running a single diagnostic routine
// at a time.
int main(int argc, char** argv) {
  DEFINE_string(action, "",
                "Action to perform. Options are:\n\tget_routines - retrieve "
                "available routines.\n\trun_routine - run specified routine.");
  DEFINE_string(routine, "",
                "Diagnostic routine to run. For a list of available routines, "
                "run 'diag --action=get_routines'.");
  DEFINE_int32(low_mah, 1000, "Lower bound for the battery routine, in mAh.");
  DEFINE_int32(high_mah, 10000, "Upper bound for the battery routine, in mAh.");
  DEFINE_int32(length_seconds, 10,
               "Number of seconds to run the urandom routine for.");
  brillo::FlagHelper::Init(argc, argv, "diag - Device diagnostic tool.");

  logging::InitLogging(logging::LoggingSettings());

  base::MessageLoopForIO message_loop;

  if (FLAGS_action == "") {
    std::cout << "--action must be specified. Use --help for help on usage."
              << std::endl;
    return EXIT_FAILURE;
  }

  if (FLAGS_action == "get_routines")
    return ActionGetRoutines() ? EXIT_SUCCESS : EXIT_FAILURE;

  if (FLAGS_action == "run_routine") {
    std::map<std::string, diagnostics::grpc_api::DiagnosticRoutine>
        switch_to_diagnostic_routine;
    for (const auto& item : kDiagnosticRoutineSwitches)
      switch_to_diagnostic_routine[item.switch_name] = item.routine;
    auto itr = switch_to_diagnostic_routine.find(FLAGS_routine);
    if (itr == switch_to_diagnostic_routine.end()) {
      std::cout << "Unknown routine: " << FLAGS_routine << std::endl;
      return EXIT_FAILURE;
    }

    bool routine_result;
    switch (itr->second) {
      case diagnostics::grpc_api::ROUTINE_BATTERY:
        routine_result = ActionRunBatteryRoutine(FLAGS_low_mah, FLAGS_high_mah);
        break;
      case diagnostics::grpc_api::ROUTINE_URANDOM:
        routine_result = ActionRunUrandomRoutine(FLAGS_length_seconds);
        break;
      default:
        std::cout << "Unsupported routine: " << FLAGS_routine << std::endl;
        return EXIT_FAILURE;
    }

    return routine_result ? EXIT_SUCCESS : EXIT_FAILURE;
  }

  std::cout << "Unknown action: " << FLAGS_action << std::endl;
  return EXIT_FAILURE;
}
