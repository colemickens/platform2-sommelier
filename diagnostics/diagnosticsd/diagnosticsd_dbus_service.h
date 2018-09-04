// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DIAGNOSTICS_DIAGNOSTICSD_DIAGNOSTICSD_DBUS_SERVICE_H_
#define DIAGNOSTICS_DIAGNOSTICSD_DIAGNOSTICSD_DBUS_SERVICE_H_

#include <base/files/scoped_file.h>
#include <base/macros.h>
#include <dbus/exported_object.h>
#include <dbus/message.h>

namespace diagnostics {

// Implements the "org.chromium.DiagnosticsdInterface" D-Bus interface exposed
// by the diagnosticsd daemon (see constants for the API methods at
// src/platform/system_api/dbus/diagnosticsd/dbus-constants.h).
class DiagnosticsdDbusService final {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;
  };

  explicit DiagnosticsdDbusService(Delegate* delegate);
  ~DiagnosticsdDbusService();

  // Implementation of the "org.chromium.DiagnosticsdInterface" D-Bus interface:
  void BootstrapMojoConnection(const base::ScopedFD& mojo_fd);

 private:
  // Unowned. The delegate should outlive this instance.
  Delegate* const delegate_;

  DISALLOW_COPY_AND_ASSIGN(DiagnosticsdDbusService);
};

}  // namespace diagnostics

#endif  // DIAGNOSTICS_DIAGNOSTICSD_DIAGNOSTICSD_DBUS_SERVICE_H_
