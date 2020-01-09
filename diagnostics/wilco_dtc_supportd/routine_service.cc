// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "diagnostics/wilco_dtc_supportd/routine_service.h"

#include <iterator>
#include <string>
#include <utility>

#include "diagnostics/common/mojo_utils.h"
#include "mojo/cros_healthd.mojom.h"
#include "mojo/cros_healthd_diagnostics.mojom.h"

namespace diagnostics {

namespace {

// When the battery routine's low_mah and high_mah parameters are not set, we
// default to low_mah = 1000mAh and high_mah = 10000mAh.
constexpr int kRoutineBatteryDefaultLowmAh = 1000;
constexpr int kRoutineBatteryDefaultHighmAh = 10000;

// Converts from mojo's DiagnosticRoutineStatusEnum to gRPC's
// DiagnosticRoutineStatus.
bool GetGrpcStatusFromMojoStatus(
    chromeos::cros_healthd::mojom::DiagnosticRoutineStatusEnum mojo_status,
    grpc_api::DiagnosticRoutineStatus* grpc_status_out) {
  DCHECK(grpc_status_out);
  switch (mojo_status) {
    case chromeos::cros_healthd::mojom::DiagnosticRoutineStatusEnum::kReady:
      *grpc_status_out = grpc_api::ROUTINE_STATUS_READY;
      return true;
    case chromeos::cros_healthd::mojom::DiagnosticRoutineStatusEnum::kRunning:
      *grpc_status_out = grpc_api::ROUTINE_STATUS_RUNNING;
      return true;
    case chromeos::cros_healthd::mojom::DiagnosticRoutineStatusEnum::kWaiting:
      *grpc_status_out = grpc_api::ROUTINE_STATUS_WAITING;
      return true;
    case chromeos::cros_healthd::mojom::DiagnosticRoutineStatusEnum::kPassed:
      *grpc_status_out = grpc_api::ROUTINE_STATUS_PASSED;
      return true;
    case chromeos::cros_healthd::mojom::DiagnosticRoutineStatusEnum::kFailed:
      *grpc_status_out = grpc_api::ROUTINE_STATUS_FAILED;
      return true;
    case chromeos::cros_healthd::mojom::DiagnosticRoutineStatusEnum::kError:
      *grpc_status_out = grpc_api::ROUTINE_STATUS_ERROR;
      return true;
    case chromeos::cros_healthd::mojom::DiagnosticRoutineStatusEnum::kCancelled:
      *grpc_status_out = grpc_api::ROUTINE_STATUS_CANCELLED;
      return true;
    case chromeos::cros_healthd::mojom::DiagnosticRoutineStatusEnum::
        kFailedToStart:
      *grpc_status_out = grpc_api::ROUTINE_STATUS_FAILED_TO_START;
      return true;
    case chromeos::cros_healthd::mojom::DiagnosticRoutineStatusEnum::kRemoved:
      *grpc_status_out = grpc_api::ROUTINE_STATUS_REMOVED;
      return true;
    case chromeos::cros_healthd::mojom::DiagnosticRoutineStatusEnum::
        kCancelling:
      *grpc_status_out = grpc_api::ROUTINE_STATUS_CANCELLING;
      return true;
  }
  LOG(ERROR) << "Unknown mojo routine status: "
             << static_cast<int>(mojo_status);
  return false;
}

// Converts from mojo's DiagnosticRoutineUserMessageEnum to gRPC's
// DiagnosticRoutineUserMessage.
bool GetUserMessageFromMojoEnum(
    chromeos::cros_healthd::mojom::DiagnosticRoutineUserMessageEnum
        mojo_message,
    grpc_api::DiagnosticRoutineUserMessage* grpc_message_out) {
  DCHECK(grpc_message_out);
  switch (mojo_message) {
    case chromeos::cros_healthd::mojom::DiagnosticRoutineUserMessageEnum::
        kUnplugACPower:
      *grpc_message_out = grpc_api::ROUTINE_USER_MESSAGE_UNPLUG_AC_POWER;
      return true;
  }
  LOG(ERROR) << "Unknown mojo user message: " << static_cast<int>(mojo_message);
  return false;
}

// Converts from mojo's DiagnosticRoutineEnum to gRPC's DiagnosticRoutine.
bool GetGrpcRoutineEnumFromMojoRoutineEnum(
    chromeos::cros_healthd::mojom::DiagnosticRoutineEnum mojo_enum,
    grpc_api::DiagnosticRoutine* grpc_enum_out) {
  DCHECK(grpc_enum_out);
  switch (mojo_enum) {
    case chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kBatteryCapacity:
      *grpc_enum_out = grpc_api::ROUTINE_BATTERY;
      return true;
    case chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kBatteryHealth:
      *grpc_enum_out = grpc_api::ROUTINE_BATTERY_SYSFS;
      return true;
    case chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kUrandom:
      *grpc_enum_out = grpc_api::ROUTINE_URANDOM;
      return true;
    case chromeos::cros_healthd::mojom::DiagnosticRoutineEnum::kSmartctlCheck:
      *grpc_enum_out = grpc_api::ROUTINE_SMARTCTL_CHECK;
      return true;
  }
  LOG(ERROR) << "Unknown mojo routine: " << static_cast<int>(mojo_enum);
  return false;
}

// Converts from mojo's RoutineUpdate to gRPC's GetRoutineUpdateResponse.
void SetGrpcUpdateFromMojoUpdate(
    chromeos::cros_healthd::mojom::RoutineUpdatePtr mojo_update,
    grpc_api::GetRoutineUpdateResponse* grpc_update) {
  DCHECK(grpc_update);
  grpc_update->set_progress_percent(mojo_update->progress_percent);
  const auto& update_union = mojo_update->routine_update_union;
  if (update_union->is_interactive_update()) {
    grpc_api::DiagnosticRoutineUserMessage grpc_message;
    chromeos::cros_healthd::mojom::DiagnosticRoutineUserMessageEnum
        mojo_message = update_union->get_interactive_update()->user_message;
    if (!GetUserMessageFromMojoEnum(mojo_message, &grpc_message)) {
      grpc_update->set_status(grpc_api::ROUTINE_STATUS_ERROR);
    } else {
      grpc_update->set_user_message(grpc_message);
    }
  } else {
    grpc_update->set_status_message(
        update_union->get_noninteractive_update()->status_message);
    grpc_api::DiagnosticRoutineStatus grpc_status;
    auto mojo_status = update_union->get_noninteractive_update()->status;
    if (!GetGrpcStatusFromMojoStatus(mojo_status, &grpc_status)) {
      grpc_update->set_status(grpc_api::ROUTINE_STATUS_ERROR);
    } else {
      grpc_update->set_status(grpc_status);
    }
  }

  if (!mojo_update->output.is_valid()) {
    // This isn't necessarily an error, since some requests may not have
    // specified that they wanted output returned, and some routines may never
    // return any extra input. We'll log the event in the case that it was an
    // error.
    VLOG(1) << "No output in mojo update.";
    return;
  }

  auto shared_memory =
      GetReadOnlySharedMemoryFromMojoHandle(std::move(mojo_update->output));
  if (!shared_memory) {
    PLOG(ERROR) << "Failed to read data from mojo handle";
    return;
  }
  grpc_update->set_output(
      std::string(static_cast<const char*>(shared_memory->memory()),
                  shared_memory->mapped_size()));
}

// Converts from gRPC's GetRoutineUpdateRequest::Command to mojo's
// DiagnosticRoutineCommandEnum.
bool GetMojoCommandFromGrpcCommand(
    grpc_api::GetRoutineUpdateRequest::Command grpc_command,
    chromeos::cros_healthd::mojom::DiagnosticRoutineCommandEnum*
        mojo_command_out) {
  DCHECK(mojo_command_out);
  switch (grpc_command) {
    case grpc_api::GetRoutineUpdateRequest::RESUME:
      *mojo_command_out = chromeos::cros_healthd::mojom::
          DiagnosticRoutineCommandEnum::kContinue;
      return true;
    case grpc_api::GetRoutineUpdateRequest::CANCEL:
      *mojo_command_out =
          chromeos::cros_healthd::mojom::DiagnosticRoutineCommandEnum::kCancel;
      return true;
    case grpc_api::GetRoutineUpdateRequest::GET_STATUS:
      *mojo_command_out = chromeos::cros_healthd::mojom::
          DiagnosticRoutineCommandEnum::kGetStatus;
      return true;
    case grpc_api::GetRoutineUpdateRequest::REMOVE:
      *mojo_command_out =
          chromeos::cros_healthd::mojom::DiagnosticRoutineCommandEnum::kRemove;
      return true;
    default:
      LOG(ERROR) << "Unknown gRPC command: " << static_cast<int>(grpc_command);
      return false;
  }
}

// Forwards and wraps the result of a GetAvailableRoutines call into a gRPC
// response.
void ForwardGetAvailableRoutinesResponse(
    const RoutineService::GetAvailableRoutinesToServiceCallback& callback,
    const std::vector<chromeos::cros_healthd::mojom::DiagnosticRoutineEnum>&
        mojo_routines) {
  std::vector<grpc_api::DiagnosticRoutine> grpc_routines;
  for (auto mojo_routine : mojo_routines) {
    grpc_api::DiagnosticRoutine grpc_routine;
    if (GetGrpcRoutineEnumFromMojoRoutineEnum(mojo_routine, &grpc_routine))
      grpc_routines.push_back(grpc_routine);
  }
  callback.Run(std::move(grpc_routines), grpc_api::ROUTINE_SERVICE_STATUS_OK);
}

// Forwards and wraps the result of a RunRoutine call into a gRPC response.
void ForwardRunRoutineResponse(
    const RoutineService::RunRoutineToServiceCallback& callback,
    chromeos::cros_healthd::mojom::RunRoutineResponsePtr response) {
  grpc_api::DiagnosticRoutineStatus grpc_status;
  chromeos::cros_healthd::mojom::DiagnosticRoutineStatusEnum mojo_status =
      response->status;
  if (!GetGrpcStatusFromMojoStatus(mojo_status, &grpc_status)) {
    callback.Run(0 /* uuid */, grpc_api::ROUTINE_STATUS_ERROR,
                 grpc_api::ROUTINE_SERVICE_STATUS_OK);
    return;
  }
  callback.Run(response->id, grpc_status, grpc_api::ROUTINE_SERVICE_STATUS_OK);
}

// Forwards and wraps the result of a GetRoutineUpdate call into a gRPC
// response.
void ForwardGetRoutineUpdateResponse(
    int uuid,
    const RoutineService::GetRoutineUpdateRequestToServiceCallback& callback,
    chromeos::cros_healthd::mojom::RoutineUpdatePtr response) {
  grpc_api::GetRoutineUpdateResponse grpc_response;
  SetGrpcUpdateFromMojoUpdate(std::move(response), &grpc_response);
  callback.Run(uuid, grpc_response.status(), grpc_response.progress_percent(),
               grpc_response.user_message(), grpc_response.output(),
               grpc_response.status_message(),
               grpc_api::ROUTINE_SERVICE_STATUS_OK);
}

}  // namespace

RoutineService::RoutineService(Delegate* delegate) : delegate_(delegate) {
  DCHECK(delegate_);
}

RoutineService::~RoutineService() = default;

void RoutineService::GetAvailableRoutines(
    const GetAvailableRoutinesToServiceCallback& callback) {
  if (!BindCrosHealthdDiagnosticsServiceIfNeeded()) {
    LOG(WARNING) << "GetAvailableRoutines called before mojo was bootstrapped.";
    callback.Run(std::vector<grpc_api::DiagnosticRoutine>{},
                 grpc_api::ROUTINE_SERVICE_STATUS_UNAVAILABLE);
    return;
  }

  service_ptr_->GetAvailableRoutines(
      base::Bind(&ForwardGetAvailableRoutinesResponse, callback));
}

void RoutineService::RunRoutine(const grpc_api::RunRoutineRequest& request,
                                const RunRoutineToServiceCallback& callback) {
  if (!BindCrosHealthdDiagnosticsServiceIfNeeded()) {
    LOG(WARNING) << "RunRoutine called before mojo was bootstrapped.";
    callback.Run(0 /* uuid */, grpc_api::ROUTINE_STATUS_FAILED_TO_START,
                 grpc_api::ROUTINE_SERVICE_STATUS_UNAVAILABLE);
    return;
  }

  switch (request.routine()) {
    case grpc_api::ROUTINE_BATTERY:
      DCHECK_EQ(request.parameters_case(),
                grpc_api::RunRoutineRequest::kBatteryParams);
      service_ptr_->RunBatteryCapacityRoutine(
          request.battery_params().low_mah()
              ? request.battery_params().low_mah()
              : kRoutineBatteryDefaultLowmAh,
          request.battery_params().high_mah()
              ? request.battery_params().high_mah()
              : kRoutineBatteryDefaultHighmAh,
          base::Bind(&ForwardRunRoutineResponse, callback));
      break;
    case grpc_api::ROUTINE_BATTERY_SYSFS:
      DCHECK_EQ(request.parameters_case(),
                grpc_api::RunRoutineRequest::kBatterySysfsParams);
      service_ptr_->RunBatteryHealthRoutine(
          request.battery_sysfs_params().maximum_cycle_count(),
          request.battery_sysfs_params().percent_battery_wear_allowed(),
          base::Bind(&ForwardRunRoutineResponse, callback));
      break;
    case grpc_api::ROUTINE_URANDOM:
      DCHECK_EQ(request.parameters_case(),
                grpc_api::RunRoutineRequest::kUrandomParams);
      service_ptr_->RunUrandomRoutine(
          request.urandom_params().length_seconds(),
          base::Bind(&ForwardRunRoutineResponse, callback));
      break;
    case grpc_api::ROUTINE_SMARTCTL_CHECK:
      DCHECK_EQ(request.parameters_case(),
                grpc_api::RunRoutineRequest::kSmartctlCheckParams);
      service_ptr_->RunSmartctlCheckRoutine(
          base::Bind(&ForwardRunRoutineResponse, callback));
      break;
    default:
      LOG(ERROR) << "RunRoutineRequest routine not set or unrecognized.";
      callback.Run(0 /* uuid */, grpc_api::ROUTINE_STATUS_INVALID_FIELD,
                   grpc_api::ROUTINE_SERVICE_STATUS_OK);
  }
}

void RoutineService::GetRoutineUpdate(
    int uuid,
    grpc_api::GetRoutineUpdateRequest::Command command,
    bool include_output,
    const GetRoutineUpdateRequestToServiceCallback& callback) {
  if (!BindCrosHealthdDiagnosticsServiceIfNeeded()) {
    LOG(WARNING) << "GetRoutineUpdate called before mojo was bootstrapped.";
    callback.Run(uuid, grpc_api::ROUTINE_STATUS_ERROR, 0 /* progress_percent */,
                 grpc_api::ROUTINE_USER_MESSAGE_UNSET, "" /* output */,
                 "" /* status_message */,
                 grpc_api::ROUTINE_SERVICE_STATUS_UNAVAILABLE);
    return;
  }

  chromeos::cros_healthd::mojom::DiagnosticRoutineCommandEnum mojo_command;
  if (!GetMojoCommandFromGrpcCommand(command, &mojo_command)) {
    callback.Run(uuid, grpc_api::ROUTINE_STATUS_INVALID_FIELD,
                 0 /* progress_percent */, grpc_api::ROUTINE_USER_MESSAGE_UNSET,
                 "" /* output */, "" /* status_message */,
                 grpc_api::ROUTINE_SERVICE_STATUS_OK);
    return;
  }

  service_ptr_->GetRoutineUpdate(
      uuid, mojo_command, include_output,
      base::Bind(&ForwardGetRoutineUpdateResponse, uuid, callback));
}

bool RoutineService::BindCrosHealthdDiagnosticsServiceIfNeeded() {
  if (service_ptr_.is_bound())
    return true;

  if (!delegate_->GetCrosHealthdDiagnosticsService(
          mojo::MakeRequest(&service_ptr_))) {
    return false;
  }

  service_ptr_.set_connection_error_handler(
      base::Bind(&RoutineService::OnDisconnect, base::Unretained(this)));
  return true;
}

void RoutineService::OnDisconnect() {
  VLOG(1) << "cros_healthd Mojo connection closed.";
  service_ptr_.reset();
}

}  // namespace diagnostics
