// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <brillo/daemons/daemon.h>
#include <brillo/flag_helper.h>
#include <brillo/syslog_logging.h>

int main(int argc, char** argv) {
  brillo::FlagHelper::Init(argc, argv,
                           "wilco_dtc - Device telemetry and diagnostics data "
                           "processing daemon.");

  brillo::InitLog(brillo::kLogToSyslog | brillo::kLogToStderrIfTty);

  return brillo::Daemon().Run();
}
