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
  return false;
}

// Converts from mojo's RoutineUpdate to gRPC's GetRoutineUpdateResponse.
void SetGrpcUpdateFromMojoUpdate(
    chromeos::cros_healthd::mojom::RoutineUpdate* mojo_update,
    grpc_api::GetRoutineUpdateResponse* grpc_update) {
  DCHECK(grpc_update);
  grpc_update->set_progress_percent(mojo_update->progress_percent);
  const auto& update_union = mojo_update->routine_update_union;
  if (update_union->is_interactive_update()) {
    grpc_api::DiagnosticRoutineUserMessage grpc_message;
    chromeos::cros_healthd::mojom::DiagnosticRoutineUserMessageEnum
        mojo_message = update_union->get_interactive_update()->user_message;
    if (!GetUserMessageFromMojoEnum(mojo_message, &grpc_message)) {
      LOG(ERROR) << "Unknown mojo user message: "
                 << static_cast<int>(mojo_message);
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
      LOG(ERROR) << "Unknown mojo routine status: "
                 << static_cast<int>(mojo_status);
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
    VLOG(0) << "No output in mojo update.";
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
}  // namespace

RoutineService::RoutineService() {
  routine_factory_impl_ = std::make_unique<RoutineFactoryImpl>();
  routine_factory_ = routine_factory_impl_.get();
}

RoutineService::RoutineService(RoutineFactory* routine_factory)
    : routine_factory_(routine_factory) {}

RoutineService::~RoutineService() = default;

void RoutineService::GetAvailableRoutines(
    const GetAvailableRoutinesToServiceCallback& callback) {
  callback.Run(available_routines_);
}

void RoutineService::SetAvailableRoutinesForTesting(
    const std::vector<grpc_api::DiagnosticRoutine>& available_routines) {
  available_routines_ = available_routines;
}

void RoutineService::RunRoutine(const grpc_api::RunRoutineRequest& request,
                                const RunRoutineToServiceCallback& callback) {
  auto new_routine = routine_factory_->CreateRoutine(request);

  if (!new_routine) {
    callback.Run(0 /* uuid */, grpc_api::ROUTINE_STATUS_FAILED_TO_START);
    return;
  }

  new_routine->Start();
  int uuid = next_uuid_;
  DCHECK(active_routines_.find(uuid) == active_routines_.end());
  active_routines_[uuid] = std::move(new_routine);
  ++next_uuid_;

  grpc_api::DiagnosticRoutineStatus grpc_status;
  auto mojo_status = active_routines_[uuid]->GetStatus();
  if (!GetGrpcStatusFromMojoStatus(mojo_status, &grpc_status)) {
    LOG(ERROR) << "Unknown mojo routine status: "
               << static_cast<int>(mojo_status);
    callback.Run(uuid, grpc_api::ROUTINE_STATUS_ERROR);
    return;
  }
  callback.Run(uuid, grpc_status);
}

void RoutineService::GetRoutineUpdate(
    int uuid,
    grpc_api::GetRoutineUpdateRequest::Command command,
    bool include_output,
    const GetRoutineUpdateRequestToServiceCallback& callback) {
  auto itr = active_routines_.find(uuid);
  if (itr == active_routines_.end()) {
    LOG(ERROR) << "Bad uuid in GetRoutineUpdateRequest.";
    callback.Run(uuid, grpc_api::ROUTINE_STATUS_ERROR, 0 /* progress_percent */,
                 grpc_api::ROUTINE_USER_MESSAGE_UNSET, "" /* output */,
                 "Specified routine does not exist.");
    return;
  }

  auto* routine = itr->second.get();
  grpc_api::GetRoutineUpdateResponse response;
  chromeos::cros_healthd::mojom::RoutineUpdate update{
      0, mojo::ScopedHandle(),
      chromeos::cros_healthd::mojom::RoutineUpdateUnion::New()};
  switch (command) {
    case grpc_api::GetRoutineUpdateRequest::RESUME:
      routine->Resume();
      break;
    case grpc_api::GetRoutineUpdateRequest::CANCEL:
      routine->Cancel();
      break;
    case grpc_api::GetRoutineUpdateRequest::GET_STATUS:
      // Retrieving the status and output of a routine is handled below.
      break;
    case grpc_api::GetRoutineUpdateRequest::REMOVE:
      routine->PopulateStatusUpdate(&update, include_output);
      SetGrpcUpdateFromMojoUpdate(&update, &response);
      response.set_status(grpc_api::ROUTINE_STATUS_REMOVED);
      active_routines_.erase(itr);
      // |routine| is invalid at this point!
      callback.Run(uuid, response.status(), response.progress_percent(),
                   response.user_message(), response.output(),
                   response.status_message());
      return;
    default:
      LOG(ERROR) << "Invalid command: " << static_cast<int>(command)
                 << " in GetRoutineUpdateRequest";
      routine->PopulateStatusUpdate(&update, include_output);
      SetGrpcUpdateFromMojoUpdate(&update, &response);
      callback.Run(uuid, grpc_api::ROUTINE_STATUS_ERROR,
                   response.progress_percent(), response.user_message(),
                   response.output(), response.status_message());
      return;
  }

  routine->PopulateStatusUpdate(&update, include_output);
  SetGrpcUpdateFromMojoUpdate(&update, &response);

  callback.Run(uuid, response.status(), response.progress_percent(),
               response.user_message(), response.output(),
               response.status_message());
}

}  // namespace diagnostics
