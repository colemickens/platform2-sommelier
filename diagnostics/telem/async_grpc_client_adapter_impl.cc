// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "diagnostics/telem/async_grpc_client_adapter_impl.h"

#include <base/logging.h>
#include <base/threading/thread_task_runner_handle.h>

namespace diagnostics {

AsyncGrpcClientAdapterImpl::AsyncGrpcClientAdapterImpl() = default;

AsyncGrpcClientAdapterImpl::~AsyncGrpcClientAdapterImpl() = default;

bool AsyncGrpcClientAdapterImpl::IsConnected() const {
  // A connection is defined as having an existing AsyncGrpcClient,
  // because when an AsyncGrpcClient exists, the adapter is listening
  // over some gRPC URI.
  return client_ != nullptr;
}

void AsyncGrpcClientAdapterImpl::Connect(const std::string& target_uri) {
  DCHECK(!client_);

  // Create the AsyncGrpcClient, listening over the specified gRPC URI.
  client_ = std::make_unique<AsyncGrpcClient<grpc_api::WilcoDtcSupportd>>(
      base::ThreadTaskRunnerHandle::Get(), target_uri);
  VLOG(0) << "Created gRPC wilco_dtc_supportd client on " << target_uri;
}

void AsyncGrpcClientAdapterImpl::Shutdown(const base::Closure& on_shutdown) {
  client_->Shutdown(on_shutdown);
}

void AsyncGrpcClientAdapterImpl::GetProcData(
    const grpc_api::GetProcDataRequest& request,
    base::Callback<void(
        std::unique_ptr<grpc_api::GetProcDataResponse> response)> callback) {
  client_->CallRpc(&grpc_api::WilcoDtcSupportd::Stub::AsyncGetProcData, request,
                   callback);
}

void AsyncGrpcClientAdapterImpl::GetSysfsData(
    const grpc_api::GetSysfsDataRequest& request,
    base::Callback<void(
        std::unique_ptr<grpc_api::GetSysfsDataResponse> response)> callback) {
  client_->CallRpc(&grpc_api::WilcoDtcSupportd::Stub::AsyncGetSysfsData,
                   request, callback);
}

}  // namespace diagnostics
