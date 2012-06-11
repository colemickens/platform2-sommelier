// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WIMAX_MANAGER_DBUS_SERVICE_DBUS_PROXY_H_
#define WIMAX_MANAGER_DBUS_SERVICE_DBUS_PROXY_H_

#include <string>

#include "wimax_manager/dbus_bindings/dbus_service.h"
#include "wimax_manager/dbus_proxy.h"

namespace wimax_manager {

class DBusService;

class DBusServiceDBusProxy : public org::freedesktop::DBus_proxy,
                             public DBusProxy {
 public:
  DBusServiceDBusProxy(DBus::Connection *connection, DBusService *dbus_service);
  virtual ~DBusServiceDBusProxy();

  virtual void NameOwnerChanged(const std::string &name,
                                const std::string &old_owner,
                                const std::string &new_owner);

 private:
  DBusService *dbus_service_;

  DISALLOW_COPY_AND_ASSIGN(DBusServiceDBusProxy);
};

}  // namespace wimax_manager

#endif  // WIMAX_MANAGER_DBUS_SERVICE_DBUS_PROXY_H_
