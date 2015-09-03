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

#ifndef SHILL_CELLULAR_MODEM_GOBI_PROXY_H_
#define SHILL_CELLULAR_MODEM_GOBI_PROXY_H_

#include <string>

#include <base/macros.h>

#include "shill/cellular/modem_gobi_proxy_interface.h"
#include "shill/dbus_proxies/modem-gobi.h"

namespace shill {

// A proxy to (old) ModemManager.Modem.Gobi.
class ModemGobiProxy : public ModemGobiProxyInterface {
 public:
  // Constructs a ModemManager.Modem.Gobi DBus object proxy at |path| owned by
  // |service|.
  ModemGobiProxy(DBus::Connection* connection,
                 const std::string& path,
                 const std::string& service);
  ~ModemGobiProxy() override;

  // Inherited from ModemGobiProxyInterface.
  void SetCarrier(const std::string& carrier,
                  Error* error, const ResultCallback& callback,
                  int timeout) override;

 private:
  class Proxy
      : public org::chromium::ModemManager::Modem::Gobi_proxy,
        public DBus::ObjectProxy {
   public:
    Proxy(DBus::Connection* connection,
          const std::string& path,
          const std::string& service);
    ~Proxy() override;

   private:
    // Signal callbacks inherited from ModemManager::Modem::Gobi_proxy.
    // [None]

    // Method callbacks inherited from ModemManager::Modem::Gobi_proxy.
    void SetCarrierCallback(const DBus::Error& dberror, void* data) override;

    DISALLOW_COPY_AND_ASSIGN(Proxy);
  };

  Proxy proxy_;

  DISALLOW_COPY_AND_ASSIGN(ModemGobiProxy);
};

}  // namespace shill

#endif  // SHILL_CELLULAR_MODEM_GOBI_PROXY_H_
