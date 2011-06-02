// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_DBUS_ADAPTOR_H_
#define SHILL_DBUS_ADAPTOR_H_

#include <string>

#include <base/basictypes.h>
#include <dbus-c++/dbus.h>

namespace shill {

#define SHILL_INTERFACE "org.chromium.flimflam"
#define SHILL_PATH "/org/chromium/flimflam"

// Superclass for all DBus-backed Adaptor objects
class DBusAdaptor : public DBus::ObjectAdaptor {
 public:
  DBusAdaptor(DBus::Connection* conn, const std::string& object_path);
  virtual ~DBusAdaptor();

  static ::DBus::Variant BoolToVariant(bool value);
  static ::DBus::Variant UInt32ToVariant(uint32 value);
  static ::DBus::Variant IntToVariant(int value);
  static ::DBus::Variant StringToVariant(const std::string& value);

 protected:
    DISALLOW_COPY_AND_ASSIGN(DBusAdaptor);
};

}  // namespace shill
#endif  // SHILL_DBUS_ADAPTOR_H_
