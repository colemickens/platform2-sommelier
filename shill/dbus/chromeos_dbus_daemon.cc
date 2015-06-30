// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/dbus/chromeos_dbus_daemon.h"

#include <sysexits.h>

#include <chromeos/dbus/service_constants.h>

#include "shill/dbus/chromeos_dbus_control.h"

namespace shill {

ChromeosDBusDaemon::ChromeosDBusDaemon(const Settings& settings,
                                       Config* config)
    : DBusServiceDaemon(kFlimflamServiceName, kFlimflamServicePath),
      ChromeosDaemon(
          settings,
          config,
          new ChromeosDBusControl(object_manager_->AsWeakPtr(), bus_)) {}

void ChromeosDBusDaemon::RunMessageLoop() {
  DBusServiceDaemon::Run();
}

int ChromeosDBusDaemon::OnInit() {
  // Manager DBus interface will get registered as part of this init call.
  int return_code = chromeos::DBusServiceDaemon::OnInit();
  if (return_code != EX_OK) {
    return return_code;
  }

  // Start manager.
  ChromeosDaemon::Start();

  return EX_OK;
}

void ChromeosDBusDaemon::OnShutdown(int* return_code) {
  ChromeosDaemon::Quit();
  chromeos::DBusServiceDaemon::OnShutdown(return_code);
}

void ChromeosDBusDaemon::RegisterDBusObjectsAsync(
    chromeos::dbus_utils::AsyncEventSequencer* sequencer) {
  // TODO(zqiu): Register "org.chromium.flimflam.Manager" interface.
  // The daemon will request the ownership of the DBus service
  // "org.chromium.flimflam" after Manager interface registration is
  // completed.
}

}  // namespace shill
