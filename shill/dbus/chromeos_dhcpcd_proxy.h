// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_DBUS_CHROMEOS_DHCPCD_PROXY_H_
#define SHILL_DBUS_CHROMEOS_DHCPCD_PROXY_H_

#include <string>

#include <base/macros.h>

#include "dhcpcd/dbus-proxies.h"
#include "shill/dhcp/dhcp_proxy_interface.h"

namespace shill {

// There's a single DHCPCD proxy per DHCP client identified by its process id.
class ChromeosDHCPCDProxy : public DHCPProxyInterface {
 public:
  ChromeosDHCPCDProxy(const scoped_refptr<dbus::Bus>& bus,
                      const std::string& service_name);
  ~ChromeosDHCPCDProxy() override;

  // Inherited from DHCPProxyInterface.
  void Rebind(const std::string& interface) override;
  void Release(const std::string& interface) override;

 private:
  void LogDBusError(const chromeos::ErrorPtr& error,
                    const std::string& method,
                    const std::string& interface);

  std::unique_ptr<org::chromium::dhcpcdProxy> dhcpcd_proxy_;

  DISALLOW_COPY_AND_ASSIGN(ChromeosDHCPCDProxy);
};

}  // namespace shill

#endif  // SHILL_DBUS_CHROMEOS_DHCPCD_PROXY_H_
