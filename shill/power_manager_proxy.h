// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_POWER_MANAGER_PROXY_
#define SHILL_POWER_MANAGER_PROXY_

#include "shill/dbus_bindings/power_manager.h"
#include "shill/power_manager_proxy_interface.h"

namespace shill {

class PowerManagerProxy : public PowerManagerProxyInterface {
 public:
  // Constructs a PowerManager DBus object proxy with signals dispatched to
  // |delegate|.
  PowerManagerProxy(PowerManagerProxyDelegate *delegate,
                    DBus::Connection *connection);
  virtual ~PowerManagerProxy();

  // Inherited from PowerManagerProxyInterface.
  virtual void RegisterSuspendDelay(uint32 delay_ms);

 private:
  class Proxy : public org::chromium::PowerManager_proxy,
                public DBus::ObjectProxy {
   public:
    Proxy(PowerManagerProxyDelegate *delegate,
          DBus::Connection *connection);
    virtual ~Proxy();

   private:
    // Signal callbacks inherited from org::chromium::PowerManager_proxy.
    virtual void SuspendDelay(const uint32_t &sequence_number);

    PowerManagerProxyDelegate *delegate_;

    DISALLOW_COPY_AND_ASSIGN(Proxy);
  };

  Proxy proxy_;

  DISALLOW_COPY_AND_ASSIGN(PowerManagerProxy);
};

}  // namespace shill

#endif  // SHILL_POWER_MANAGER_PROXY_
