// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/at_exit.h>
#include <base/logging.h>
#include <base/message_loop/message_loop.h>
#include <base/run_loop.h>
#include <brillo/flag_helper.h>
#include <brillo/syslog_logging.h>

#include "bluetooth/dispatcher/daemon.h"

int main(int argc, char** argv) {
  brillo::FlagHelper::Init(argc, argv,
                           "btdispatch, the Chromium OS Bluetooth service.");

  brillo::InitLog(brillo::kLogToSyslog | brillo::kLogToStderrIfTty);

  base::AtExitManager exit_manager;
  base::MessageLoopForIO message_loop;

  dbus::Bus::Options options;
  options.bus_type = dbus::Bus::SYSTEM;
  scoped_refptr<dbus::Bus> bus(new dbus::Bus(options));

  if (!bus->Connect()) {
    LOG(ERROR) << "Failed to connect to system bus";
    return 1;
  }

  bluetooth::Daemon daemon(bus);
  daemon.Init();

  base::RunLoop().Run();
  return 0;
}
