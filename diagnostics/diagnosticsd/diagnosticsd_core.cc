// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "diagnostics/diagnosticsd/diagnosticsd_core.h"

#include <utility>

#include <base/bind.h>
#include <base/files/file_util.h>
#include <base/logging.h>
#include <dbus/diagnosticsd/dbus-constants.h>
#include <dbus/object_path.h>
#include <mojo/public/cpp/system/message_pipe.h>

namespace diagnostics {

DiagnosticsdCore::DiagnosticsdCore(Delegate* delegate) : delegate_(delegate) {
  DCHECK(delegate);
}

DiagnosticsdCore::~DiagnosticsdCore() = default;

void DiagnosticsdCore::RegisterDBusObjectsAsync(
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
      &DiagnosticsdDBusService::BootstrapMojoConnection);
  dbus_object_->RegisterAsync(sequencer->GetHandler(
      "Failed to register D-Bus object" /* descriptive_message */,
      true /* failure_is_fatal */));
}

bool DiagnosticsdCore::StartMojoService(base::ScopedFD mojo_pipe_fd,
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
  mojo_service_ =
      std::make_unique<DiagnosticsdMojoService>(this /* delegate */);
  mojo_service_binding_ = delegate_->BindDiagnosticsdMojoService(
      mojo_service_.get(), std::move(mojo_pipe_fd));
  if (!mojo_service_binding_) {
    *error_message = "Failed to bootstrap Mojo";
    ShutDownDueToMojoError("Mojo bootstrap failed" /* debug_reason */);
    return false;
  }
  mojo_service_binding_->set_connection_error_handler(base::Bind(
      &DiagnosticsdCore::ShutDownDueToMojoError, base::Unretained(this),
      "Mojo connection error" /* debug_reason */));
  LOG(INFO) << "Successfully bootstrapped Mojo connection";
  return true;
}

void DiagnosticsdCore::ShutDownDueToMojoError(const std::string& debug_reason) {
  // Our daemon has to be restarted to be prepared for future Mojo connection
  // bootstraps. We can't do this without a restart since Mojo EDK gives no
  // guarantee to support repeated bootstraps. Therefore tear down and exit from
  // our process and let upstart to restart us again.
  LOG(INFO) << "Shutting down due to: " << debug_reason;
  mojo_service_binding_.reset();
  mojo_service_.reset();
  delegate_->BeginDaemonShutdown();
}

}  // namespace diagnostics
