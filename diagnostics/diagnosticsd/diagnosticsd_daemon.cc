// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "diagnostics/diagnosticsd/diagnosticsd_daemon.h"

#include <base/bind.h>
#include <base/logging.h>
#include <brillo/dbus/async_event_sequencer.h>
#include <dbus/diagnosticsd/dbus-constants.h>
#include <dbus/object_path.h>

namespace diagnostics {

DiagnosticsdDaemon::DiagnosticsdDaemon()
    : DBusServiceDaemon(kDiagnosticsdServiceName /* service_name */) {}

DiagnosticsdDaemon::~DiagnosticsdDaemon() = default;

void DiagnosticsdDaemon::RegisterDBusObjectsAsync(
    brillo::dbus_utils::AsyncEventSequencer* sequencer) {
  DCHECK(bus_);
  dbus_object_ = std::make_unique<brillo::dbus_utils::DBusObject>(
      nullptr /* object_manager */, bus_,
      dbus::ObjectPath(kDiagnosticsdServicePath));
  brillo::dbus_utils::DBusInterface* dbus_interface =
      dbus_object_->AddOrGetInterface(kDiagnosticsdServiceInterface);
  dbus_interface->AddSimpleMethodHandler(
      kDiagnosticsdBootstrapMojoConnectionMethod,
      base::Unretained(&dbus_service_),
      &DiagnosticsdDbusService::BootstrapMojoConnection);
}

}  // namespace diagnostics
