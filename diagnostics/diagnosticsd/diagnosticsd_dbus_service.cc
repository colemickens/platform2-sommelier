// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "diagnostics/diagnosticsd/diagnosticsd_dbus_service.h"

#include <unistd.h>
#include <utility>

#include <base/location.h>
#include <base/logging.h>
#include <base/posix/eintr_wrapper.h>
#include <brillo/errors/error_codes.h>
#include <dbus/dbus-protocol.h>

namespace diagnostics {

DiagnosticsdDBusService::DiagnosticsdDBusService(Delegate* delegate)
    : delegate_(delegate) {
  DCHECK(delegate_);
}

DiagnosticsdDBusService::~DiagnosticsdDBusService() = default;

bool DiagnosticsdDBusService::BootstrapMojoConnection(
    brillo::ErrorPtr* error, const base::ScopedFD& mojo_fd) {
  VLOG(0) << "Received BootstrapMojoConnection D-Bus request";
  std::string error_message;
  if (!DoBootstrapMojoConnection(mojo_fd, &error_message)) {
    *error = brillo::Error::Create(FROM_HERE, brillo::errors::dbus::kDomain,
                                   DBUS_ERROR_FAILED, error_message);
    return false;
  }
  return true;
}

bool DiagnosticsdDBusService::DoBootstrapMojoConnection(
    const base::ScopedFD& mojo_fd, std::string* error_message) {
  if (!mojo_fd.is_valid()) {
    LOG(ERROR) << "Invalid Mojo file descriptor";
    *error_message = "Invalid file descriptor";
    return false;
  }

  // We need a file descriptor that stays alive after the current method
  // finishes, but libbrillo's D-Bus wrappers currently don't support passing
  // base::ScopedFD by value.
  base::ScopedFD mojo_fd_copy(HANDLE_EINTR(dup(mojo_fd.get())));
  if (!mojo_fd_copy.is_valid()) {
    PLOG(ERROR) << "Failed to duplicate the Mojo file descriptor";
    *error_message = "Failed to duplicate file descriptor";
    return false;
  }

  return delegate_->StartMojoServiceFactory(std::move(mojo_fd_copy),
                                            error_message);
}

}  // namespace diagnostics
