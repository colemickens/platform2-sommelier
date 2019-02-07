// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "diagnostics/diag/diag_routine_requester.h"

#include <utility>

#include <base/bind.h>
#include <base/logging.h>
#include <base/run_loop.h>

namespace diagnostics {

namespace {
// Generic callback which moves the response to its destination. All
// specific parsing logic is left to other methods, allowing this
// single callback to be used for each gRPC request.
template <typename ResponseType>
void OnRpcResponseReceived(std::unique_ptr<ResponseType>* response_destination,
                           base::Closure run_loop_quit_closure,
                           std::unique_ptr<ResponseType> response) {
  *response_destination = std::move(response);
  run_loop_quit_closure.Run();
}

}  // namespace

DiagRoutineRequester::DiagRoutineRequester() {
  owned_client_impl_ = std::make_unique<DiagAsyncGrpcClientAdapterImpl>();
  client_impl_ = owned_client_impl_.get();
}

DiagRoutineRequester::DiagRoutineRequester(DiagAsyncGrpcClientAdapter* client)
    : client_impl_(client) {}

DiagRoutineRequester::~DiagRoutineRequester() {
  ShutdownClient();
}

void DiagRoutineRequester::Connect(const std::string& target_uri) {
  DCHECK(!client_impl_->IsConnected());

  client_impl_->Connect(target_uri);
}

std::vector<grpc_api::DiagnosticRoutine>
DiagRoutineRequester::GetAvailableRoutines() {
  grpc_api::GetAvailableRoutinesRequest request;
  std::unique_ptr<grpc_api::GetAvailableRoutinesResponse> response;
  base::RunLoop run_loop;

  client_impl_->GetAvailableRoutines(
      request,
      base::Bind(&OnRpcResponseReceived<grpc_api::GetAvailableRoutinesResponse>,
                 base::Unretained(&response), run_loop.QuitClosure()));
  VLOG(0) << "Sent GetAvailableRoutinesRequest.";
  run_loop.Run();

  if (!response) {
    LOG(ERROR) << "No GetAvailableRoutinesResponse received.";
    return std::vector<grpc_api::DiagnosticRoutine>();
  }

  std::vector<grpc_api::DiagnosticRoutine> available_routines;
  for (int i = 0; i < response->routines_size(); i++)
    available_routines.push_back(response->routines(i));

  return available_routines;
}

std::unique_ptr<grpc_api::RunRoutineResponse> DiagRoutineRequester::RunRoutine(
    const grpc_api::RunRoutineRequest& request) {
  std::unique_ptr<grpc_api::RunRoutineResponse> response;
  base::RunLoop run_loop;

  client_impl_->RunRoutine(
      request, base::Bind(&OnRpcResponseReceived<grpc_api::RunRoutineResponse>,
                          base::Unretained(&response), run_loop.QuitClosure()));
  VLOG(0) << "Sent RunRoutineRequest.";
  run_loop.Run();

  if (!response)
    LOG(ERROR) << "No RunRoutineResponse received.";

  return response;
}

std::unique_ptr<grpc_api::GetRoutineUpdateResponse>
DiagRoutineRequester::GetRoutineUpdate(
    int uuid,
    grpc_api::GetRoutineUpdateRequest::Command command,
    bool include_output) {
  grpc_api::GetRoutineUpdateRequest request;
  request.set_uuid(uuid);
  request.set_command(command);
  request.set_include_output(include_output);
  std::unique_ptr<grpc_api::GetRoutineUpdateResponse> response;
  base::RunLoop run_loop;

  client_impl_->GetRoutineUpdate(
      request,
      base::Bind(&OnRpcResponseReceived<grpc_api::GetRoutineUpdateResponse>,
                 base::Unretained(&response), run_loop.QuitClosure()));
  VLOG(0) << "Sent GetRoutineUpdateRequest.";
  run_loop.Run();

  if (!response) {
    LOG(ERROR) << "No GetRoutineUpdateResponse received.";
  }

  return response;
}

void DiagRoutineRequester::ShutdownClient() {
  base::RunLoop loop;
  client_impl_->Shutdown(loop.QuitClosure());
  loop.Run();
}

}  // namespace diagnostics
