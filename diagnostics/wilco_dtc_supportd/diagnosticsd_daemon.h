// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DIAGNOSTICS_WILCO_DTC_SUPPORTD_DIAGNOSTICSD_DAEMON_H_
#define DIAGNOSTICS_WILCO_DTC_SUPPORTD_DIAGNOSTICSD_DAEMON_H_

#include <base/macros.h>
#include <brillo/daemons/dbus_daemon.h>

#include "diagnostics/wilco_dtc_supportd/diagnosticsd_core.h"
#include "diagnostics/wilco_dtc_supportd/diagnosticsd_core_delegate_impl.h"

namespace diagnostics {

// Daemon class for the diagnosticsd daemon.
class DiagnosticsdDaemon final : public brillo::DBusServiceDaemon {
 public:
  DiagnosticsdDaemon();
  ~DiagnosticsdDaemon() override;

 private:
  // brillo::DBusServiceDaemon overrides:
  int OnInit() override;
  void RegisterDBusObjectsAsync(
      brillo::dbus_utils::AsyncEventSequencer* sequencer) override;
  void OnShutdown(int* error_code) override;

  DiagnosticsdCoreDelegateImpl diagnosticsd_core_delegate_impl_{
      this /* daemon */};
  DiagnosticsdCore diagnosticsd_core_;

  DISALLOW_COPY_AND_ASSIGN(DiagnosticsdDaemon);
};

}  // namespace diagnostics

#endif  // DIAGNOSTICS_WILCO_DTC_SUPPORTD_DIAGNOSTICSD_DAEMON_H_
