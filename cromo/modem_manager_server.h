// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CROMO_MODEM_MANAGER_SERVER_H_
#define CROMO_MODEM_MANAGER_SERVER_H_

#include <dbus-c++/dbus.h>
#include "manager_server_glue.h"

class ModemManager;

// Implements the ModemManager DBus API, and manages the
// modem manager instances that handle specific types of
// modems.
class ModemManagerServer
    : public org::freedesktop::ModemManager_adaptor,
      public DBus::IntrospectableAdaptor,
      public DBus::ObjectAdaptor {
 public:
  ModemManagerServer(DBus::Connection& connection);
  ~ModemManagerServer();

  void AddModemManager(ModemManager* manager);

  // ModemManager DBUS API methods
  std::vector<DBus::Path> EnumerateDevices();

  static const char* SERVER_NAME;
  static const char* SERVER_PATH;

 private:
  // The modem managers that we are managing
  std::vector<ModemManager*> modem_managers_;
};

#endif // CROMO_MODEM_MANAGER_SERVER_H_
