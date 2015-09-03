//
// Copyright (C) 2012 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

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
