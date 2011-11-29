// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MODEM_PROXY_
#define SHILL_MODEM_PROXY_

#include <string>

#include <base/basictypes.h>

#include "shill/dbus_bindings/modem.h"
#include "shill/modem_proxy_interface.h"

namespace shill {

// A proxy to ModemManager.Modem.
class ModemProxy : public ModemProxyInterface {
 public:
  // Constructs a ModemManager.Modem DBus object proxy at |path| owned by
  // |service|. Caught signals will be dispatched to |delegate|.
  ModemProxy(ModemProxyDelegate *delegate,
             DBus::Connection *connection,
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
    Proxy(ModemProxyDelegate *delegate,
          DBus::Connection *connection,
          const std::string &path,
          const std::string &service);
    virtual ~Proxy();

   private:
    // Signal callbacks inherited from ModemManager::Modem_proxy.
    virtual void StateChanged(const uint32 &old,
                              const uint32 &_new,
                              const uint32 &reason);

    ModemProxyDelegate *delegate_;

    DISALLOW_COPY_AND_ASSIGN(Proxy);
  };

  Proxy proxy_;

  DISALLOW_COPY_AND_ASSIGN(ModemProxy);
};

}  // namespace shill

#endif  // SHILL_MODEM_PROXY_
