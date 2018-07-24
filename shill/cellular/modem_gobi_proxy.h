// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
