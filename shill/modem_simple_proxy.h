// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MODEM_SIMPLE_PROXY_
#define SHILL_MODEM_SIMPLE_PROXY_

#include <base/basictypes.h>

#include "shill/dbus_bindings/modem-simple.h"
#include "shill/modem_simple_proxy_interface.h"

namespace shill {

// A proxy to ModemManager.Modem.Simple.
class ModemSimpleProxy : public ModemSimpleProxyInterface {
 public:
  ModemSimpleProxy(DBus::Connection *connection,
                   const std::string &path,
                   const std::string &service);
  virtual ~ModemSimpleProxy();

  // Inherited from ModemSimpleProxyInterface.
  virtual DBusPropertiesMap GetStatus();
  virtual void Connect(const DBusPropertiesMap &properties);

 private:
  class Proxy : public org::freedesktop::ModemManager::Modem::Simple_proxy,
                public DBus::ObjectProxy {
   public:
    Proxy(DBus::Connection *connection,
          const std::string &path,
          const std::string &service);
    virtual ~Proxy();

   private:
    DISALLOW_COPY_AND_ASSIGN(Proxy);
  };

  Proxy proxy_;

  DISALLOW_COPY_AND_ASSIGN(ModemSimpleProxy);
};

}  // namespace shill

#endif  // SHILL_MODEM_SIMPLE_PROXY_
