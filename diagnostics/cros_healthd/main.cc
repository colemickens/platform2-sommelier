// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>

#include <brillo/flag_helper.h>
#include <brillo/syslog_logging.h>
#include <brillo/dbus/dbus_connection.h>

#include "debugd/dbus-proxies.h"
#include "diagnostics/cros_healthd/cros_healthd.h"
#include "diagnostics/cros_healthd/utils/battery_utils.h"
#include "diagnostics/cros_healthd/utils/disk_utils.h"

int main(int argc, char** argv) {
  brillo::FlagHelper::Init(
      argc, argv, "cros_healthd - Device telemetry and diagnostics daemon.");

  brillo::InitLog(brillo::kLogToSyslog | brillo::kLogToStderrIfTty);

  // Setting up the D-bus connection and initiating debugdProxy, which
  // cros_healthd will use to speak to debugd.
  brillo::DBusConnection connection;
  scoped_refptr<dbus::Bus> bus = connection.Connect();
  if (!bus) {
    LOG(ERROR) << "cros_healthd: Failed to connect to system D-bus bus";
    return EXIT_FAILURE;
  }

  auto proxy = std::make_unique<org::chromium::debugdProxy>(bus);
  diagnostics::CrosHealthd daemon(std::move(proxy));
  return daemon.Run();
}
