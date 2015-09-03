//
// Copyright (C) 2011 The Android Open Source Project
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
