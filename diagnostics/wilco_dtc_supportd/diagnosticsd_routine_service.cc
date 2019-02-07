// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "diagnostics/wilco_dtc_supportd/diagnosticsd_routine_service.h"

#include <iterator>
#include <utility>

namespace diagnostics {

DiagnosticsdRoutineService::DiagnosticsdRoutineService() {
  routine_factory_impl_ = std::make_unique<DiagnosticsdRoutineFactoryImpl>();
  routine_factory_ = routine_factory_impl_.get();
}

DiagnosticsdRoutineService::DiagnosticsdRoutineService(
    DiagnosticsdRoutineFactory* routine_factory)
    : routine_factory_(routine_factory) {}

DiagnosticsdRoutineService::~DiagnosticsdRoutineService() = default;

void DiagnosticsdRoutineService::GetAvailableRoutines(
    const GetAvailableRoutinesToServiceCallback& callback) {
  callback.Run(available_routines_);
}

void DiagnosticsdRoutineService::SetAvailableRoutinesForTesting(
    const std::vector<grpc_api::DiagnosticRoutine>& available_routines) {
  available_routines_ = available_routines;
}

void DiagnosticsdRoutineService::RunRoutine(
    const grpc_api::RunRoutineRequest& request,
    const RunRoutineToServiceCallback& callback) {
  auto new_routine = routine_factory_->CreateRoutine(request);

  if (!new_routine) {
    callback.Run(0 /* uuid */, grpc_api::ROUTINE_STATUS_FAILED_TO_START);
    return;
  }

  new_routine->Start();
  int uuid = next_uuid_;
  active_routines_[uuid] = std::move(new_routine);
  next_uuid_++;
  callback.Run(uuid, active_routines_[uuid]->GetStatus());
}

void DiagnosticsdRoutineService::GetRoutineUpdate(
    int uuid,
    grpc_api::GetRoutineUpdateRequest::Command command,
    bool include_output,
    const GetRoutineUpdateRequestToServiceCallback& callback) {
  auto itr = active_routines_.find(uuid);
  if (itr == active_routines_.end()) {
    LOG(ERROR) << "Bad uuid in GetRoutineUpdateRequest.";
    callback.Run(uuid, grpc_api::ROUTINE_STATUS_ERROR, 0 /* progress_percent */,
                 grpc_api::ROUTINE_USER_MESSAGE_UNSET,
                 "Specified routine does not exist.");
    return;
  }

  auto* routine = itr->second.get();
  grpc_api::GetRoutineUpdateResponse response;
  switch (command) {
    case grpc_api::GetRoutineUpdateRequest::PAUSE:
      routine->Pause();
      break;
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
      routine->PopulateStatusUpdate(&response, include_output);
      response.set_status(grpc_api::ROUTINE_STATUS_REMOVED);
      active_routines_.erase(itr);
      // |routine| is invalid at this point!
      callback.Run(uuid, response.status(), response.progress_percent(),
                   response.user_message(), response.output());
      return;
    default:
      LOG(ERROR) << "Invalid command in GetRoutineUpdateRequest.";
      routine->PopulateStatusUpdate(&response, include_output);
      callback.Run(uuid, grpc_api::ROUTINE_STATUS_ERROR,
                   response.progress_percent(), response.user_message(),
                   response.output());
      return;
  }

  routine->PopulateStatusUpdate(&response, include_output);

  callback.Run(uuid, response.status(), response.progress_percent(),
               response.user_message(), response.output());
}

}  // namespace diagnostics
