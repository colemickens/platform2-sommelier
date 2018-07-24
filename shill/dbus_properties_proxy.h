// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_DBUS_PROPERTIES_PROXY_H_
#define SHILL_DBUS_PROPERTIES_PROXY_H_

#include <string>
#include <vector>

#include <base/macros.h>

#include "shill/dbus_properties_proxy_interface.h"
#include "shill/dbus_proxies/dbus-properties.h"

namespace shill {

class DBusPropertiesProxy : public DBusPropertiesProxyInterface {
 public:
  DBusPropertiesProxy(DBus::Connection* connection,
                      const std::string& path,
                      const std::string& service);
  ~DBusPropertiesProxy() override;

  // Inherited from DBusPropertiesProxyInterface.
  DBusPropertiesMap GetAll(const std::string& interface_name) override;
  DBus::Variant Get(const std::string& interface_name,
                    const std::string& property) override;

  void set_properties_changed_callback(
      const PropertiesChangedCallback& callback) override;
  void set_modem_manager_properties_changed_callback(
      const ModemManagerPropertiesChangedCallback& callback) override;

 private:
  class Proxy : public org::freedesktop::DBus::Properties_proxy,
                public DBus::ObjectProxy {
   public:
    Proxy(DBus::Connection* connection,
          const std::string& path,
          const std::string& service);
    ~Proxy() override;

    virtual void set_properties_changed_callback(
        const PropertiesChangedCallback& callback);
    virtual void set_modem_manager_properties_changed_callback(
        const ModemManagerPropertiesChangedCallback& callback);

   private:
    // Signal callbacks inherited from DBusProperties_proxy.
    void MmPropertiesChanged(const std::string& interface,
                             const DBusPropertiesMap& properties) override;

    void PropertiesChanged(
        const std::string& interface,
        const DBusPropertiesMap& changed_properties,
        const std::vector<std::string>& invalidated_properties) override;

    PropertiesChangedCallback properties_changed_callback_;
    ModemManagerPropertiesChangedCallback mm_properties_changed_callback_;

    DISALLOW_COPY_AND_ASSIGN(Proxy);
  };

  Proxy proxy_;

  DISALLOW_COPY_AND_ASSIGN(DBusPropertiesProxy);
};

}  // namespace shill

#endif  // SHILL_DBUS_PROPERTIES_PROXY_H_
