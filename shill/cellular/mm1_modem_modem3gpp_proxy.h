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

#ifndef SHILL_CELLULAR_MM1_MODEM_MODEM3GPP_PROXY_H_
#define SHILL_CELLULAR_MM1_MODEM_MODEM3GPP_PROXY_H_

#include <string>
#include <vector>

#include "dbus_proxies/org.freedesktop.ModemManager1.Modem.Modem3gpp.h"
#include "shill/cellular/mm1_modem_modem3gpp_proxy_interface.h"
#include "shill/dbus_properties.h"

namespace shill {
namespace mm1 {

// A proxy to org.freedesktop.ModemManager1.Modem.Modem3gpp.
class ModemModem3gppProxy : public ModemModem3gppProxyInterface {
 public:
  // Constructs an org.freedesktop.ModemManager1.Modem.Modem3gpp DBus
  // object proxy at |path| owned by |service|.
  ModemModem3gppProxy(DBus::Connection* connection,
                      const std::string& path,
                      const std::string& service);
  ~ModemModem3gppProxy() override;
  // Inherited methods from ModemModem3gppProxyInterface.
  void Register(const std::string& operator_id,
                Error* error,
                const ResultCallback& callback,
                int timeout) override;
  void Scan(Error* error,
            const DBusPropertyMapsCallback& callback,
            int timeout) override;

 private:
  class Proxy : public org::freedesktop::ModemManager1::Modem::Modem3gpp_proxy,
                public DBus::ObjectProxy {
   public:
    Proxy(DBus::Connection* connection,
          const std::string& path,
          const std::string& service);
    ~Proxy() override;

   private:
    // Method callbacks inherited from
    // org::freedesktop::ModemManager1::Modem::Modem3gppProxy
    void RegisterCallback(const ::DBus::Error& dberror, void* data) override;
    void ScanCallback(
        const std::vector<DBusPropertiesMap>& results,
        const ::DBus::Error& dberror, void* data) override;

    DISALLOW_COPY_AND_ASSIGN(Proxy);
  };

  Proxy proxy_;

  DISALLOW_COPY_AND_ASSIGN(ModemModem3gppProxy);
};

}  // namespace mm1
}  // namespace shill

#endif  // SHILL_CELLULAR_MM1_MODEM_MODEM3GPP_PROXY_H_
