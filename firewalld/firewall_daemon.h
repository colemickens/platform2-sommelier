// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FIREWALLD_FIREWALL_DAEMON_H_
#define FIREWALLD_FIREWALL_DAEMON_H_

#include <dbus/dbus.h>
#include <stdint.h>

#include <base/macros.h>

namespace firewalld {

class FirewallDaemon {
 public:
  FirewallDaemon();
  virtual ~FirewallDaemon();

  // Initializes the daemon and loops waiting for requests on the DBus
  // interface. Never returns.
  void Run();

 private:
  // The callback invoked by the DBus method handler in order to dispatch method
  // calls to their individual handlers.
  static DBusHandlerResult MainDBusMethodHandler(DBusConnection* connection,
                                                 DBusMessage* message,
                                                 void* data);

  DBusMessage* HandlePunchHoleMethod(DBusMessage* message);
  DBusMessage* HandlePlugHoleMethod(DBusMessage* message);

  DISALLOW_COPY_AND_ASSIGN(FirewallDaemon);
};

}  // namespace firewalld

#endif  // FIREWALLD_FIREWALL_DAEMON_H_
