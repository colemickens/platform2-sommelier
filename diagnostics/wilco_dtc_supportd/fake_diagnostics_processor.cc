// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "diagnostics/wilco_dtc_supportd/fake_diagnostics_processor.h"

#include <utility>

#include <base/run_loop.h>
#include <base/threading/thread_task_runner_handle.h>

#include "diagnostics/wilco_dtc_supportd/bind_utils.h"

namespace diagnostics {

FakeDiagnosticsProcessor::FakeDiagnosticsProcessor(
    const std::string& grpc_server_uri,
    const std::string& wilco_dtc_supportd_grpc_uri)
    : grpc_server_(base::ThreadTaskRunnerHandle::Get(), grpc_server_uri),
      diagnosticsd_grpc_client_(base::ThreadTaskRunnerHandle::Get(),
                                wilco_dtc_supportd_grpc_uri) {
  grpc_server_.RegisterHandler(
      &grpc_api::DiagnosticsProcessor::AsyncService::RequestHandleMessageFromUi,
      base::Bind(&FakeDiagnosticsProcessor::HandleMessageFromUi,
                 base::Unretained(this)));
  grpc_server_.RegisterHandler(
      &grpc_api::DiagnosticsProcessor::AsyncService::
          RequestHandleEcNotification,
      base::Bind(&FakeDiagnosticsProcessor::HandleEcNotification,
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

void FakeDiagnosticsProcessor::set_handle_message_from_ui_json_message_response(
    const std::string& json_message_response) {
  handle_message_from_ui_json_message_response_.emplace(json_message_response);
}

const base::Optional<std::string>&
FakeDiagnosticsProcessor::handle_message_from_ui_actual_json_message() const {
  return handle_message_from_ui_actual_json_message_;
}

void FakeDiagnosticsProcessor::HandleMessageFromUi(
    std::unique_ptr<grpc_api::HandleMessageFromUiRequest> request,
    const HandleMessageFromUiCallback& callback) {
  DCHECK(handle_message_from_ui_callback_);
  DCHECK(handle_message_from_ui_json_message_response_.has_value());

  handle_message_from_ui_actual_json_message_.emplace(request->json_message());

  auto response = std::make_unique<grpc_api::HandleMessageFromUiResponse>();
  response->set_response_json_message(
      handle_message_from_ui_json_message_response_.value());
  callback.Run(std::move(response));

  handle_message_from_ui_callback_->Run();
}

void FakeDiagnosticsProcessor::HandleEcNotification(
    std::unique_ptr<grpc_api::HandleEcNotificationRequest> request,
    const HandleEcNotificationCallback& callback) {
  DCHECK(handle_ec_event_request_callback_);

  auto response = std::make_unique<grpc_api::HandleEcNotificationResponse>();
  callback.Run(std::move(response));

  handle_ec_event_request_callback_->Run(request->type(), request->payload());
  handle_ec_event_request_callback_.reset();
}

}  // namespace diagnostics
