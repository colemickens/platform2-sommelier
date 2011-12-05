// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MODEM_SIMPLE_PROXY_
#define SHILL_MODEM_SIMPLE_PROXY_

#include <string>

#include <base/basictypes.h>

#include "shill/dbus_bindings/modem-simple.h"
#include "shill/modem_simple_proxy_interface.h"

namespace shill {

// A proxy to ModemManager.Modem.Simple.
class ModemSimpleProxy : public ModemSimpleProxyInterface {
 public:
  ModemSimpleProxy(ModemSimpleProxyDelegate *delegate,
                   DBus::Connection *connection,
                   const std::string &path,
                   const std::string &service);
  virtual ~ModemSimpleProxy();

  // Inherited from ModemSimpleProxyInterface.
  virtual void GetModemStatus(AsyncCallHandler *call_handler, int timeout);
  virtual void Connect(const DBusPropertiesMap &properties,
                       AsyncCallHandler *call_handler, int timeout);

 private:
  class Proxy : public org::freedesktop::ModemManager::Modem::Simple_proxy,
                public DBus::ObjectProxy {
   public:
    Proxy(ModemSimpleProxyDelegate *delegate,
          DBus::Connection *connection,
          const std::string &path,
          const std::string &service);
    virtual ~Proxy();

   private:
    // Signal callbacks inherited from ModemManager::Modem::Simple_proxy.
    // [None]

    // Method callbacks inherited from ModemManager::Modem::Simple_proxy.
    virtual void GetStatusCallback(const DBusPropertiesMap &props,
                                   const DBus::Error &dberror, void *data);
    virtual void ConnectCallback(const DBus::Error &dberror, void *data);

    ModemSimpleProxyDelegate *delegate_;

    DISALLOW_COPY_AND_ASSIGN(Proxy);
  };

  Proxy proxy_;

  DISALLOW_COPY_AND_ASSIGN(ModemSimpleProxy);
};

}  // namespace shill

#endif  // SHILL_MODEM_SIMPLE_PROXY_
