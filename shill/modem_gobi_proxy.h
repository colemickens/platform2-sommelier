// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MODEM_GOBI_PROXY_H_
#define SHILL_MODEM_GOBI_PROXY_H_

#include <base/basictypes.h>

#include "shill/dbus_bindings/modem-gobi.h"
#include "shill/modem_gobi_proxy_interface.h"

namespace shill {

class ModemGobiProxy : public ModemGobiProxyInterface {
 public:
  // Constructs a ModemManager.Modem.Gobi DBus object proxy at |path| owned by
  // |service|.
  ModemGobiProxy(DBus::Connection *connection,
                 const std::string &path,
                 const std::string &service);
  virtual ~ModemGobiProxy();

  // Inherited from ModemGobiProxyInterface.
  virtual void SetCarrier(const std::string &carrier,
                          Error *error, const ResultCallback &callback,
                          int timeout);

 private:
  class Proxy
      : public org::chromium::ModemManager::Modem::Gobi_proxy,
        public DBus::ObjectProxy {
   public:
    Proxy(DBus::Connection *connection,
          const std::string &path,
          const std::string &service);
    virtual ~Proxy();

   private:
    // Signal callbacks inherited from ModemManager::Modem::Gobi_proxy.
    // [None]

    // Method callbacks inherited from ModemManager::Modem::Gobi_proxy.
    virtual void SetCarrierCallback(const DBus::Error &dberror, void *data);

    DISALLOW_COPY_AND_ASSIGN(Proxy);
  };

  Proxy proxy_;

  DISALLOW_COPY_AND_ASSIGN(ModemGobiProxy);
};

}  // namespace shill

#endif  // SHILL_MODEM_GOBI_PROXY_H_
