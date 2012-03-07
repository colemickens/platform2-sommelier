// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef SHILL_DBUS_OBJECTMANAGER_INTERFACE_H_
#define SHILL_DBUS_OBJECTMANAGER_INTERFACE_H_
#include <string>

#include <base/basictypes.h>
#include <base/callback.h>

#include "shill/dbus_properties.h"  // For DBusPropertiesMap

namespace shill {

class Error;

typedef std::map<std::string, DBusPropertiesMap> DBusInterfaceToProperties;
typedef std::map< ::DBus::Path, DBusInterfaceToProperties>
    DBusObjectsWithProperties;
typedef base::Callback<void(const DBusObjectsWithProperties &, const Error &)>
    ManagedObjectsCallback;
typedef base::Callback<void(const DBusInterfaceToProperties &, const Error &)>
    InterfaceAndPropertiesCallback;
typedef base::Callback<void(const DBus::Path &,
                            const DBusInterfaceToProperties &)>
    InterfacesAddedSignalCallback;
typedef base::Callback<void(const DBus::Path &,
                            const std::vector<std::string> &)>
    InterfacesRemovedSignalCallback;

// These are the methods that a org.freedesktop.DBus.ObjectManager
// proxy must support.  The interface is provided so that it can be
// mocked in tests.  All calls are made asynchronously. Call completion
// is signalled via the callbacks passed to the methods.
class DBusObjectManagerProxyInterface {
 public:
  virtual ~DBusObjectManagerProxyInterface() {}
  virtual void GetManagedObjects(Error *error,
                                 const ManagedObjectsCallback &callback,
                                 int timeout) = 0;
  virtual void set_interfaces_added_callback(
      const InterfacesAddedSignalCallback &callback) = 0;
  virtual void set_interfaces_removed_callback(
      const InterfacesRemovedSignalCallback &callback) = 0;
};

}  // namespace shill

#endif  // SHILL_DBUS_OBJECTMANAGER_INTERFACE_H_
