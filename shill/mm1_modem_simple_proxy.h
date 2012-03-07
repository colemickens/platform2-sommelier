// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MM1_MODEM_SIMPLE_PROXY_
#define SHILL_MM1_MODEM_SIMPLE_PROXY_

#include <string>

#include "shill/dbus_bindings/mm1-modem-simple.h"
#include "shill/dbus_properties.h"
#include "shill/mm1_modem_simple_proxy_interface.h"

namespace shill {
namespace mm1 {

class ModemSimpleProxy : public ModemSimpleProxyInterface {
 public:
  // Constructs a org.freedesktop.ModemManager1.Modem.Simple DBus
  // object proxy at |path| owned by |service|.
  ModemSimpleProxy(DBus::Connection *connection,
                   const std::string &path,
                   const std::string &service);
  virtual ~ModemSimpleProxy();

  // Inherited methods from SimpleProxyInterface.
  virtual void Connect(
      const DBusPropertiesMap &properties,
      Error *error,
      const DBusPathCallback &callback,
      int timeout);
  virtual void Disconnect(const ::DBus::Path &bearer,
                          Error *error,
                          const ResultCallback &callback,
                          int timeout);
  virtual void GetStatus(Error *error,
                         const DBusPropertyMapCallback &callback,
                         int timeout);

 private:
  class Proxy : public org::freedesktop::ModemManager1::Modem::Simple_proxy,
                public DBus::ObjectProxy {
   public:
    Proxy(DBus::Connection *connection,
          const std::string &path,
          const std::string &service);
    virtual ~Proxy();

   private:
    // Method callbacks inherited from
    // org::freedesktop::ModemManager1::Modem::SimpleProxy
    virtual void ConnectCallback(const ::DBus::Path &bearer,
                                 const ::DBus::Error &dberror,
                                 void *data);
    virtual void DisconnectCallback(const ::DBus::Error &dberror, void *data);
    virtual void GetStatusCallback(
        const DBusPropertiesMap &bearer,
        const ::DBus::Error &dberror,
        void *data);

    DISALLOW_COPY_AND_ASSIGN(Proxy);
  };

  Proxy proxy_;

  DISALLOW_COPY_AND_ASSIGN(ModemSimpleProxy);
};

}  // namespace mm1
}  // namespace shill

#endif  // SHILL_MM1_MODEM_SIMPLE_PROXY_
