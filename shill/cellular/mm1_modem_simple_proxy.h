// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_CELLULAR_MM1_MODEM_SIMPLE_PROXY_H_
#define SHILL_CELLULAR_MM1_MODEM_SIMPLE_PROXY_H_

#include <string>

#include "dbus_proxies/org.freedesktop.ModemManager1.Modem.Simple.h"
#include "shill/cellular/mm1_modem_simple_proxy_interface.h"
#include "shill/dbus_properties.h"

namespace shill {
namespace mm1 {

// A proxy to org.freedesktop.ModemManager1.Modem.Simple.
class ModemSimpleProxy : public ModemSimpleProxyInterface {
 public:
  // Constructs a org.freedesktop.ModemManager1.Modem.Simple DBus
  // object proxy at |path| owned by |service|.
  ModemSimpleProxy(DBus::Connection* connection,
                   const std::string& path,
                   const std::string& service);
  ~ModemSimpleProxy() override;

  // Inherited methods from SimpleProxyInterface.
  void Connect(
      const DBusPropertiesMap& properties,
      Error* error,
      const DBusPathCallback& callback,
      int timeout) override;
  void Disconnect(const ::DBus::Path& bearer,
                  Error* error,
                  const ResultCallback& callback,
                  int timeout) override;
  void GetStatus(Error* error,
                 const DBusPropertyMapCallback& callback,
                 int timeout) override;

 private:
  class Proxy : public org::freedesktop::ModemManager1::Modem::Simple_proxy,
                public DBus::ObjectProxy {
   public:
    Proxy(DBus::Connection* connection,
          const std::string& path,
          const std::string& service);
    ~Proxy() override;

   private:
    // Method callbacks inherited from
    // org::freedesktop::ModemManager1::Modem::SimpleProxy
    void ConnectCallback(const ::DBus::Path& bearer,
                         const ::DBus::Error& dberror,
                         void* data) override;
    void DisconnectCallback(const ::DBus::Error& dberror, void* data) override;
    void GetStatusCallback(
        const DBusPropertiesMap& bearer,
        const ::DBus::Error& dberror,
        void* data) override;

    DISALLOW_COPY_AND_ASSIGN(Proxy);
  };

  Proxy proxy_;

  DISALLOW_COPY_AND_ASSIGN(ModemSimpleProxy);
};

}  // namespace mm1
}  // namespace shill

#endif  // SHILL_CELLULAR_MM1_MODEM_SIMPLE_PROXY_H_
