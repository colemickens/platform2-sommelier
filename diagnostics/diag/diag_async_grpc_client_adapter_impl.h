// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DIAGNOSTICS_DIAG_DIAG_ASYNC_GRPC_CLIENT_ADAPTER_IMPL_H_
#define DIAGNOSTICS_DIAG_DIAG_ASYNC_GRPC_CLIENT_ADAPTER_IMPL_H_

#include <memory>
#include <string>

#include <base/macros.h>

#include "diagnostics/diag/diag_async_grpc_client_adapter.h"
#include "diagnostics/grpc_async_adapter/async_grpc_client.h"
#include "diagnosticsd.grpc.pb.h"  // NOLINT(build/include)

namespace diagnostics {

// Production implementation of DiagAsyncGrpcClientAdapter.
class DiagAsyncGrpcClientAdapterImpl final : public DiagAsyncGrpcClientAdapter {
 public:
  DiagAsyncGrpcClientAdapterImpl();
  ~DiagAsyncGrpcClientAdapterImpl() override;
  bool IsConnected() const override;
  void Connect(const std::string& target_uri) override;
  void Shutdown(const base::Closure& on_shutdown) override;
  void GetAvailableRoutines(
      const grpc_api::GetAvailableRoutinesRequest& request,
      base::Callback<void(
          std::unique_ptr<grpc_api::GetAvailableRoutinesResponse> response)>
          callback) override;
  void RunRoutine(
      const grpc_api::RunRoutineRequest& request,
      base::Callback<void(std::unique_ptr<grpc_api::RunRoutineResponse>)>
          callback) override;
  void GetRoutineUpdate(
      const grpc_api::GetRoutineUpdateRequest& request,
      base::Callback<void(std::unique_ptr<grpc_api::GetRoutineUpdateResponse>)>
          callback) override;

 private:
  std::unique_ptr<AsyncGrpcClient<grpc_api::Diagnosticsd>> client_;

  DISALLOW_COPY_AND_ASSIGN(DiagAsyncGrpcClientAdapterImpl);
};

}  // namespace diagnostics

#endif  // DIAGNOSTICS_DIAG_DIAG_ASYNC_GRPC_CLIENT_ADAPTER_IMPL_H_
