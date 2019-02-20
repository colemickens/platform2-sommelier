// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <brillo/flag_helper.h>
#include <brillo/syslog_logging.h>

#include "diagnostics/wilco_dtc_supportd/diagnosticsd_daemon.h"

int main(int argc, char** argv) {
  brillo::FlagHelper::Init(
      argc, argv, "diagnosticsd - Device telemetry and diagnostics daemon.");

  brillo::InitLog(brillo::kLogToSyslog | brillo::kLogToStderrIfTty);

  return diagnostics::DiagnosticsdDaemon().Run();
}
