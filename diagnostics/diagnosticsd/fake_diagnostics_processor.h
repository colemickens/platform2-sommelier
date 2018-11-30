// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DIAGNOSTICS_DIAGNOSTICSD_FAKE_DIAGNOSTICS_PROCESSOR_H_
#define DIAGNOSTICS_DIAGNOSTICSD_FAKE_DIAGNOSTICS_PROCESSOR_H_

#include <memory>
#include <string>

#include <base/callback.h>
#include <base/macros.h>
#include <base/optional.h>

#include "diagnostics/grpc_async_adapter/async_grpc_client.h"
#include "diagnostics/grpc_async_adapter/async_grpc_server.h"

#include "diagnostics_processor.grpc.pb.h"  // NOLINT(build/include)
#include "diagnosticsd.grpc.pb.h"           // NOLINT(build/include)

namespace diagnostics {

// Helper class that allows to test gRPC communication between
// diagnostics daemon and diagnostics processor.
//
// This class runs a "DiagnosticsProcessor" gRPC server on the given
// |grpc_server_uri| URI, and a gRPC client to the "Diagnosticsd" gRPC service
// on the |diagnosticsd_grpc_uri| gRPC URI.
class FakeDiagnosticsProcessor final {
 public:
  using GetProcDataCallback =
      base::Callback<void(std::unique_ptr<grpc_api::GetProcDataResponse>)>;
  using RunEcCommandCallback =
      base::Callback<void(std::unique_ptr<grpc_api::RunEcCommandResponse>)>;
  using GetEcPropertyCallback =
      base::Callback<void(std::unique_ptr<grpc_api::GetEcPropertyResponse>)>;
  using HandleMessageFromUiCallback = base::Callback<void(
      std::unique_ptr<grpc_api::HandleMessageFromUiResponse>)>;

  FakeDiagnosticsProcessor(const std::string& grpc_server_uri,
                           const std::string& diagnosticsd_grpc_uri);
  ~FakeDiagnosticsProcessor();

  // Methods that correspond to the "Diagnosticsd" gRPC interface and allow
  // to perform actual gRPC requests as if the diagnostics_processor daemon
  // would do them:
  void GetProcData(const grpc_api::GetProcDataRequest& request,
                   GetProcDataCallback callback);
  void RunEcCommand(const grpc_api::RunEcCommandRequest& request,
                    RunEcCommandCallback callback);
  void GetEcProperty(const grpc_api::GetEcPropertyRequest& request,
                     GetEcPropertyCallback callback);

  // Setups callback for the next |HandleMessageFromUi| gRPC call.
  void set_handle_message_from_ui_callback(
      base::Closure handle_message_from_ui_callback);

  const base::Optional<std::string>&
  handle_message_from_ui_actual_json_message() const;

 private:
  using AsyncGrpcDiagnosticsProcessorServer =
      AsyncGrpcServer<grpc_api::DiagnosticsProcessor::AsyncService>;
  using AsyncGrpcDiagnosticsdClient = AsyncGrpcClient<grpc_api::Diagnosticsd>;

  // Receives gRPC request and saves json message from request in
  // |handle_message_from_ui_actual_json_message_|.
  // Calls the callback |handle_message_from_ui_callback_| after all.
  void HandleMessageFromUi(
      std::unique_ptr<grpc_api::HandleMessageFromUiRequest> request,
      const HandleMessageFromUiCallback& callback);

  AsyncGrpcDiagnosticsProcessorServer grpc_server_;
  AsyncGrpcDiagnosticsdClient diagnosticsd_grpc_client_;

  base::Optional<base::Closure> handle_message_from_ui_callback_;
  base::Optional<std::string> handle_message_from_ui_actual_json_message_;

  DISALLOW_COPY_AND_ASSIGN(FakeDiagnosticsProcessor);
};

}  // namespace diagnostics

#endif  // DIAGNOSTICS_DIAGNOSTICSD_FAKE_DIAGNOSTICS_PROCESSOR_H_
