// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "diagnostics/wilco_dtc_supportd/fake_wilco_dtc.h"

#include <utility>

#include <base/run_loop.h>
#include <base/threading/thread_task_runner_handle.h>

#include "diagnostics/wilco_dtc_supportd/bind_utils.h"

namespace diagnostics {

FakeWilcoDtc::FakeWilcoDtc(const std::string& grpc_server_uri,
                           const std::string& wilco_dtc_supportd_grpc_uri)
    : grpc_server_(base::ThreadTaskRunnerHandle::Get(), grpc_server_uri),
      wilco_dtc_supportd_grp_client_(base::ThreadTaskRunnerHandle::Get(),
                                     wilco_dtc_supportd_grpc_uri) {
  grpc_server_.RegisterHandler(
      &grpc_api::WilcoDtc::AsyncService::RequestHandleMessageFromUi,
      base::Bind(&FakeWilcoDtc::HandleMessageFromUi, base::Unretained(this)));
  grpc_server_.RegisterHandler(
      &grpc_api::WilcoDtc::AsyncService::RequestHandleEcNotification,
      base::Bind(&FakeWilcoDtc::HandleEcNotification, base::Unretained(this)));
  grpc_server_.Start();
}

FakeWilcoDtc::~FakeWilcoDtc() {
  // Wait until both gRPC server and client get shut down.
  base::RunLoop run_loop;
  const base::Closure barrier_closure =
      BarrierClosure(2, run_loop.QuitClosure());
  grpc_server_.Shutdown(barrier_closure);
  wilco_dtc_supportd_grp_client_.Shutdown(barrier_closure);
  run_loop.Run();
}

void FakeWilcoDtc::GetProcData(const grpc_api::GetProcDataRequest& request,
                               GetProcDataCallback callback) {
  wilco_dtc_supportd_grp_client_.CallRpc(
      &grpc_api::WilcoDtcSupportd::Stub::AsyncGetProcData, request, callback);
}

void FakeWilcoDtc::GetEcTelemetry(
    const grpc_api::GetEcTelemetryRequest& request,
    GetEcTelemetryCallback callback) {
  wilco_dtc_supportd_grp_client_.CallRpc(
      &grpc_api::WilcoDtcSupportd::Stub::AsyncGetEcTelemetry, request,
      callback);
}

void FakeWilcoDtc::GetEcProperty(const grpc_api::GetEcPropertyRequest& request,
                                 GetEcPropertyCallback callback) {
  wilco_dtc_supportd_grp_client_.CallRpc(
      &grpc_api::WilcoDtcSupportd::Stub::AsyncGetEcProperty, request, callback);
}

void FakeWilcoDtc::PerformWebRequest(
    const grpc_api::PerformWebRequestParameter& parameter,
    const PerformWebRequestResponseCallback& callback) {
  wilco_dtc_supportd_grp_client_.CallRpc(
      &grpc_api::WilcoDtcSupportd::Stub::AsyncPerformWebRequest, parameter,
      callback);
}

void FakeWilcoDtc::set_handle_message_from_ui_callback(
    base::Closure handle_message_from_ui_callback) {
  handle_message_from_ui_callback_.emplace(
      std::move(handle_message_from_ui_callback));
}

void FakeWilcoDtc::set_handle_message_from_ui_json_message_response(
    const std::string& json_message_response) {
  handle_message_from_ui_json_message_response_.emplace(json_message_response);
}

const base::Optional<std::string>&
FakeWilcoDtc::handle_message_from_ui_actual_json_message() const {
  return handle_message_from_ui_actual_json_message_;
}

void FakeWilcoDtc::HandleMessageFromUi(
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

void FakeWilcoDtc::HandleEcNotification(
    std::unique_ptr<grpc_api::HandleEcNotificationRequest> request,
    const HandleEcNotificationCallback& callback) {
  DCHECK(handle_ec_event_request_callback_);

  auto response = std::make_unique<grpc_api::HandleEcNotificationResponse>();
  callback.Run(std::move(response));

  handle_ec_event_request_callback_->Run(request->type(), request->payload());
  handle_ec_event_request_callback_.reset();
}

}  // namespace diagnostics
