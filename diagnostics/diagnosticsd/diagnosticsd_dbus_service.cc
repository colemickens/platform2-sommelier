// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "diagnostics/diagnosticsd/diagnosticsd_dbus_service.h"

#include <base/logging.h>

namespace diagnostics {

DiagnosticsdDbusService::DiagnosticsdDbusService(Delegate* delegate)
    : delegate_(delegate) {
  DCHECK(delegate_);
}

DiagnosticsdDbusService::~DiagnosticsdDbusService() = default;

void DiagnosticsdDbusService::BootstrapMojoConnection(
    const base::ScopedFD& mojo_fd) {
  NOTIMPLEMENTED();
}

}  // namespace diagnostics
