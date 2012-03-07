// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MM1_MODEM_MODEM3GPP_PROXY_
#define SHILL_MM1_MODEM_MODEM3GPP_PROXY_

#include <string>

#include "shill/dbus_bindings/mm1-modem-modem3gpp.h"
#include "shill/dbus_properties.h"
#include "shill/mm1_modem_modem3gpp_proxy_interface.h"

namespace shill {
namespace mm1 {

class ModemModem3gppProxy : public ModemModem3gppProxyInterface {
 public:
  // Constructs a org.freedesktop.ModemManager1.Modem.Modem3gpp DBus
  // object proxy at |path| owned by |service|.
  ModemModem3gppProxy(DBus::Connection *connection,
                      const std::string &path,
                      const std::string &service);
  virtual ~ModemModem3gppProxy();
  // Inherited methods from ModemModem3gppProxyInterface.
  virtual void Register(const std::string &operator_id,
                        Error *error,
                        const ResultCallback &callback,
                        int timeout);
  virtual void Scan(Error *error,
                    const DBusPropertyMapsCallback &callback,
                    int timeout);

  // Inherited properties from Modem3gppProxyInterface.
  virtual std::string Imei();
  virtual uint32_t RegistrationState();
  virtual std::string OperatorCode();
  virtual std::string OperatorName();
  virtual uint32_t EnabledFacilityLocks();

 private:
  class Proxy : public org::freedesktop::ModemManager1::Modem::Modem3gpp_proxy,
                public DBus::ObjectProxy {
   public:
    Proxy(DBus::Connection *connection,
          const std::string &path,
          const std::string &service);
    virtual ~Proxy();

   private:
    // Method callbacks inherited from
    // org::freedesktop::ModemManager1::Modem::Modem3gppProxy
    virtual void RegisterCallback(const ::DBus::Error& dberror, void *data);
    virtual void ScanCallback(
        const std::vector<DBusPropertiesMap> &results,
        const ::DBus::Error& dberror, void *data);

    DISALLOW_COPY_AND_ASSIGN(Proxy);
  };

  Proxy proxy_;

  DISALLOW_COPY_AND_ASSIGN(ModemModem3gppProxy);
};

}  // namespace mm1
}  // namespace shill

#endif  // MM1_SHILL_MODEM_MODEM3GPP_PROXY_
