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

#include "diagnostics/grpc_async_adapter/async_grpc_server.h"

#include "common.pb.h"                      // NOLINT(build/include)
#include "diagnostics_processor.grpc.pb.h"  // NOLINT(build/include)

namespace diagnostics {

// Helper class that allows to test gRPC communication between
// diagnostics daemon and diagnostics processor.
class FakeDiagnosticsProcessor final {
 public:
  using HandleMessageFromUiCallback =
      base::Callback<void(std::unique_ptr<grpc_api::EmptyMessage>)>;

  explicit FakeDiagnosticsProcessor(const std::string& grpc_server_uri);
  ~FakeDiagnosticsProcessor();

  void Start();
  void Shutdown(base::Closure on_shutdown_callback);

  // Setups callback for the next |HandleMessageFromUi| gRPC call.
  void set_handle_message_from_ui_callback(
      base::Closure handle_message_from_ui_callback);

  const base::Optional<std::string>&
  handle_message_from_ui_actual_json_message() const;

 private:
  using AsyncGrpcDiagnosticsProcessorServer =
      AsyncGrpcServer<grpc_api::DiagnosticsProcessor::AsyncService>;

  // Receives gRPC request and saves json message from request in
  // |handle_message_from_ui_actual_json_message_|.
  // Calls the callback |handle_message_from_ui_callback_| after all.
  void HandleMessageFromUi(
      std::unique_ptr<grpc_api::HandleMessageFromUiRequest> request,
      const HandleMessageFromUiCallback& callback);

  AsyncGrpcDiagnosticsProcessorServer grpc_server_;

  base::Optional<base::Closure> handle_message_from_ui_callback_;
  base::Optional<std::string> handle_message_from_ui_actual_json_message_;

  DISALLOW_COPY_AND_ASSIGN(FakeDiagnosticsProcessor);
};

}  // namespace diagnostics

#endif  // DIAGNOSTICS_DIAGNOSTICSD_FAKE_DIAGNOSTICS_PROCESSOR_H_
