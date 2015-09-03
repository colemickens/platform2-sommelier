//
// Copyright (C) 2011 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#ifndef SHILL_DHCP_DHCPCD_PROXY_H_
#define SHILL_DHCP_DHCPCD_PROXY_H_

#include <map>
#include <string>

#include <base/macros.h>

#include "shill/dbus_proxies/dhcpcd.h"
#include "shill/dhcp/dhcp_config.h"
#include "shill/dhcp/dhcp_proxy_interface.h"
#include "shill/dhcp/dhcpcd_listener_interface.h"

namespace shill {

class DHCPProvider;

// The DHCPCD listener is a singleton proxy that listens to signals from all
// DHCP clients and dispatches them through the DHCP provider to the appropriate
// client based on the PID.
class DHCPCDListener : public DHCPCDListenerInterface {
 public:
  DHCPCDListener(DBus::Connection* connection, DHCPProvider* provider);

 private:
  class Proxy : public DBus::InterfaceProxy,
                public DBus::ObjectProxy {
   public:
    Proxy(DBus::Connection* connection, DHCPProvider* provider);
    ~Proxy() override;

   private:
    void EventSignal(const DBus::SignalMessage& signal);
    void StatusChangedSignal(const DBus::SignalMessage& signal);

    DHCPProvider* provider_;

    DISALLOW_COPY_AND_ASSIGN(Proxy);
  };

  Proxy proxy_;

  DISALLOW_COPY_AND_ASSIGN(DHCPCDListener);
};

// There's a single DHCPCD proxy per DHCP client identified by its process id
// and service name |service|.
class DHCPCDProxy : public DHCPProxyInterface {
 public:
  static const char kDBusInterfaceName[];
  static const char kDBusPath[];

  DHCPCDProxy(DBus::Connection* connection, const std::string& service);

  // Inherited from DHCPProxyInterface.
  virtual void Rebind(const std::string& interface);
  virtual void Release(const std::string& interface);

 private:
  class Proxy : public org::chromium::dhcpcd_proxy,
                public DBus::ObjectProxy {
   public:
    Proxy(DBus::Connection* connection, const std::string& service);
    ~Proxy() override;

   private:
    // Signal callbacks inherited from dhcpcd_proxy. Note that these callbacks
    // are unused because signals are dispatched directly to the DHCP
    // configuration instance by the signal listener.
    void Event(
        const uint32_t& pid,
        const std::string& reason,
        const std::map<std::string, DBus::Variant>& configuration) override;
    void StatusChanged(const uint32_t& pid, const std::string& status) override;

    DISALLOW_COPY_AND_ASSIGN(Proxy);
  };

  // Report a D-Bus method call failure.
  void LogDbusError(const DBus::Error& e, const std::string& method,
                    const std::string& interface);

  Proxy proxy_;

  DISALLOW_COPY_AND_ASSIGN(DHCPCDProxy);
};

}  // namespace shill

#endif  // SHILL_DHCP_DHCPCD_PROXY_H_
