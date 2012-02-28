// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef SHILL_DBUS_OBJECTMANAGER_INTERFACE_H_
#define SHILL_DBUS_OBJECTMANAGER_INTERFACE_H_
#include <string>

#include <base/basictypes.h>

#include "shill/dbus_properties.h"  // For DBusPropertiesMap

namespace shill {

class AsyncCallHandler;
class Error;

typedef std::map<std::string, DBusPropertiesMap> DBusInterfaceToProperties;
typedef
   std::map< ::DBus::Path, DBusInterfaceToProperties> DBusObjectsWithProperties;

// These are the methods that a org.freedesktop.DBus.ObjectManager
// proxy must support.  The interface is provided so that it can be
// mocked in tests.  All calls are made asynchronously. Call
// completion is signalled through the corresponding 'OnXXXCallback'
// method in the ProxyDelegate interface.

class DBusObjectManagerProxyInterface {
 public:
  virtual ~DBusObjectManagerProxyInterface() {}
  virtual void GetManagedObjects(
      AsyncCallHandler *call_handler, int timeout) = 0;
};

class DBusObjectManagerProxyDelegate {
 public:
  virtual ~DBusObjectManagerProxyDelegate() {}

  // Signals
  virtual void OnInterfacesAdded(
      const ::DBus::Path &object_path,
      const DBusInterfaceToProperties &interface_to_properties) = 0;
  virtual void OnInterfacesRemoved(
      const ::DBus::Path &object_path,
      const std::vector<std::string> &interfaces) = 0;

  // Async method callbacks
  virtual void OnGetManagedObjectsCallback(
      const DBusObjectsWithProperties &objects_with_properties,
      const shill::Error &error,
      AsyncCallHandler *call_handler) = 0;
};

}  // namespace shill

#endif  // SHILL_DBUS_OBJECTMANAGER_INTERFACE_H_
