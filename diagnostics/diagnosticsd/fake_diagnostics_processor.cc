// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "diagnostics/diagnosticsd/fake_diagnostics_processor.h"

#include <utility>

#include <base/run_loop.h>
#include <base/threading/thread_task_runner_handle.h>

#include "diagnostics/diagnosticsd/bind_utils.h"

namespace diagnostics {

FakeDiagnosticsProcessor::FakeDiagnosticsProcessor(
    const std::string& grpc_server_uri,
    const std::string& diagnosticsd_grpc_uri)
    : grpc_server_(base::ThreadTaskRunnerHandle::Get(), grpc_server_uri),
      diagnosticsd_grpc_client_(base::ThreadTaskRunnerHandle::Get(),
                                diagnosticsd_grpc_uri) {
  grpc_server_.RegisterHandler(
      &grpc_api::DiagnosticsProcessor::AsyncService::RequestHandleMessageFromUi,
      base::Bind(&FakeDiagnosticsProcessor::HandleMessageFromUi,
                 base::Unretained(this)));
  grpc_server_.Start();
}

FakeDiagnosticsProcessor::~FakeDiagnosticsProcessor() {
  // Wait until both gRPC server and client get shut down.
  base::RunLoop run_loop;
  const base::Closure barrier_closure =
      BarrierClosure(2, run_loop.QuitClosure());
  grpc_server_.Shutdown(barrier_closure);
  diagnosticsd_grpc_client_.Shutdown(barrier_closure);
  run_loop.Run();
}

void FakeDiagnosticsProcessor::GetProcData(
    const grpc_api::GetProcDataRequest& request, GetProcDataCallback callback) {
  diagnosticsd_grpc_client_.CallRpc(
      &grpc_api::Diagnosticsd::Stub::AsyncGetProcData, request, callback);
}

void FakeDiagnosticsProcessor::RunEcCommand(
    const grpc_api::RunEcCommandRequest& request,
    RunEcCommandCallback callback) {
  diagnosticsd_grpc_client_.CallRpc(
      &grpc_api::Diagnosticsd::Stub::AsyncRunEcCommand, request, callback);
}

void FakeDiagnosticsProcessor::GetEcProperty(
    const grpc_api::GetEcPropertyRequest& request,
    GetEcPropertyCallback callback) {
  diagnosticsd_grpc_client_.CallRpc(
      &grpc_api::Diagnosticsd::Stub::AsyncGetEcProperty, request, callback);
}

void FakeDiagnosticsProcessor::PerformWebRequest(
    const grpc_api::PerformWebRequestParameter& parameter,
    const PerformWebRequestResponseCallback& callback) {
  diagnosticsd_grpc_client_.CallRpc(
      &grpc_api::Diagnosticsd::Stub::AsyncPerformWebRequest, parameter,
      callback);
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

void FakeDiagnosticsProcessor::HandleMessageFromUi(
    std::unique_ptr<grpc_api::HandleMessageFromUiRequest> request,
    const HandleMessageFromUiCallback& callback) {
  DCHECK(handle_message_from_ui_callback_);
  handle_message_from_ui_actual_json_message_.emplace(request->json_message());
  callback.Run(std::make_unique<grpc_api::HandleMessageFromUiResponse>());
  handle_message_from_ui_callback_->Run();
}

}  // namespace diagnostics
