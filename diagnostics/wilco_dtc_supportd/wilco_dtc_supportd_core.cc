// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "diagnostics/wilco_dtc_supportd/wilco_dtc_supportd_core.h"

#include <algorithm>
#include <cstddef>
#include <utility>

#include <base/bind.h>
#include <base/files/file_util.h>
#include <base/logging.h>
#include <base/optional.h>
#include <base/threading/thread_task_runner_handle.h>
#include <dbus/diagnosticsd/dbus-constants.h>
#include <dbus/object_path.h>
#include <mojo/public/cpp/system/message_pipe.h>

#include "diagnostics/wilco_dtc_supportd/bind_utils.h"
#include "diagnostics/wilco_dtc_supportd/json_utils.h"

namespace diagnostics {

namespace {

using MojomDiagnosticsdWebRequestStatus =
    chromeos::diagnosticsd::mojom::DiagnosticsdWebRequestStatus;
using MojomDiagnosticsdWebRequestHttpMethod =
    chromeos::diagnosticsd::mojom::DiagnosticsdWebRequestHttpMethod;

// Converts HTTP method into an appropriate mojom one.
MojomDiagnosticsdWebRequestHttpMethod ConvertWebRequestHttpMethodToMojom(
    WilcoDtcSupportdCore::WebRequestHttpMethod http_method) {
  switch (http_method) {
    case WilcoDtcSupportdCore::WebRequestHttpMethod::kGet:
      return MojomDiagnosticsdWebRequestHttpMethod::kGet;
    case WilcoDtcSupportdCore::WebRequestHttpMethod::kHead:
      return MojomDiagnosticsdWebRequestHttpMethod::kHead;
    case WilcoDtcSupportdCore::WebRequestHttpMethod::kPost:
      return MojomDiagnosticsdWebRequestHttpMethod::kPost;
    case WilcoDtcSupportdCore::WebRequestHttpMethod::kPut:
      return MojomDiagnosticsdWebRequestHttpMethod::kPut;
  }
}

// Convert the result back from mojom status.
WilcoDtcSupportdCore::WebRequestStatus ConvertStatusFromMojom(
    MojomDiagnosticsdWebRequestStatus status) {
  switch (status) {
    case MojomDiagnosticsdWebRequestStatus::kOk:
      return WilcoDtcSupportdCore::WebRequestStatus::kOk;
    case MojomDiagnosticsdWebRequestStatus::kNetworkError:
      return WilcoDtcSupportdCore::WebRequestStatus::kNetworkError;
    case MojomDiagnosticsdWebRequestStatus::kHttpError:
      return WilcoDtcSupportdCore::WebRequestStatus::kHttpError;
  }
}

}  // namespace

WilcoDtcSupportdCore::WilcoDtcSupportdCore(
    const std::string& grpc_service_uri,
    const std::string& ui_message_receiver_wilco_dtc_grpc_uri,
    const std::vector<std::string>& wilco_dtc_grpc_uris,
    Delegate* delegate)
    : delegate_(delegate),
      grpc_service_uri_(grpc_service_uri),
      ui_message_receiver_wilco_dtc_grpc_uri_(
          ui_message_receiver_wilco_dtc_grpc_uri),
      wilco_dtc_grpc_uris_(wilco_dtc_grpc_uris),
      grpc_server_(base::ThreadTaskRunnerHandle::Get(), grpc_service_uri_) {
  DCHECK(delegate);
}

WilcoDtcSupportdCore::~WilcoDtcSupportdCore() = default;

bool WilcoDtcSupportdCore::Start() {
  // Associate RPCs of the to-be-exposed gRPC interface with methods of
  // |grpc_service_|.
  grpc_server_.RegisterHandler(
      &grpc_api::WilcoDtcSupportd::AsyncService::RequestSendMessageToUi,
      base::Bind(&WilcoDtcSupportdGrpcService::SendMessageToUi,
                 base::Unretained(&grpc_service_)));
  grpc_server_.RegisterHandler(
      &grpc_api::WilcoDtcSupportd::AsyncService::RequestGetProcData,
      base::Bind(&WilcoDtcSupportdGrpcService::GetProcData,
                 base::Unretained(&grpc_service_)));
  grpc_server_.RegisterHandler(
      &grpc_api::WilcoDtcSupportd::AsyncService::RequestGetSysfsData,
      base::Bind(&WilcoDtcSupportdGrpcService::GetSysfsData,
                 base::Unretained(&grpc_service_)));
  grpc_server_.RegisterHandler(
      &grpc_api::WilcoDtcSupportd::AsyncService::RequestGetEcTelemetry,
      base::Bind(&WilcoDtcSupportdGrpcService::GetEcTelemetry,
                 base::Unretained(&grpc_service_)));
  grpc_server_.RegisterHandler(
      &grpc_api::WilcoDtcSupportd::AsyncService::RequestGetEcProperty,
      base::Bind(&WilcoDtcSupportdGrpcService::GetEcProperty,
                 base::Unretained(&grpc_service_)));
  grpc_server_.RegisterHandler(
      &grpc_api::WilcoDtcSupportd::AsyncService::RequestPerformWebRequest,
      base::Bind(&WilcoDtcSupportdGrpcService::PerformWebRequest,
                 base::Unretained(&grpc_service_)));
  grpc_server_.RegisterHandler(
      &grpc_api::WilcoDtcSupportd::AsyncService::RequestGetAvailableRoutines,
      base::Bind(&WilcoDtcSupportdGrpcService::GetAvailableRoutines,
                 base::Unretained(&grpc_service_)));
  grpc_server_.RegisterHandler(
      &grpc_api::WilcoDtcSupportd::AsyncService::RequestRunRoutine,
      base::Bind(&WilcoDtcSupportdGrpcService::RunRoutine,
                 base::Unretained(&grpc_service_)));
  grpc_server_.RegisterHandler(
      &grpc_api::WilcoDtcSupportd::AsyncService::RequestGetRoutineUpdate,
      base::Bind(&WilcoDtcSupportdGrpcService::GetRoutineUpdate,
                 base::Unretained(&grpc_service_)));
  grpc_server_.RegisterHandler(
      &grpc_api::WilcoDtcSupportd::AsyncService::RequestGetOsVersion,
      base::Bind(&WilcoDtcSupportdGrpcService::GetOsVersion,
                 base::Unretained(&grpc_service_)));
  grpc_server_.RegisterHandler(
      &grpc_api::WilcoDtcSupportd::AsyncService::RequestGetConfigurationData,
      base::Bind(&WilcoDtcSupportdGrpcService::GetConfigurationData,
                 base::Unretained(&grpc_service_)));

  // Start the gRPC server that listens for incoming gRPC requests.
  VLOG(1) << "Starting gRPC server";
  if (!grpc_server_.Start()) {
    LOG(ERROR) << "Failed to start the gRPC server listening on "
               << grpc_service_uri_;
    return false;
  }
  VLOG(0) << "Successfully started gRPC server listening on "
          << grpc_service_uri_;

  // Start the gRPC clients that talk to the wilco_dtc daemon.
  for (const auto& uri : wilco_dtc_grpc_uris_) {
    wilco_dtc_grpc_clients_.push_back(
        std::make_unique<AsyncGrpcClient<grpc_api::WilcoDtc>>(
            base::ThreadTaskRunnerHandle::Get(), uri));
    VLOG(0) << "Created gRPC wilco_dtc client on " << uri;
  }

  // Start the gRPC client that is allowed to receive UI messages as a normal
  // gRPC client that talks to the wilco_dtc daemon.
  wilco_dtc_grpc_clients_.push_back(
      std::make_unique<AsyncGrpcClient<grpc_api::WilcoDtc>>(
          base::ThreadTaskRunnerHandle::Get(),
          ui_message_receiver_wilco_dtc_grpc_uri_));
  VLOG(0) << "Created gRPC wilco_dtc client on "
          << ui_message_receiver_wilco_dtc_grpc_uri_;
  ui_message_receiver_wilco_dtc_grpc_client_ =
      wilco_dtc_grpc_clients_.back().get();

  // Start EC event service.
  if (!ec_event_service_.Start()) {
    LOG(WARNING)
        << "Failed to start EC event service. EC events will be ignored.";
  }

  return true;
}

void WilcoDtcSupportdCore::ShutDown(const base::Closure& on_shutdown) {
  VLOG(1) << "Tearing down gRPC server, gRPC wilco_dtc clients and "
             "EC event service";
  const base::Closure barrier_closure =
      BarrierClosure(wilco_dtc_grpc_clients_.size() + 2, on_shutdown);
  ec_event_service_.Shutdown(barrier_closure);
  grpc_server_.Shutdown(barrier_closure);
  for (const auto& client : wilco_dtc_grpc_clients_) {
    client->Shutdown(barrier_closure);
  }
  ui_message_receiver_wilco_dtc_grpc_client_ = nullptr;
}

void WilcoDtcSupportdCore::RegisterDBusObjectsAsync(
    const scoped_refptr<dbus::Bus>& bus,
    brillo::dbus_utils::AsyncEventSequencer* sequencer) {
  DCHECK(bus);
  DCHECK(!dbus_object_);
  dbus_object_ = std::make_unique<brillo::dbus_utils::DBusObject>(
      nullptr /* object_manager */, bus,
      dbus::ObjectPath(kDiagnosticsdServicePath));
  brillo::dbus_utils::DBusInterface* dbus_interface =
      dbus_object_->AddOrGetInterface(kDiagnosticsdServiceInterface);
  DCHECK(dbus_interface);
  dbus_interface->AddSimpleMethodHandlerWithError(
      kDiagnosticsdBootstrapMojoConnectionMethod,
      base::Unretained(&dbus_service_),
      &WilcoDtcSupportdDBusService::BootstrapMojoConnection);
  dbus_object_->RegisterAsync(sequencer->GetHandler(
      "Failed to register D-Bus object" /* descriptive_message */,
      true /* failure_is_fatal */));
}

bool WilcoDtcSupportdCore::StartMojoServiceFactory(base::ScopedFD mojo_pipe_fd,
                                                   std::string* error_message) {
  DCHECK(mojo_pipe_fd.is_valid());

  if (mojo_service_bind_attempted_) {
    // This should not normally be triggered, since the other endpoint - the
    // browser process - should bootstrap the Mojo connection only once, and
    // when that process is killed the Mojo shutdown notification should have
    // been received earlier. But handle this case to be on the safe side. After
    // our restart the browser process is expected to invoke the bootstrapping
    // again.
    *error_message = "Mojo connection was already bootstrapped";
    ShutDownDueToMojoError(
        "Repeated Mojo bootstrap request received" /* debug_reason */);
    return false;
  }

  if (!base::SetCloseOnExec(mojo_pipe_fd.get())) {
    PLOG(ERROR) << "Failed to set FD_CLOEXEC on Mojo file descriptor";
    *error_message = "Failed to set FD_CLOEXEC";
    return false;
  }

  mojo_service_bind_attempted_ = true;
  mojo_service_factory_binding_ = delegate_->BindDiagnosticsdMojoServiceFactory(
      this /* mojo_service_factory */, std::move(mojo_pipe_fd));
  if (!mojo_service_factory_binding_) {
    *error_message = "Failed to bootstrap Mojo";
    ShutDownDueToMojoError("Mojo bootstrap failed" /* debug_reason */);
    return false;
  }
  mojo_service_factory_binding_->set_connection_error_handler(base::Bind(
      &WilcoDtcSupportdCore::ShutDownDueToMojoError, base::Unretained(this),
      "Mojo connection error" /* debug_reason */));
  LOG(INFO) << "Successfully bootstrapped Mojo connection";
  return true;
}

void WilcoDtcSupportdCore::GetService(MojomDiagnosticsdServiceRequest service,
                                      MojomDiagnosticsdClientPtr client,
                                      const GetServiceCallback& callback) {
  // Mojo guarantees that these parameters are nun-null (see
  // VALIDATION_ERROR_UNEXPECTED_INVALID_HANDLE).
  DCHECK(service.is_pending());
  DCHECK(client);

  if (mojo_service_) {
    LOG(WARNING) << "GetService Mojo method called multiple times";
    // We should not normally be called more than once, so don't bother with
    // trying to reuse objects from the previous call. However, make sure we
    // don't have duplicate instances of the service at any moment of time.
    mojo_service_.reset();
  }

  // Create an instance of WilcoDtcSupportdMojoService that will handle incoming
  // Mojo calls. Pass |service| to it to fulfill the remote endpoint's request,
  // allowing it to call into |mojo_service_|. Pass also |client| to allow
  // |mojo_service_| to do calls in the opposite direction.
  mojo_service_ = std::make_unique<WilcoDtcSupportdMojoService>(
      this /* delegate */, std::move(service), std::move(client));

  callback.Run();
}

void WilcoDtcSupportdCore::ShutDownDueToMojoError(
    const std::string& debug_reason) {
  // Our daemon has to be restarted to be prepared for future Mojo connection
  // bootstraps. We can't do this without a restart since Mojo EDK gives no
  // guarantee to support repeated bootstraps. Therefore tear down and exit from
  // our process and let upstart to restart us again.
  LOG(INFO) << "Shutting down due to: " << debug_reason;
  mojo_service_.reset();
  mojo_service_factory_binding_.reset();
  delegate_->BeginDaemonShutdown();
}

void WilcoDtcSupportdCore::PerformWebRequestToBrowser(
    WebRequestHttpMethod http_method,
    const std::string& url,
    const std::vector<std::string>& headers,
    const std::string& request_body,
    const PerformWebRequestToBrowserCallback& callback) {
  VLOG(1) << "WilcoDtcSupportdCore::PerformWebRequestToBrowser";

  if (!mojo_service_) {
    LOG(WARNING) << "PerformWebRequestToBrowser happens before Mojo connection "
                 << "is established.";
    callback.Run(WebRequestStatus::kInternalError, 0 /* http_status */,
                 "" /* response_body */);
    return;
  }

  mojo_service_->PerformWebRequest(
      ConvertWebRequestHttpMethodToMojom(http_method), url, headers,
      request_body,
      base::Bind(
          [](const PerformWebRequestToBrowserCallback& callback,
             MojomDiagnosticsdWebRequestStatus status, int http_status,
             base::StringPiece response_body) {
            callback.Run(ConvertStatusFromMojom(status), http_status,
                         response_body);
          },
          callback));
}

void WilcoDtcSupportdCore::SendGrpcEcEventToWilcoDtc(
    const WilcoDtcSupportdEcEventService::EcEvent& ec_event) {
  VLOG(1) << "WilcoDtcSupportdCore::SendGrpcEcEventToWilcoDtc";

  grpc_api::HandleEcNotificationRequest request;
  request.set_type(ec_event.type);

  size_t data_size =
      std::min(ec_event.size * sizeof(ec_event.data[0]), sizeof(ec_event.data));

  request.set_payload(ec_event.data, data_size);

  for (auto& client : wilco_dtc_grpc_clients_) {
    client->CallRpc(
        &grpc_api::WilcoDtc::Stub::AsyncHandleEcNotification, request,
        base::Bind([](std::unique_ptr<grpc_api::HandleEcNotificationResponse>
                          response) {
          if (!response) {
            LOG(ERROR)
                << "Failed to call HandleEcNotificationRequest gRPC method on "
                   "wilco_dtc: response message is nullptr";
            return;
          }
          VLOG(1) << "gRPC method HandleEcNotificationRequest was successfully"
                     "called on wilco_dtc";
        }));
  }
}

void WilcoDtcSupportdCore::GetAvailableRoutinesToService(
    const GetAvailableRoutinesToServiceCallback& callback) {
  routine_service_.GetAvailableRoutines(callback);
}

void WilcoDtcSupportdCore::RunRoutineToService(
    const grpc_api::RunRoutineRequest& request,
    const RunRoutineToServiceCallback& callback) {
  routine_service_.RunRoutine(request, callback);
}

void WilcoDtcSupportdCore::GetRoutineUpdateRequestToService(
    int uuid,
    grpc_api::GetRoutineUpdateRequest::Command command,
    bool include_output,
    const GetRoutineUpdateRequestToServiceCallback& callback) {
  routine_service_.GetRoutineUpdate(uuid, command, include_output, callback);
}

void WilcoDtcSupportdCore::GetConfigurationDataFromBrowser(
    const GetConfigurationDataFromBrowserCallback& callback) {
  VLOG(1) << "WilcoDtcSupportdCore::GetConfigurationDataFromBrowser";

  if (!mojo_service_) {
    LOG(WARNING) << "GetConfigurationDataFromBrowser happens before Mojo "
                 << "connection is established.";
    callback.Run("" /* json_configuration_data */);
    return;
  }

  mojo_service_->GetConfigurationData(callback);
}

void WilcoDtcSupportdCore::SendGrpcUiMessageToWilcoDtc(
    base::StringPiece json_message,
    const SendGrpcUiMessageToWilcoDtcCallback& callback) {
  VLOG(1) << "WilcoDtcSupportdCore::SendGrpcMessageToWilcoDtc";

  if (!ui_message_receiver_wilco_dtc_grpc_client_) {
    VLOG(1) << "The UI message is discarded since the recipient has been shut "
            << "down.";
    callback.Run(std::string() /* response_json_message */);
    return;
  }

  grpc_api::HandleMessageFromUiRequest request;
  request.set_json_message(json_message.data() ? json_message.data() : "",
                           json_message.length());

  ui_message_receiver_wilco_dtc_grpc_client_->CallRpc(
      &grpc_api::WilcoDtc::Stub::AsyncHandleMessageFromUi, request,
      base::Bind(
          [](const SendGrpcUiMessageToWilcoDtcCallback& callback,
             std::unique_ptr<grpc_api::HandleMessageFromUiResponse> response) {
            if (!response) {
              LOG(ERROR)
                  << "Failed to call HandleMessageFromUiRequest gRPC method on "
                     "wilco_dtc: response message is nullptr";
              callback.Run(std::string() /* response_json_message */);
              return;
            }

            VLOG(1) << "gRPC method HandleMessageFromUiRequest was "
                       "successfully called on wilco_dtc";

            std::string json_error_message;
            if (!IsJsonValid(
                    base::StringPiece(response->response_json_message()),
                    &json_error_message)) {
              LOG(ERROR) << "Invalid JSON error: " << json_error_message;
              callback.Run(std::string() /* response_json_message */);
              return;
            }

            callback.Run(response->response_json_message());
          },
          callback));
}

}  // namespace diagnostics
