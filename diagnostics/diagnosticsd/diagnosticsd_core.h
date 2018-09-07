// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DIAGNOSTICS_DIAGNOSTICSD_DIAGNOSTICSD_CORE_H_
#define DIAGNOSTICS_DIAGNOSTICSD_DIAGNOSTICSD_CORE_H_

#include <memory>
#include <string>

#include <base/files/scoped_file.h>
#include <base/macros.h>
#include <base/memory/ref_counted.h>
#include <brillo/dbus/async_event_sequencer.h>
#include <brillo/dbus/dbus_object.h>
#include <dbus/bus.h>
#include <mojo/public/cpp/bindings/binding.h>

#include "diagnostics/diagnosticsd/diagnosticsd_dbus_service.h"
#include "diagnostics/diagnosticsd/diagnosticsd_grpc_service.h"
#include "diagnostics/diagnosticsd/diagnosticsd_mojo_service.h"

#include "mojo/diagnosticsd.mojom.h"

namespace diagnostics {

// Integrates together all pieces which implement separate IPC services exposed
// by the diagnosticsd daemon and IPC clients.
class DiagnosticsdCore final : public DiagnosticsdDBusService::Delegate,
                               public DiagnosticsdGrpcService::Delegate,
                               public DiagnosticsdMojoService::Delegate {
 public:
  class Delegate {
   public:
    using MojomDiagnosticsdService =
        chromeos::diagnostics::mojom::DiagnosticsdService;

    virtual ~Delegate() = default;

    // Binds the given |mojo_service| to the Mojo message pipe that works via
    // the given |mojo_pipe_fd|. On success, returns the created Mojo binding,
    // otherwise returns nullptr.
    //
    // In production this method must be called no more than once during the
    // lifetime of the daemon, since Mojo EDK gives no guarantee to support
    // repeated initialization with different parent handles.
    virtual std::unique_ptr<mojo::Binding<MojomDiagnosticsdService>>
    BindDiagnosticsdMojoService(MojomDiagnosticsdService* mojo_service,
                                base::ScopedFD mojo_pipe_fd) = 0;

    // Begins the graceful shutdown of the diagnosticsd daemon.
    virtual void BeginDaemonShutdown() = 0;
  };

  explicit DiagnosticsdCore(Delegate* delegate);
  ~DiagnosticsdCore() override;

  // Register the D-Bus object that the diagnosticsd daemon exposes and tie
  // methods exposed by this object with the actual implementation.
  void RegisterDBusObjectsAsync(
      const scoped_refptr<dbus::Bus>& bus,
      brillo::dbus_utils::AsyncEventSequencer* sequencer);

 private:
  using MojomDiagnosticsdService =
      chromeos::diagnostics::mojom::DiagnosticsdService;

  // DiagnosticsdDBusService::Delegate overrides:
  bool StartMojoService(base::ScopedFD mojo_pipe_fd,
                        std::string* error_message) override;

  // Shuts down the self instance after a Mojo fatal error happens.
  void ShutDownDueToMojoError(const std::string& debug_reason);

  // Unowned. The delegate should outlive this instance.
  Delegate* const delegate_;
  // Implementation of the D-Bus interface exposed by the diagnosticsd daemon.
  DiagnosticsdDBusService dbus_service_{this /* delegate */};
  // Implementation of the gRPC interface exposed by the diagnosticsd daemon.
  DiagnosticsdGrpcService grpc_service_{this /* delegate */};
  // Implementation of the Mojo interface exposed by the diagnosticsd daemon and
  // a proxy that allows sending outgoing Mojo requests.
  //
  // Gets created after the BootstrapMojoConnection D-Bus method is called.
  std::unique_ptr<DiagnosticsdMojoService> mojo_service_;
  // Binding that connects |mojo_service_| with the message pipe set up on top
  // of the received file descriptor.
  std::unique_ptr<mojo::Binding<MojomDiagnosticsdService>>
      mojo_service_binding_;
  // Whether binding of the Mojo service was attempted. This flag is useful
  // during shutdown when |mojo_service_| and |mojo_service_binding_| may
  // already get destroyed.
  bool mojo_service_bind_attempted_ = false;
  // Connects |dbus_service_| with the methods of the D-Bus object exposed by
  // the diagnosticsd daemon.
  std::unique_ptr<brillo::dbus_utils::DBusObject> dbus_object_;

  DISALLOW_COPY_AND_ASSIGN(DiagnosticsdCore);
};

}  // namespace diagnostics

#endif  // DIAGNOSTICS_DIAGNOSTICSD_DIAGNOSTICSD_CORE_H_
