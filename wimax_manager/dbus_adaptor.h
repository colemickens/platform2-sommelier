// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WIMAX_MANAGER_DBUS_ADAPTOR_H_
#define WIMAX_MANAGER_DBUS_ADAPTOR_H_

#include <string>

#include <base/basictypes.h>
#include <dbus-c++/dbus.h>

namespace wimax_manager {

class DBusAdaptor : public DBus::ObjectAdaptor,
                    public DBus::PropertiesAdaptor,
                    public DBus::IntrospectableAdaptor {
 public:
  DBusAdaptor(DBus::Connection *connection, const std::string &object_path);
  virtual ~DBusAdaptor();

 private:
  DISALLOW_COPY_AND_ASSIGN(DBusAdaptor);
};

}  // namespace wimax_manager

#endif  // WIMAX_MANAGER_DBUS_ADAPTOR_H_
