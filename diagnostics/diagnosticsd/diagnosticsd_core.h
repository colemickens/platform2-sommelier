// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DIAGNOSTICS_DIAGNOSTICSD_DIAGNOSTICSD_CORE_H_
#define DIAGNOSTICS_DIAGNOSTICSD_DIAGNOSTICSD_CORE_H_

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

#include "diagnostics/diagnosticsd/diagnosticsd_dbus_service.h"
#include "diagnostics/diagnosticsd/diagnosticsd_grpc_service.h"
#include "diagnostics/diagnosticsd/diagnosticsd_mojo_service.h"
#include "diagnostics/grpc_async_adapter/async_grpc_client.h"
#include "diagnostics/grpc_async_adapter/async_grpc_server.h"

#include "diagnostics_processor.grpc.pb.h"  // NOLINT(build/include)
#include "diagnosticsd.grpc.pb.h"           // NOLINT(build/include)
#include "mojo/diagnosticsd.mojom.h"

namespace diagnostics {

// Integrates together all pieces which implement separate IPC services exposed
// by the diagnosticsd daemon and IPC clients.
class DiagnosticsdCore final
    : public DiagnosticsdDBusService::Delegate,
      public DiagnosticsdGrpcService::Delegate,
      public DiagnosticsdMojoService::Delegate,
      public chromeos::diagnosticsd::mojom::DiagnosticsdServiceFactory {
 public:
  class Delegate {
   public:
    using MojomDiagnosticsdServiceFactory =
        chromeos::diagnosticsd::mojom::DiagnosticsdServiceFactory;

    virtual ~Delegate() = default;

    // Binds the given |mojo_service_factory| to the Mojo message pipe that
    // works via the given |mojo_pipe_fd|. On success, returns the created Mojo
    // binding, otherwise returns nullptr.
    //
    // In production this method must be called no more than once during the
    // lifetime of the daemon, since Mojo EDK gives no guarantee to support
    // repeated initialization with different parent handles.
    virtual std::unique_ptr<mojo::Binding<MojomDiagnosticsdServiceFactory>>
    BindDiagnosticsdMojoServiceFactory(
        MojomDiagnosticsdServiceFactory* mojo_service_factory,
        base::ScopedFD mojo_pipe_fd) = 0;

    // Begins the graceful shutdown of the diagnosticsd daemon.
    virtual void BeginDaemonShutdown() = 0;
  };

  // |grpc_service_uri| is the URI on which the gRPC interface exposed by the
  // diagnosticsd daemon will be listening.
  // |diagnostics_processor_grpc_uri| is the URI which is used for making
  // requests to the gRPC interface exposed by the diagnostics_processor daemon.
  DiagnosticsdCore(const std::string& grpc_service_uri,
                   const std::string& diagnostics_processor_grpc_uri,
                   Delegate* delegate);
  ~DiagnosticsdCore() override;

  // Overrides the file system root directory for file operations in tests.
  void set_root_dir_for_testing(const base::FilePath& root_dir) {
    grpc_service_.set_root_dir_for_testing(root_dir);
  }

  // Starts gRPC servers and clients.
  bool StartGrpcCommunication();

  // Performs asynchronous shutdown and cleanup of gRPC servers and clients.
  // This must be used before deleting this instance in case
  // StartGrpcCommunication() was called and returned success - in that case,
  // the instance must be destroyed only after |on_torn_down| has been called.
  void TearDownGrpcCommunication(const base::Closure& on_torn_down);

  // Register the D-Bus object that the diagnosticsd daemon exposes and tie
  // methods exposed by this object with the actual implementation.
  void RegisterDBusObjectsAsync(
      const scoped_refptr<dbus::Bus>& bus,
      brillo::dbus_utils::AsyncEventSequencer* sequencer);

 private:
  using MojomDiagnosticsdClientPtr =
      chromeos::diagnosticsd::mojom::DiagnosticsdClientPtr;
  using MojomDiagnosticsdServiceRequest =
      chromeos::diagnosticsd::mojom::DiagnosticsdServiceRequest;

  // DiagnosticsdDBusService::Delegate overrides:
  bool StartMojoServiceFactory(base::ScopedFD mojo_pipe_fd,
                               std::string* error_message) override;

  // Shuts down the self instance after a Mojo fatal error happens.
  void ShutDownDueToMojoError(const std::string& debug_reason);

  // DiagnosticsGrpcService::Delegate overrides:
  void PerformWebRequestToBrowser(
      WebRequestHttpMethod http_method,
      const std::string& url,
      const std::vector<std::string>& headers,
      const std::string& request_body,
      const PerformWebRequestToBrowserCallback& callback) override;

  // DiagnosticsdMojoService::Delegate overrides:
  void SendGrpcUiMessageToDiagnosticsProcessor(
      base::StringPiece json_message,
      const SendGrpcUiMessageToDiagnosticsProcessorCallback& callback) override;

  // chromeos::diagnosticsd::mojom::DiagnosticsdServiceFactory overrides:
  void GetService(MojomDiagnosticsdServiceRequest service,
                  MojomDiagnosticsdClientPtr client,
                  const GetServiceCallback& callback) override;

  // Unowned. The delegate should outlive this instance.
  Delegate* const delegate_;

  // gRPC-related members:

  // gRPC URI on which the |grpc_server_| is listening for incoming requests.
  const std::string grpc_service_uri_;
  // gRPC URI which is used by |diagnostics_processor_grpc_client_| for
  // accessing the gRPC interface exposed by the diagnostics_processor daemon.
  const std::string diagnostics_processor_grpc_uri_;
  // Implementation of the gRPC interface exposed by the diagnosticsd daemon.
  DiagnosticsdGrpcService grpc_service_{this /* delegate */};
  // Connects |grpc_service_| with the gRPC server that listens for incoming
  // requests.
  AsyncGrpcServer<grpc_api::Diagnosticsd::AsyncService> grpc_server_;
  // Allows to make outgoing requests to the gRPC interface exposed by the
  // diagnostics_processor daemon.
  std::unique_ptr<AsyncGrpcClient<grpc_api::DiagnosticsProcessor>>
      diagnostics_processor_grpc_client_;

  // D-Bus-related members:

  // Implementation of the D-Bus interface exposed by the diagnosticsd daemon.
  DiagnosticsdDBusService dbus_service_{this /* delegate */};
  // Connects |dbus_service_| with the methods of the D-Bus object exposed by
  // the diagnosticsd daemon.
  std::unique_ptr<brillo::dbus_utils::DBusObject> dbus_object_;

  // Mojo-related members:

  // Binding that connects this instance (which is an implementation of
  // chromeos::diagnosticsd::mojom::DiagnosticsdServiceFactory) with the message
  // pipe set up on top of the received file descriptor.
  //
  // Gets created after the BootstrapMojoConnection D-Bus method is called.
  std::unique_ptr<mojo::Binding<DiagnosticsdServiceFactory>>
      mojo_service_factory_binding_;
  // Implementation of the Mojo interface exposed by the diagnosticsd daemon and
  // a proxy that allows sending outgoing Mojo requests.
  //
  // Gets created after the GetService() Mojo method is called.
  std::unique_ptr<DiagnosticsdMojoService> mojo_service_;
  // Whether binding of the Mojo service was attempted.
  //
  // This flag is needed for detecting repeated Mojo bootstrapping attempts
  // (alternative ways, like checking |mojo_service_factory_binding_|, are
  // unreliable during shutdown).
  bool mojo_service_bind_attempted_ = false;

  DISALLOW_COPY_AND_ASSIGN(DiagnosticsdCore);
};

}  // namespace diagnostics

#endif  // DIAGNOSTICS_DIAGNOSTICSD_DIAGNOSTICSD_CORE_H_
