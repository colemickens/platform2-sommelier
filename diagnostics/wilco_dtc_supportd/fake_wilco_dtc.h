// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DIAGNOSTICS_WILCO_DTC_SUPPORTD_FAKE_WILCO_DTC_H_
#define DIAGNOSTICS_WILCO_DTC_SUPPORTD_FAKE_WILCO_DTC_H_

#include <memory>
#include <string>

#include <base/callback.h>
#include <base/macros.h>
#include <base/optional.h>

#include "diagnostics/grpc_async_adapter/async_grpc_client.h"
#include "diagnostics/grpc_async_adapter/async_grpc_server.h"

#include "wilco_dtc.grpc.pb.h"           // NOLINT(build/include)
#include "wilco_dtc_supportd.grpc.pb.h"  // NOLINT(build/include)

namespace diagnostics {

// Helper class that allows to test gRPC communication between wilco_dtc and
// support daemon.
//
// This class runs a "WilcoDtc" gRPC server on the given |grpc_server_uri| URI,
// and a gRPC client to the "WilcoDtcSupportd" gRPC service on the
// |wilco_dtc_supportd_grpc_uri| gRPC URI.
class FakeWilcoDtc final {
 public:
  using GetProcDataCallback =
      base::Callback<void(std::unique_ptr<grpc_api::GetProcDataResponse>)>;
  using GetEcTelemetryCallback =
      base::Callback<void(std::unique_ptr<grpc_api::GetEcTelemetryResponse>)>;
  using GetEcPropertyCallback =
      base::Callback<void(std::unique_ptr<grpc_api::GetEcPropertyResponse>)>;
  using HandleMessageFromUiCallback = base::Callback<void(
      std::unique_ptr<grpc_api::HandleMessageFromUiResponse>)>;
  using HandleEcNotificationCallback = base::Callback<void(
      std::unique_ptr<grpc_api::HandleEcNotificationResponse>)>;
  using PerformWebRequestResponseCallback = base::Callback<void(
      std::unique_ptr<grpc_api::PerformWebRequestResponse>)>;

  using HandleEcNotificationRequestCallback =
      base::RepeatingCallback<void(int32_t, const std::string&)>;

  FakeWilcoDtc(const std::string& grpc_server_uri,
               const std::string& wilco_dtc_supportd_grpc_uri);
  ~FakeWilcoDtc();

  // Methods that correspond to the "WilcoDtcSupportd" gRPC interface and allow
  // to perform actual gRPC requests as if the wilco_dtc daemon would do them:
  void GetProcData(const grpc_api::GetProcDataRequest& request,
                   GetProcDataCallback callback);
  void GetEcTelemetry(const grpc_api::GetEcTelemetryRequest& request,
                      GetEcTelemetryCallback callback);
  void GetEcProperty(const grpc_api::GetEcPropertyRequest& request,
                     GetEcPropertyCallback callback);
  void PerformWebRequest(const grpc_api::PerformWebRequestParameter& parameter,
                         const PerformWebRequestResponseCallback& callback);

  // Setups callback for the next |HandleMessageFromUi| gRPC call.
  void set_handle_message_from_ui_callback(
      base::Closure handle_message_from_ui_callback);

  // Setups json message response for the next |HandleMessageFromUi| gRPC call.
  void set_handle_message_from_ui_json_message_response(
      const std::string& json_message_response);

  // Setups callback for the next |HandleEcNotification| gRPC call.
  // |handle_ec_event_request_callback_| will be called only once.
  void set_handle_ec_event_request_callback(
      HandleEcNotificationRequestCallback handle_ec_event_request_callback) {
    handle_ec_event_request_callback_ = handle_ec_event_request_callback;
  }

  const base::Optional<std::string>&
  handle_message_from_ui_actual_json_message() const;

 private:
  using AsyncGrpcWilcoDtcServer =
      AsyncGrpcServer<grpc_api::WilcoDtc::AsyncService>;
  using AsyncGrpcWilcoDtcSupportdClient =
      AsyncGrpcClient<grpc_api::WilcoDtcSupportd>;

  // Receives gRPC request and saves json message from request in
  // |handle_message_from_ui_actual_json_message_|.
  // Calls the callback |handle_message_from_ui_callback_| after all.
  void HandleMessageFromUi(
      std::unique_ptr<grpc_api::HandleMessageFromUiRequest> request,
      const HandleMessageFromUiCallback& callback);

  // Receives gRPC request and invokes the given |callback| with gRPC response.
  // Calls the callback |handle_ec_event_request_callback_| after all with the
  // request type and payload.
  void HandleEcNotification(
      std::unique_ptr<grpc_api::HandleEcNotificationRequest> request,
      const HandleEcNotificationCallback& callback);

  AsyncGrpcWilcoDtcServer grpc_server_;
  AsyncGrpcWilcoDtcSupportdClient wilco_dtc_supportd_grp_client_;

  base::Optional<base::Closure> handle_message_from_ui_callback_;
  base::Optional<std::string> handle_message_from_ui_actual_json_message_;
  base::Optional<std::string> handle_message_from_ui_json_message_response_;

  base::Optional<HandleEcNotificationRequestCallback>
      handle_ec_event_request_callback_;

  DISALLOW_COPY_AND_ASSIGN(FakeWilcoDtc);
};

}  // namespace diagnostics

#endif  // DIAGNOSTICS_WILCO_DTC_SUPPORTD_FAKE_WILCO_DTC_H_
