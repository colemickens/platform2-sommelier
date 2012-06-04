// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WIMAX_MANAGER_DBUS_PROXY_H_
#define WIMAX_MANAGER_DBUS_PROXY_H_

#include <string>

#include <base/basictypes.h>
#include <dbus-c++/dbus.h>

namespace wimax_manager {

class DBusProxy : public DBus::ObjectProxy {
 public:
  DBusProxy(DBus::Connection *connection,
            const std::string &service_name, const std::string &object_path);
  virtual ~DBusProxy();

 private:
  DISALLOW_COPY_AND_ASSIGN(DBusProxy);
};

}  // namespace wimax_manager

#endif  // WIMAX_MANAGER_DBUS_PROXY_H_
