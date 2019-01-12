// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DIAGNOSTICS_DIAG_DIAG_ASYNC_GRPC_CLIENT_ADAPTER_H_
#define DIAGNOSTICS_DIAG_DIAG_ASYNC_GRPC_CLIENT_ADAPTER_H_

#include <memory>
#include <string>

#include <base/callback.h>

#include "diagnosticsd.grpc.pb.h"  // NOLINT(build/include)

namespace diagnostics {

// Provides an interface for connecting to and communicating
// with the diagnosticsd daemon.
class DiagAsyncGrpcClientAdapter {
 public:
  virtual ~DiagAsyncGrpcClientAdapter() = default;

  // Whether or not the DiagAsyncGrpcClientAdapter is currently
  // connected to the diagnosticsd daemon.
  virtual bool IsConnected() const = 0;

  // Connects to the diagnosticsd daemon. This method should
  // only be called a single time. The resulting connection
  // lasts for the lifetime of the DiagAsyncGrpcClientAdapter.
  virtual void Connect(const std::string& target_uri) = 0;

  // Gracefully shutdown the connection to the diagnosticsd
  // daemon.
  virtual void Shutdown(const base::Closure& on_shutdown) = 0;

  // Retrieve a list of routines that the platform supports.
  virtual void GetAvailableRoutines(
      const grpc_api::GetAvailableRoutinesRequest& request,
      base::Callback<void(
          std::unique_ptr<grpc_api::GetAvailableRoutinesResponse> response)>
          callback) = 0;
};

}  // namespace diagnostics

#endif  // DIAGNOSTICS_DIAG_DIAG_ASYNC_GRPC_CLIENT_ADAPTER_H_
