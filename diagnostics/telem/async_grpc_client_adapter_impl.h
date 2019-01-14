// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DIAGNOSTICS_TELEM_ASYNC_GRPC_CLIENT_ADAPTER_IMPL_H_
#define DIAGNOSTICS_TELEM_ASYNC_GRPC_CLIENT_ADAPTER_IMPL_H_

#include <memory>
#include <string>

#include <base/macros.h>

#include "diagnostics/grpc_async_adapter/async_grpc_client.h"
#include "diagnostics/telem/async_grpc_client_adapter.h"
#include "diagnosticsd.grpc.pb.h"  // NOLINT(build/include)

namespace diagnostics {

// Production implementation of AsyncGrpcClientAdapter.
class AsyncGrpcClientAdapterImpl final : public AsyncGrpcClientAdapter {
 public:
  AsyncGrpcClientAdapterImpl();
  ~AsyncGrpcClientAdapterImpl() override;
  bool IsConnected() const override;
  void Connect(const std::string& target_uri) override;
  void Shutdown(const base::Closure& on_shutdown) override;
  void GetProcData(const grpc_api::GetProcDataRequest& request,
                   base::Callback<void(
                       std::unique_ptr<grpc_api::GetProcDataResponse> response)>
                       callback) override;
  void GetSysfsData(
      const grpc_api::GetSysfsDataRequest& request,
      base::Callback<void(std::unique_ptr<grpc_api::GetSysfsDataResponse>
                              response)> callback) override;

 private:
  std::unique_ptr<AsyncGrpcClient<grpc_api::Diagnosticsd>> client_;

  DISALLOW_COPY_AND_ASSIGN(AsyncGrpcClientAdapterImpl);
};

}  // namespace diagnostics

#endif  // DIAGNOSTICS_TELEM_ASYNC_GRPC_CLIENT_ADAPTER_IMPL_H_
