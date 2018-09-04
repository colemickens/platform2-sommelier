// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DIAGNOSTICS_DIAGNOSTICSD_DIAGNOSTICSD_DAEMON_H_
#define DIAGNOSTICS_DIAGNOSTICSD_DIAGNOSTICSD_DAEMON_H_

#include <memory>

#include <base/macros.h>
#include <brillo/daemons/dbus_daemon.h>
#include <brillo/dbus/dbus_object.h>

#include "diagnostics/diagnosticsd/diagnosticsd_dbus_service.h"
#include "diagnostics/diagnosticsd/diagnosticsd_grpc_service.h"
#include "diagnostics/diagnosticsd/diagnosticsd_mojo_service.h"

namespace diagnostics {

// Daemon class for the diagnosticsd daemon. Integrates together all pieces
// which implement separate IPC services and clients.
class DiagnosticsdDaemon final : public brillo::DBusServiceDaemon,
                                 public DiagnosticsdDbusService::Delegate,
                                 public DiagnosticsdGrpcService::Delegate,
                                 public DiagnosticsdMojoService::Delegate {
 public:
  DiagnosticsdDaemon();
  ~DiagnosticsdDaemon() override;

 private:
  // DBusServiceDaemon overrides:
  void RegisterDBusObjectsAsync(
      brillo::dbus_utils::AsyncEventSequencer* sequencer) override;

  DiagnosticsdDbusService dbus_service_{this /* delegate */};
  DiagnosticsdGrpcService grpc_service_{this /* delegate */};
  std::unique_ptr<DiagnosticsdMojoService> mojo_service_;
  std::unique_ptr<brillo::dbus_utils::DBusObject> dbus_object_;

  DISALLOW_COPY_AND_ASSIGN(DiagnosticsdDaemon);
};

}  // namespace diagnostics

#endif  // DIAGNOSTICS_DIAGNOSTICSD_DIAGNOSTICSD_DAEMON_H_
