// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <dbus/dbus-shared.h>
#include <stdio.h>
#include <stdlib.h>

#include <string>

#include "base/bind.h"
#include "chromeos/dbus/dbus.h"
#include "chromeos/dbus/service_constants.h"
#include "power_manager/common/util.h"
#include "power_manager/common/util_dbus.h"
#include "power_manager/powerm/powerman.h"

namespace power_manager {

PowerManDaemon::PowerManDaemon() : loop_(NULL) {}

PowerManDaemon::~PowerManDaemon() {}

void PowerManDaemon::Init() {
  RegisterDBusMessageHandler();
  loop_ = g_main_loop_new(NULL, false);
}

void PowerManDaemon::Run() {
  g_main_loop_run(loop_);
}

bool PowerManDaemon::HandleShutdownSignal(DBusMessage* message) {
  const char* reason = '\0';
  DBusError error;
  dbus_error_init(&error);
  if (dbus_message_get_args(message, &error,
                            DBUS_TYPE_STRING, &reason,
                            DBUS_TYPE_INVALID) == FALSE) {
    reason = kShutdownReasonUnknown;
    dbus_error_free(&error);
  }
  Shutdown(reason);
  return true;
}

bool PowerManDaemon::HandleRestartSignal(DBusMessage*) {  // NOLINT
  Restart();
  return true;
}

bool PowerManDaemon::HandleRequestCleanShutdownSignal(DBusMessage*) { // NOLINT
  util::Launch("initctl emit power-manager-clean-shutdown");
  return true;
}

void PowerManDaemon::RegisterDBusMessageHandler() {
  util::RequestDBusServiceName(kRootPowerManagerServiceName);

  dbus_handler_.AddDBusSignalHandler(
      kRootPowerManagerInterface,
      kShutdownSignal,
      base::Bind(&PowerManDaemon::HandleShutdownSignal,
                 base::Unretained(this)));
  dbus_handler_.AddDBusSignalHandler(
      kRootPowerManagerInterface,
      kRestartSignal,
      base::Bind(&PowerManDaemon::HandleRestartSignal, base::Unretained(this)));
  dbus_handler_.AddDBusSignalHandler(
      kRootPowerManagerInterface,
      kRequestCleanShutdown,
      base::Bind(&PowerManDaemon::HandleRequestCleanShutdownSignal,
                 base::Unretained(this)));

  dbus_handler_.Start();
}

void PowerManDaemon::Shutdown(const std::string& reason) {
  std::string command = "initctl emit --no-wait runlevel RUNLEVEL=0";
  if (!reason.empty())
    command += std::string(" SHUTDOWN_REASON=") + reason;
  util::Launch(command.c_str());
}

void PowerManDaemon::Restart() {
  util::Launch("shutdown -r now");
}

}  // namespace power_manager
