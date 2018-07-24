// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_CELLULAR_DBUS_OBJECTMANAGER_PROXY_H_
#define SHILL_CELLULAR_DBUS_OBJECTMANAGER_PROXY_H_

#include <string>
#include <vector>

#include "shill/cellular/dbus_objectmanager_proxy_interface.h"
#include "shill/dbus_proxies/dbus-objectmanager.h"

namespace shill {

class DBusObjectManagerProxy : public DBusObjectManagerProxyInterface {
 public:
  // Constructs a org.freedesktop.DBus.ObjectManager DBus object proxy
  // at |path| owned by |service|.

  DBusObjectManagerProxy(DBus::Connection* connection,
                         const std::string& path,
                         const std::string& service);
  ~DBusObjectManagerProxy() override;

  // Inherited methods from DBusObjectManagerProxyInterface.
  void GetManagedObjects(Error* error,
                         const ManagedObjectsCallback& callback,
                         int timeout) override;

  void set_interfaces_added_callback(
      const InterfacesAddedSignalCallback& callback) override;
  void set_interfaces_removed_callback(
      const InterfacesRemovedSignalCallback& callback) override;

 private:
  class Proxy : public org::freedesktop::DBus::ObjectManager_proxy,
                public DBus::ObjectProxy {
   public:
    Proxy(DBus::Connection* connection,
          const std::string& path,
          const std::string& service);
    ~Proxy() override;

    virtual void set_interfaces_added_callback(
        const InterfacesAddedSignalCallback& callback);
    virtual void set_interfaces_removed_callback(
        const InterfacesRemovedSignalCallback& callback);

   private:
    // Signal callbacks
    void InterfacesAdded(
        const ::DBus::Path& object_path,
        const DBusInterfaceToProperties& interfaces_and_properties) override;
    void InterfacesRemoved(const ::DBus::Path& object_path,
                           const std::vector<std::string>& interfaces) override;

    // Method callbacks
    void GetManagedObjectsCallback(
        const DBusObjectsWithProperties& objects_with_properties,
        const DBus::Error& error,
        void* call_handler) override;

    InterfacesAddedSignalCallback interfaces_added_callback_;
    InterfacesRemovedSignalCallback interfaces_removed_callback_;

    DISALLOW_COPY_AND_ASSIGN(Proxy);
  };
  Proxy proxy_;

  DISALLOW_COPY_AND_ASSIGN(DBusObjectManagerProxy);
};

}  // namespace shill

#endif  // SHILL_CELLULAR_DBUS_OBJECTMANAGER_PROXY_H_
