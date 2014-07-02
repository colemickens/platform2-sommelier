// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MM1_MODEM_LOCATION_PROXY_H_
#define SHILL_MM1_MODEM_LOCATION_PROXY_H_

#include <string>

#include "dbus_proxies/org.freedesktop.ModemManager1.Modem.Location.h"
#include "shill/dbus_properties.h"
#include "shill/mm1_modem_location_proxy_interface.h"

namespace shill {
namespace mm1 {

// A proxy to org.freedesktop.ModemManager1.Bearer.
class ModemLocationProxy : public ModemLocationProxyInterface {
 public:
  // Constructs an org.freedesktop.ModemManager1.Modem.Location DBus
  // object proxy at |path| owned by |service|.
  ModemLocationProxy(DBus::Connection *connection,
                     const std::string &path,
                     const std::string &service);

  virtual ~ModemLocationProxy();

  // Inherited methods from ModemLocationProxyInterface.
  virtual void Setup(uint32_t sources,
                     bool signal_location,
                     Error *error,
                     const ResultCallback &callback,
                     int timeout);

  virtual void GetLocation(Error *error,
                           const DBusEnumValueMapCallback &callback,
                           int timeout);

 private:
  class Proxy : public org::freedesktop::ModemManager1::Modem::Location_proxy,
                public DBus::ObjectProxy {
   public:
    Proxy(DBus::Connection *connection,
          const std::string &path,
          const std::string &service);
    virtual ~Proxy();

   private:
    // Method callbacks inherited from
    // org::freedesktop::ModemManager1::Modem:Location_proxy
    virtual void SetupCallback(const ::DBus::Error &dberror,
                               void *data);

    virtual void GetLocationCallback(const DBusEnumValueMap &location,
                                     const ::DBus::Error &dberror,
                                     void *data);

    DISALLOW_COPY_AND_ASSIGN(Proxy);
  };

  Proxy proxy_;

  DISALLOW_COPY_AND_ASSIGN(ModemLocationProxy);
};

}  // namespace mm1
}  // namespace shill

#endif  // SHILL_MM1_MODEM_LOCATION_PROXY_H_
