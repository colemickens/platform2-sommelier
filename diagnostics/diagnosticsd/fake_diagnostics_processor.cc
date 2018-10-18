// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "diagnostics/diagnosticsd/fake_diagnostics_processor.h"

#include <utility>

#include <base/threading/thread_task_runner_handle.h>

namespace diagnostics {

FakeDiagnosticsProcessor::FakeDiagnosticsProcessor(
    const std::string& grpc_server_uri)
    : grpc_server_(base::ThreadTaskRunnerHandle::Get(), grpc_server_uri) {
  grpc_server_.RegisterHandler(
      &grpc_api::DiagnosticsProcessor::AsyncService::RequestHandleMessageFromUi,
      base::Bind(&FakeDiagnosticsProcessor::HandleMessageFromUi,
                 base::Unretained(this)));
}

FakeDiagnosticsProcessor::~FakeDiagnosticsProcessor() = default;

void FakeDiagnosticsProcessor::Start() {
  grpc_server_.Start();
}

void FakeDiagnosticsProcessor::Shutdown(base::Closure on_shutdown_callback) {
  grpc_server_.Shutdown(on_shutdown_callback);
}

void FakeDiagnosticsProcessor::HandleMessageFromUi(
    std::unique_ptr<grpc_api::HandleMessageFromUiRequest> request,
    const HandleMessageFromUiCallback& callback) {
  handle_message_from_ui_actual_json_message_.emplace(request->json_message());
  callback.Run(std::make_unique<grpc_api::EmptyMessage>());
  handle_message_from_ui_callback_.value().Run();
}

void FakeDiagnosticsProcessor::set_handle_message_from_ui_callback(
    base::Closure handle_message_from_ui_callback) {
  handle_message_from_ui_callback_.emplace(
      std::move(handle_message_from_ui_callback));
}

const base::Optional<std::string>&
FakeDiagnosticsProcessor::handle_message_from_ui_actual_json_message() const {
  return handle_message_from_ui_actual_json_message_;
}

}  // namespace diagnostics
