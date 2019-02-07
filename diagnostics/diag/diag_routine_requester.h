// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DIAGNOSTICS_DIAG_DIAG_ROUTINE_REQUESTER_H_
#define DIAGNOSTICS_DIAG_DIAG_ROUTINE_REQUESTER_H_

#include <memory>
#include <string>
#include <vector>

#include <base/macros.h>

#include "diagnostics/diag/diag_async_grpc_client_adapter.h"
#include "diagnostics/diag/diag_async_grpc_client_adapter_impl.h"
#include "diagnosticsd.pb.h"  // NOLINT(build/include)

namespace diagnostics {

// Libdiag's main interface for requesting diagnostic routines be run.
class DiagRoutineRequester final {
 public:
  // All production code should use this constructor.
  DiagRoutineRequester();

  // Injects a custom implementation of the DiagAsyncGrpcClientAdapter
  // interface. This constructor should only be used for testing.
  explicit DiagRoutineRequester(DiagAsyncGrpcClientAdapter* client);

  ~DiagRoutineRequester();

  // Opens a gRPC connection on the interface specified by |target_uri_|.
  // This method should only be called a single time for each
  // DiagRoutineRequester object.
  void Connect(const std::string& target_uri);

  // Returns a list of routines that the platform is capable of running.
  std::vector<grpc_api::DiagnosticRoutine> GetAvailableRoutines();

  // Request that a diagnostic routine be run on the platform.
  std::unique_ptr<grpc_api::RunRoutineResponse> RunRoutine(
      const grpc_api::RunRoutineRequest& request);

  // Get the status of an existing test, or send an existing test a command.
  std::unique_ptr<grpc_api::GetRoutineUpdateResponse> GetRoutineUpdate(
      int uuid,
      grpc_api::GetRoutineUpdateRequest::Command command,
      bool include_output);

 private:
  // Gracefully shut down the DiagAsyncGrpcClientAdapter.
  void ShutdownClient();

  // Owned default implementation, used when no DiagAsyncGrpcClientAdapter
  // implementation is provided in the constructor.
  std::unique_ptr<DiagAsyncGrpcClientAdapterImpl> owned_client_impl_;
  // Unowned pointer to the chosen DiagAsyncGrpcClientAdapter implementation.
  DiagAsyncGrpcClientAdapter* client_impl_;

  DISALLOW_COPY_AND_ASSIGN(DiagRoutineRequester);
};

}  // namespace diagnostics

#endif  // DIAGNOSTICS_DIAG_DIAG_ROUTINE_REQUESTER_H_
