// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MODEM_PROXY_
#define SHILL_MODEM_PROXY_

#include <base/basictypes.h>

#include "shill/dbus_bindings/modem.h"
#include "shill/modem_proxy_interface.h"

namespace shill {

// There's a single proxy per ModemManager.Modem service identified by its DBus
// |path| and owner name |service|.
class ModemProxy : public ModemProxyInterface {
 public:
  ModemProxy(DBus::Connection *connection,
             const std::string &path,
             const std::string &service);
  virtual ~ModemProxy();

  // Inherited from ModemProxyInterface.
  virtual void Enable(const bool enable);
  virtual Info GetInfo();

 private:
  class Proxy : public org::freedesktop::ModemManager::Modem_proxy,
                public DBus::ObjectProxy {
   public:
    Proxy(DBus::Connection *connection,
          const std::string &path,
          const std::string &service);
    virtual ~Proxy();

   private:
    // Signal callbacks inherited from ModemManager::Modem_proxy.
    virtual void StateChanged(const uint32 &old_state,
                              const uint32 &new_state,
                              const uint32 &reason);

    DISALLOW_COPY_AND_ASSIGN(Proxy);
  };

  Proxy proxy_;

  DISALLOW_COPY_AND_ASSIGN(ModemProxy);
};

}  // namespace shill

#endif  // SHILL_MODEM_PROXY_
