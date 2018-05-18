// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/at_exit.h>
#include <base/logging.h>
#include <base/message_loop/message_loop.h>
#include <base/run_loop.h>
#include <brillo/flag_helper.h>
#include <brillo/syslog_logging.h>

#include "bluetooth/common/dbus_daemon.h"
#include "bluetooth/dispatcher/dispatcher_daemon.h"

int main(int argc, char** argv) {
  brillo::FlagHelper::Init(argc, argv,
                           "btdispatch, the Chromium OS Bluetooth service.");

  brillo::InitLog(brillo::kLogToSyslog | brillo::kLogToStderrIfTty);

  bluetooth::DBusDaemon daemon(std::make_unique<bluetooth::DispatcherDaemon>());
  return daemon.Run();
}
