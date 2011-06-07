// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_DHCP_CONFIG_
#define SHILL_DHCP_CONFIG_

#include <base/memory/scoped_ptr.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST
#include <dbus-c++/connection.h>

#include "shill/device.h"
#include "shill/ipconfig.h"

namespace shill {

class DHCPConfig;
class DHCPProvider;
class DHCPProxyInterface;

typedef scoped_refptr<DHCPConfig> DHCPConfigRefPtr;

class DHCPConfig : public IPConfig {
 public:
  typedef std::map<std::string, DBus::Variant> Configuration;

  static const char kConfigurationKeyBroadcastAddress[];
  static const char kConfigurationKeyDNS[];
  static const char kConfigurationKeyDomainName[];
  static const char kConfigurationKeyDomainSearch[];
  static const char kConfigurationKeyIPAddress[];
  static const char kConfigurationKeyMTU[];
  static const char kConfigurationKeyRouters[];
  static const char kConfigurationKeySubnetCIDR[];

  DHCPConfig(DHCPProvider *provider, DeviceConstRefPtr device);
  virtual ~DHCPConfig();

  // Inherited from IPConfig.
  virtual bool Request();
  virtual bool Renew();

  // If |proxy_| is not initialized already, sets it to a new D-Bus proxy to
  // |service|.
  void InitProxy(DBus::Connection *connection, const char *service);

  // Processes an Event signal from dhcpcd.
  void ProcessEventSignal(const std::string &reason,
                          const Configuration &configuration);

 private:
  FRIEND_TEST(DHCPConfigTest, GetIPv4AddressString);
  FRIEND_TEST(DHCPConfigTest, ParseConfiguration);

  static const char kDHCPCDPath[];

  // Starts dhcpcd, returns true on success and false otherwise.
  bool Start();

  // Parses |configuration| into |properties|. Returns true on success, and
  // false otherwise.
  bool ParseConfiguration(const Configuration& configuration,
                          IPConfig::Properties *properties);

  // Returns the string representation of the IP address |address|, or an
  // empty string on failure.
  std::string GetIPv4AddressString(unsigned int address);

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
