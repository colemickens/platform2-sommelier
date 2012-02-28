
// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef SHILL_DBUS_OBJECTMANAGER_PROXY_H_
#define SHILL_DBUS_OBJECTMANAGER_PROXY_H_
#include <string>

#include "shill/dbus_objectmanager_proxy_interface.h"
#include "shill/dbus_bindings/dbus-objectmanager.h"

namespace shill {

class DBusObjectManagerProxy : public DBusObjectManagerProxyInterface {
 public:
  // Constructs a org.freedesktop.DBus.ObjectManager DBus object proxy
  // at |path| owned by |service|. Caught signals and asynchronous
  // method replies will be dispatched to |delegate|.

  DBusObjectManagerProxy(DBusObjectManagerProxyDelegate *delegate,
                         DBus::Connection *connection,
                         const std::string &path,
                         const std::string &service);
  virtual ~DBusObjectManagerProxy();

  // Inherited methods from DBusObjectManagerProxyInterface.
  virtual void GetManagedObjects(AsyncCallHandler *call_handler, int timeout);

 private:
  class Proxy : public org::freedesktop::DBus::ObjectManager_proxy,
                public DBus::ObjectProxy {
   public:
    Proxy(DBusObjectManagerProxyDelegate *delegate,
          DBus::Connection *connection,
          const std::string &path,
          const std::string &service);
    virtual ~Proxy();

   private:
    // Signal callbacks
    virtual void InterfacesAdded(
        const ::DBus::Path &object_path,
        const DBusInterfaceToProperties &interfaces_and_properties);
    virtual void InterfacesRemoved(const ::DBus::Path &object_path,
                                   const std::vector<std::string> &interfaces);

    // Method callbacks
    virtual void GetManagedObjectsCallback(
        const DBusObjectsWithProperties &objects_with_properties,
        const DBus::Error &error,
        void *call_handler);

    DBusObjectManagerProxyDelegate *delegate_;

    DISALLOW_COPY_AND_ASSIGN(Proxy);
  };
  Proxy proxy_;

  DISALLOW_COPY_AND_ASSIGN(DBusObjectManagerProxy);
};

}  // namespace shill

#endif  // SHILL_DBUS_OBJECTMANAGER_PROXY_H_
