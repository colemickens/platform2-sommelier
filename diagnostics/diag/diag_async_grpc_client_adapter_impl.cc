// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "diagnostics/diag/diag_async_grpc_client_adapter_impl.h"

#include <base/logging.h>
#include <base/threading/thread_task_runner_handle.h>

namespace diagnostics {

DiagAsyncGrpcClientAdapterImpl::DiagAsyncGrpcClientAdapterImpl() = default;

DiagAsyncGrpcClientAdapterImpl::~DiagAsyncGrpcClientAdapterImpl() = default;

bool DiagAsyncGrpcClientAdapterImpl::IsConnected() const {
  // A connection is defined as having an existing DiagAsyncGrpcClient,
  // because when an DiagAsyncGrpcClient exists, the adapter is listening
  // over some gRPC URI.
  return client_ != nullptr;
}

void DiagAsyncGrpcClientAdapterImpl::Connect(const std::string& target_uri) {
  DCHECK(!client_);

  // Create the DiagAsyncGrpcClient, listening over the specified gRPC URI.
  client_ = std::make_unique<AsyncGrpcClient<grpc_api::Diagnosticsd>>(
      base::ThreadTaskRunnerHandle::Get(), target_uri);
  VLOG(0) << "Created gRPC wilco_dtc_supportd client on " << target_uri;
}

void DiagAsyncGrpcClientAdapterImpl::Shutdown(
    const base::Closure& on_shutdown) {
  client_->Shutdown(on_shutdown);
}

void DiagAsyncGrpcClientAdapterImpl::GetAvailableRoutines(
    const grpc_api::GetAvailableRoutinesRequest& request,
    base::Callback<void(std::unique_ptr<grpc_api::GetAvailableRoutinesResponse>
                            response)> callback) {
  client_->CallRpc(&grpc_api::Diagnosticsd::Stub::AsyncGetAvailableRoutines,
                   request, callback);
}

void DiagAsyncGrpcClientAdapterImpl::RunRoutine(
    const grpc_api::RunRoutineRequest& request,
    base::Callback<void(std::unique_ptr<grpc_api::RunRoutineResponse>)>
        callback) {
  client_->CallRpc(&grpc_api::Diagnosticsd::Stub::AsyncRunRoutine, request,
                   callback);
}

void DiagAsyncGrpcClientAdapterImpl::GetRoutineUpdate(
    const grpc_api::GetRoutineUpdateRequest& request,
    base::Callback<void(std::unique_ptr<grpc_api::GetRoutineUpdateResponse>)>
        callback) {
  client_->CallRpc(&grpc_api::Diagnosticsd::Stub::AsyncGetRoutineUpdate,
                   request, callback);
}

}  // namespace diagnostics
