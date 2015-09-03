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

#ifndef SHILL_CELLULAR_MODEM_SIMPLE_PROXY_H_
#define SHILL_CELLULAR_MODEM_SIMPLE_PROXY_H_

#include <string>

#include <base/macros.h>

#include "dbus_proxies/org.freedesktop.ModemManager.Modem.Simple.h"
#include "shill/cellular/modem_simple_proxy_interface.h"

namespace shill {

// A proxy to (old) ModemManager.Modem.Simple.
class ModemSimpleProxy : public ModemSimpleProxyInterface {
 public:
  // Constructs a ModemManager.Modem.Simple DBus object proxy at
  // |path| owned by |service|.
  ModemSimpleProxy(DBus::Connection* connection,
                   const std::string& path,
                   const std::string& service);
  ~ModemSimpleProxy() override;

  // Inherited from ModemSimpleProxyInterface.
  void GetModemStatus(Error* error,
                      const DBusPropertyMapCallback& callback,
                      int timeout) override;
  void Connect(const DBusPropertiesMap& properties,
               Error* error,
               const ResultCallback& callback,
               int timeout) override;

 private:
  class Proxy : public org::freedesktop::ModemManager::Modem::Simple_proxy,
                public DBus::ObjectProxy {
   public:
    Proxy(DBus::Connection* connection,
          const std::string& path,
          const std::string& service);
    ~Proxy() override;

   private:
    // Signal callbacks inherited from ModemManager::Modem::Simple_proxy.
    // [None]

    // Method callbacks inherited from ModemManager::Modem::Simple_proxy.
    void GetStatusCallback(const DBusPropertiesMap& props,
                           const DBus::Error& dberror, void* data) override;
    void ConnectCallback(const DBus::Error& dberror, void* data) override;

    DISALLOW_COPY_AND_ASSIGN(Proxy);
  };

  Proxy proxy_;

  DISALLOW_COPY_AND_ASSIGN(ModemSimpleProxy);
};

}  // namespace shill

#endif  // SHILL_CELLULAR_MODEM_SIMPLE_PROXY_H_
