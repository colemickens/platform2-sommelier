// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWERM_POWERMAN_H_
#define POWER_MANAGER_POWERM_POWERMAN_H_

#include <dbus/dbus-glib-lowlevel.h>
#include <sys/types.h>

#include "power_manager/common/power_constants.h"
#include "power_manager/common/signal_callback.h"
#include "power_manager/common/util_dbus_handler.h"

namespace power_manager {

class PowerManDaemon {
 public:
  PowerManDaemon();
  virtual ~PowerManDaemon();

  void Init();
  void Run();

 private:
  // Callbacks for handling dbus messages.
  bool HandleShutdownSignal(DBusMessage* message);
  bool HandleRestartSignal(DBusMessage* message);
  bool HandleRequestCleanShutdownSignal(DBusMessage* message);

  // Register the dbus message handler with appropriate dbus events.
  void RegisterDBusMessageHandler();

  // Restarts the system.
  void Restart();

  // Shuts the system down.  The |reason| parameter is passed as the
  // SHUTDOWN_REASON parameter to initctl.
  void Shutdown(const std::string& reason);

  GMainLoop* loop_;

  // This is the DBus helper object that dispatches DBus messages to handlers
  util::DBusHandler dbus_handler_;
};

}  // namespace power_manager

#endif  // POWER_MANAGER_POWERM_POWERMAN_H_
