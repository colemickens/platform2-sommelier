// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_POWER_MANAGER_PROXY_
#define SHILL_POWER_MANAGER_PROXY_

// An implementation of PowerManagerProxyInterface.  It connects to the dbus and
// listens for events from the power manager.  When they occur, the delegate's
// member functions are called.

// Do not instantiate this class directly.  use
// ProxyFactory::CreatePowerManagerProxy instead.

#include "shill/dbus_bindings/power_manager.h"
#include "shill/power_manager_proxy_interface.h"
#include "shill/proxy_factory.h"

namespace shill {

class PowerManagerProxy : public PowerManagerProxyInterface {
 public:
  virtual ~PowerManagerProxy();

  // Inherited from PowerManagerProxyInterface.
  virtual void RegisterSuspendDelay(uint32 delay_ms);

 private:
  // Only this factory method can create a PowerManagerProxy.
  friend PowerManagerProxyInterface *ProxyFactory::CreatePowerManagerProxy(
      PowerManagerProxyDelegate *delegate);

  class Proxy : public org::chromium::PowerManager_proxy,
                public DBus::ObjectProxy {
   public:
    Proxy(PowerManagerProxyDelegate *delegate,
          DBus::Connection *connection);
    virtual ~Proxy();

   private:
    // Signal callbacks inherited from org::chromium::PowerManager_proxy.
    virtual void SuspendDelay(const uint32_t &sequence_number);
    virtual void PowerStateChanged(const std::string &new_power_state);

    PowerManagerProxyDelegate *const delegate_;

    DISALLOW_COPY_AND_ASSIGN(Proxy);
  };

  // The PowerStateChanged event occurs on this path.  The root path ("/") is
  // used because the powerd_suspend script sends signals on that path.  When
  // the powerd_suspend script is fixed to use the same path as the rest of the
  // power manager, then this proxy can use the usual power manager path
  // power_manager::kPowerManagerServicePath.
  static const char kRootPath[];

  // Constructs a PowerManager DBus object proxy with signals dispatched to
  // |delegate|.
  PowerManagerProxy(PowerManagerProxyDelegate *delegate,
                    DBus::Connection *connection);

  Proxy proxy_;

  DISALLOW_COPY_AND_ASSIGN(PowerManagerProxy);
};

}  // namespace shill

#endif  // SHILL_POWER_MANAGER_PROXY_
