// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_DHCP_CONFIG_
#define SHILL_DHCP_CONFIG_

#include <base/memory/scoped_ptr.h>
#include <dbus-c++/connection.h>

#include "shill/ipconfig.h"

namespace shill {

class DHCPConfig;
class DHCPProvider;
class DHCPProxyInterface;

typedef scoped_refptr<DHCPConfig> DHCPConfigRefPtr;

class DHCPConfig : public IPConfig {
 public:
  DHCPConfig(DHCPProvider *provider, const Device &device);
  virtual ~DHCPConfig();

  // Inherited from IPConfig.
  virtual bool Request();
  virtual bool Renew();

  // If |proxy_| is not initialized already, sets it to a new D-Bus proxy to
  // |service|.
  void InitProxy(DBus::Connection *connection, const char *service);

 private:
  static const char kDHCPCDPath[];

  // Starts dhcpcd, returns true on success and false otherwise.
  bool Start();

  DHCPProvider *provider_;

  // The PID of the spawned DHCP client. May be 0 if no client has been spawned
  // yet or the client has died.
  unsigned int pid_;

  // The proxy for communicating with the DHCP client.
  scoped_ptr<DHCPProxyInterface> proxy_;

  DISALLOW_COPY_AND_ASSIGN(DHCPConfig);
};

}  // namespace shill

#endif  // SHILL_DHCP_CONFIG_
