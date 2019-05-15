// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DIAGNOSTICS_WILCO_DTC_SUPPORTD_WILCO_DTC_SUPPORTD_CORE_H_
#define DIAGNOSTICS_WILCO_DTC_SUPPORTD_WILCO_DTC_SUPPORTD_CORE_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <base/callback.h>
#include <base/files/scoped_file.h>
#include <base/macros.h>
#include <base/memory/ref_counted.h>
#include <base/strings/string_piece.h>
#include <brillo/dbus/async_event_sequencer.h>
#include <brillo/dbus/dbus_object.h>
#include <dbus/bus.h>
#include <mojo/public/cpp/bindings/binding.h>

#include "diagnostics/grpc_async_adapter/async_grpc_client.h"
#include "diagnostics/grpc_async_adapter/async_grpc_server.h"
#include "diagnostics/wilco_dtc_supportd/wilco_dtc_supportd_dbus_service.h"
#include "diagnostics/wilco_dtc_supportd/wilco_dtc_supportd_ec_event_service.h"
#include "diagnostics/wilco_dtc_supportd/wilco_dtc_supportd_grpc_service.h"
#include "diagnostics/wilco_dtc_supportd/wilco_dtc_supportd_mojo_service.h"
#include "diagnostics/wilco_dtc_supportd/wilco_dtc_supportd_routine_service.h"

#include "mojo/wilco_dtc_supportd.mojom.h"
#include "wilco_dtc.grpc.pb.h"           // NOLINT(build/include)
#include "wilco_dtc_supportd.grpc.pb.h"  // NOLINT(build/include)

namespace diagnostics {

// Integrates together all pieces which implement separate IPC services exposed
// by the wilco_dtc_supportd daemon and IPC clients.
class WilcoDtcSupportdCore final
    : public WilcoDtcSupportdDBusService::Delegate,
      public WilcoDtcSupportdEcEventService::Delegate,
      public WilcoDtcSupportdGrpcService::Delegate,
      public WilcoDtcSupportdMojoService::Delegate,
      public chromeos::wilco_dtc_supportd::mojom::
          WilcoDtcSupportdServiceFactory {
 public:
  class Delegate {
   public:
    using MojomWilcoDtcSupportdServiceFactory =
        chromeos::wilco_dtc_supportd::mojom::WilcoDtcSupportdServiceFactory;

    virtual ~Delegate() = default;

    // Binds the given |mojo_service_factory| to the Mojo message pipe that
    // works via the given |mojo_pipe_fd|. On success, returns the created Mojo
    // binding, otherwise returns nullptr.
    //
    // In production this method must be called no more than once during the
    // lifetime of the daemon, since Mojo EDK gives no guarantee to support
    // repeated initialization with different parent handles.
    virtual std::unique_ptr<mojo::Binding<MojomWilcoDtcSupportdServiceFactory>>
    BindWilcoDtcSupportdMojoServiceFactory(
        MojomWilcoDtcSupportdServiceFactory* mojo_service_factory,
        base::ScopedFD mojo_pipe_fd) = 0;

    // Begins the graceful shutdown of the wilco_dtc_supportd daemon.
    virtual void BeginDaemonShutdown() = 0;
  };

  // |grpc_service_uris| are the URIs on which the gRPC interface exposed by the
  // wilco_dtc_supportd daemon will be listening.
  // |ui_message_receiver_wilco_dtc_grpc_uri| is the URI which is
  // used for making requests to the gRPC interface exposed by the
  // wilco_dtc daemon which is explicitly eligible to receive
  // messages from UI extension (hosted by browser), no other gRPC client
  // recieves messages from UI extension.
  // |wilco_dtc_grpc_uris| is the list of URI's which are used for
  // making requests to the gRPC interface exposed by the wilco_dtc
  // daemons. Should not contain the URI equal to
  // |ui_message_receiver_wilco_dtc_grpc_uri|.
  WilcoDtcSupportdCore(
      const std::vector<std::string>& grpc_service_uris,
      const std::string& ui_message_receiver_wilco_dtc_grpc_uri,
      const std::vector<std::string>& wilco_dtc_grpc_uris,
      Delegate* delegate);
  ~WilcoDtcSupportdCore() override;

  // Overrides the file system root directory for file operations in tests.
  void set_root_dir_for_testing(const base::FilePath& root_dir) {
    ec_event_service_.set_root_dir_for_testing(root_dir);
    grpc_service_.set_root_dir_for_testing(root_dir);
  }

  // Overrides EC event fd events for |poll()| function in |ec_event_service_|
  // service in tests.
  void set_ec_event_service_fd_events_for_testing(int16_t events) {
    ec_event_service_.set_event_fd_events_for_testing(events);
  }

  // Starts gRPC servers, gRPC clients and EC event service.
  bool Start();

  // Performs asynchronous shutdown and cleanup of gRPC servers, gRPC clients
  // and EC event service. Destroys |dbus_object_| object.
  // This must be used before deleting this instance in case Start() was
  // called and returned success - in that case, the instance must be
  // destroyed only after |on_shutdown| has been called.
  void ShutDown(const base::Closure& on_shutdown);

  // Register the D-Bus object that the wilco_dtc_supportd daemon exposes and
  // tie methods exposed by this object with the actual implementation.
  void RegisterDBusObjectsAsync(
      const scoped_refptr<dbus::Bus>& bus,
      brillo::dbus_utils::AsyncEventSequencer* sequencer);

 private:
  using MojomWilcoDtcSupportdClientPtr =
      chromeos::wilco_dtc_supportd::mojom::WilcoDtcSupportdClientPtr;
  using MojomWilcoDtcSupportdServiceRequest =
      chromeos::wilco_dtc_supportd::mojom::WilcoDtcSupportdServiceRequest;

  // WilcoDtcSupportdDBusService::Delegate overrides:
  bool StartMojoServiceFactory(base::ScopedFD mojo_pipe_fd,
                               std::string* error_message) override;

  // Shuts down the self instance after a Mojo fatal error happens.
  void ShutDownDueToMojoError(const std::string& debug_reason);

  // WilcoDtcSupportdEcEventService::Delegate overrides:
  void SendGrpcEcEventToWilcoDtc(
      const WilcoDtcSupportdEcEventService::EcEvent& ec_event) override;

  // WilcoDtcSupportdGrpcService::Delegate overrides:
  void PerformWebRequestToBrowser(
      WebRequestHttpMethod http_method,
      const std::string& url,
      const std::vector<std::string>& headers,
      const std::string& request_body,
      const PerformWebRequestToBrowserCallback& callback) override;
  void GetAvailableRoutinesToService(
      const GetAvailableRoutinesToServiceCallback& callback) override;
  void RunRoutineToService(
      const grpc_api::RunRoutineRequest& request,
      const RunRoutineToServiceCallback& callback) override;
  void GetRoutineUpdateRequestToService(
      int uuid,
      grpc_api::GetRoutineUpdateRequest::Command command,
      bool include_output,
      const GetRoutineUpdateRequestToServiceCallback& callback) override;
  void GetConfigurationDataFromBrowser(
      const GetConfigurationDataFromBrowserCallback& callback) override;

  // WilcoDtcSupportdMojoService::Delegate overrides:
  void SendGrpcUiMessageToWilcoDtc(
      base::StringPiece json_message,
      const SendGrpcUiMessageToWilcoDtcCallback& callback) override;
  void NotifyConfigurationDataChangedToWilcoDtc() override;

  // chromeos::wilco_dtc_supportd::mojom::WilcoDtcSupportdServiceFactory
  // overrides:
  void GetService(MojomWilcoDtcSupportdServiceRequest service,
                  MojomWilcoDtcSupportdClientPtr client,
                  const GetServiceCallback& callback) override;

  // Unowned. The delegate should outlive this instance.
  Delegate* const delegate_;

  // gRPC-related members:

  // gRPC URIs on which the |grpc_server_| is listening for incoming requests.
  const std::vector<std::string> grpc_service_uris_;
  // gRPC URI which is used by
  // |ui_message_receiver_wilco_dtc_grpc_client_| for sending UI
  // messages and EC notifications over the gRPC interface.
  const std::string ui_message_receiver_wilco_dtc_grpc_uri_;
  // gRPC URIs which are used by |wilco_dtc_grpc_clients_| for
  // accessing the gRPC interface exposed by the wilco_dtc daemons.
  const std::vector<std::string> wilco_dtc_grpc_uris_;
  // Implementation of the gRPC interface exposed by the wilco_dtc_supportd
  // daemon.
  WilcoDtcSupportdGrpcService grpc_service_{this /* delegate */};
  // Connects |grpc_service_| with the gRPC server that listens for incoming
  // requests.
  AsyncGrpcServer<grpc_api::WilcoDtcSupportd::AsyncService> grpc_server_;
  // Allows to make outgoing requests to the gRPC interfaces exposed by the
  // wilco_dtc daemons.
  std::vector<std::unique_ptr<AsyncGrpcClient<grpc_api::WilcoDtc>>>
      wilco_dtc_grpc_clients_;
  // The pre-defined gRPC client that is allowed to respond to UI messages.
  // Owned by |wilco_dtc_grpc_clients_|.
  AsyncGrpcClient<grpc_api::WilcoDtc>*
      ui_message_receiver_wilco_dtc_grpc_client_;

  // D-Bus-related members:

  // Implementation of the D-Bus interface exposed by the wilco_dtc_supportd
  // daemon.
  WilcoDtcSupportdDBusService dbus_service_{this /* delegate */};
  // Connects |dbus_service_| with the methods of the D-Bus object exposed by
  // the wilco_dtc_supportd daemon.
  std::unique_ptr<brillo::dbus_utils::DBusObject> dbus_object_;

  // Mojo-related members:

  // Binding that connects this instance (which is an implementation of
  // chromeos::wilco_dtc_supportd::mojom::WilcoDtcSupportdServiceFactory) with
  // the message pipe set up on top of the received file descriptor.
  //
  // Gets created after the BootstrapMojoConnection D-Bus method is called.
  std::unique_ptr<mojo::Binding<WilcoDtcSupportdServiceFactory>>
      mojo_service_factory_binding_;
  // Implementation of the Mojo interface exposed by the wilco_dtc_supportd
  // daemon and a proxy that allows sending outgoing Mojo requests.
  //
  // Gets created after the GetService() Mojo method is called.
  std::unique_ptr<WilcoDtcSupportdMojoService> mojo_service_;
  // Whether binding of the Mojo service was attempted.
  //
  // This flag is needed for detecting repeated Mojo bootstrapping attempts
  // (alternative ways, like checking |mojo_service_factory_binding_|, are
  // unreliable during shutdown).
  bool mojo_service_bind_attempted_ = false;

  // EcEvent-related members:
  WilcoDtcSupportdEcEventService ec_event_service_{this /* delegate */};

  // Diagnostic routine-related members:

  // Implementation of the diagnostic routine interface exposed by the
  // wilco_dtc_supportd daemon.
  WilcoDtcSupportdRoutineService routine_service_;

  DISALLOW_COPY_AND_ASSIGN(WilcoDtcSupportdCore);
};

}  // namespace diagnostics

#endif  // DIAGNOSTICS_WILCO_DTC_SUPPORTD_WILCO_DTC_SUPPORTD_CORE_H_
