// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_DHCPCD_PROXY_
#define SHILL_DHCPCD_PROXY_

#include <base/basictypes.h>

#include "shill/dhcp_config.h"
#include "shill/dhcp_proxy_interface.h"
#include "shill/dhcpcd.h"

namespace shill {

class DHCPProvider;

// TODO(petkov): Convert this to match ModemManagerProxy model.

// The DHCPCD listener is a singleton proxy that listens to signals from all
// DHCP clients and dispatches them through the DHCP provider to the appropriate
// client based on the PID.
class DHCPCDListener : public DHCPListenerInterface,
                       public DBus::InterfaceProxy,
                       public DBus::ObjectProxy {
 public:
  explicit DHCPCDListener(DHCPProvider *provider,
                          DBus::Connection *connection);

 private:
  void EventSignal(const DBus::SignalMessage &signal);
  void StatusChangedSignal(const DBus::SignalMessage &signal);

  DHCPProvider *provider_;

  DISALLOW_COPY_AND_ASSIGN(DHCPCDListener);
};

// There's a single DHCPCD proxy per DHCP client identified by its process id
// and service name.
class DHCPCDProxy : public DHCPProxyInterface,
                    public org::chromium::dhcpcd_proxy,
                    public DBus::ObjectProxy {
 public:
  static const char kDBusInterfaceName[];
  static const char kDBusPath[];

  DHCPCDProxy(DBus::Connection *connection, const char *service);

  // Inherited from DHCPProxyInterface.
  virtual void DoRebind(const std::string &interface);
  virtual void DoRelease(const std::string &interface);

  // Signal callbacks inherited from dhcpcd_proxy. Note that these callbacks are
  // unused because signals are dispatched directly to the DHCP configuration
  // instance by the signal listener.
  virtual void Event(
      const uint32_t &pid,
      const std::string &reason,
      const DHCPConfig::Configuration &configuration);
  virtual void StatusChanged(const uint32_t &pid, const std::string &status);

 private:
  DISALLOW_COPY_AND_ASSIGN(DHCPCDProxy);
};

}  // namespace shill

#endif  // SHILL_DHCPCD_PROXY_
