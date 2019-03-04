// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DIAGNOSTICS_WILCO_DTC_SUPPORTD_WILCO_DTC_SUPPORTD_DAEMON_H_
#define DIAGNOSTICS_WILCO_DTC_SUPPORTD_WILCO_DTC_SUPPORTD_DAEMON_H_

#include <base/macros.h>
#include <brillo/daemons/dbus_daemon.h>

#include "diagnostics/wilco_dtc_supportd/wilco_dtc_supportd_core.h"
#include "diagnostics/wilco_dtc_supportd/wilco_dtc_supportd_core_delegate_impl.h"

namespace diagnostics {

// Daemon class for the wilco_dtc_supportd daemon.
class WilcoDtcSupportdDaemon final : public brillo::DBusServiceDaemon {
 public:
  WilcoDtcSupportdDaemon();
  ~WilcoDtcSupportdDaemon() override;

 private:
  // brillo::DBusServiceDaemon overrides:
  int OnInit() override;
  void RegisterDBusObjectsAsync(
      brillo::dbus_utils::AsyncEventSequencer* sequencer) override;
  void OnShutdown(int* error_code) override;

  WilcoDtcSupportdCoreDelegateImpl wilco_dtc_supportd_core_delegate_impl_{
      this /* daemon */};
  WilcoDtcSupportdCore wilco_dtc_supportd_core_;

  DISALLOW_COPY_AND_ASSIGN(WilcoDtcSupportdDaemon);
};

}  // namespace diagnostics

#endif  // DIAGNOSTICS_WILCO_DTC_SUPPORTD_WILCO_DTC_SUPPORTD_DAEMON_H_
