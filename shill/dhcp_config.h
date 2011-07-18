// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_DHCP_CONFIG_
#define SHILL_DHCP_CONFIG_

#include <base/file_path.h>
#include <base/memory/scoped_ptr.h>
#include <dbus-c++/types.h>
#include <glib.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST

#include "shill/ipconfig.h"

namespace shill {

class DHCPProvider;
class DHCPProxyInterface;
class GLib;

class DHCPConfig : public IPConfig {
 public:
  typedef std::map<std::string, DBus::Variant> Configuration;

  DHCPConfig(DHCPProvider *provider,
             const std::string &device_name,
             GLib *glib);
  virtual ~DHCPConfig();

  // Inherited from IPConfig.
  virtual bool RequestIP();
  virtual bool RenewIP();
  virtual bool ReleaseIP();

  // If |proxy_| is not initialized already, sets it to a new D-Bus proxy to
  // |service|.
  void InitProxy(const char *service);

  // Processes an Event signal from dhcpcd.
  void ProcessEventSignal(const std::string &reason,
                          const Configuration &configuration);

 private:
  friend class DHCPConfigTest;
  FRIEND_TEST(DHCPConfigTest, GetIPv4AddressString);
  FRIEND_TEST(DHCPConfigTest, ParseConfiguration);
  FRIEND_TEST(DHCPConfigTest, ProcessEventSignalFail);
  FRIEND_TEST(DHCPConfigTest, ProcessEventSignalSuccess);
  FRIEND_TEST(DHCPConfigTest, ProcessEventSignalUnknown);
  FRIEND_TEST(DHCPConfigTest, ReleaseIP);
  FRIEND_TEST(DHCPConfigTest, RenewIP);
  FRIEND_TEST(DHCPConfigTest, RequestIP);
  FRIEND_TEST(DHCPConfigTest, Restart);
  FRIEND_TEST(DHCPConfigTest, RestartNoClient);
  FRIEND_TEST(DHCPConfigTest, StartFail);
  FRIEND_TEST(DHCPConfigTest, StartSuccess);
  FRIEND_TEST(DHCPConfigTest, Stop);
  FRIEND_TEST(DHCPProviderTest, CreateConfig);

  static const char kConfigurationKeyBroadcastAddress[];
  static const char kConfigurationKeyDNS[];
  static const char kConfigurationKeyDomainName[];
  static const char kConfigurationKeyDomainSearch[];
  static const char kConfigurationKeyIPAddress[];
  static const char kConfigurationKeyMTU[];
  static const char kConfigurationKeyRouters[];
  static const char kConfigurationKeySubnetCIDR[];

  static const char kDHCPCDPath[];
  static const char kDHCPCDPathFormatLease[];
  static const char kDHCPCDPathFormatPID[];

  static const char kReasonBound[];
  static const char kReasonFail[];
  static const char kReasonRebind[];
  static const char kReasonReboot[];
  static const char kReasonRenew[];

  // Starts dhcpcd, returns true on success and false otherwise.
  bool Start();

  // Stops dhcpcd if running.
  void Stop();

  // Stops dhcpcd if already running and then starts it. Returns true on success
  // and false otherwise.
  bool Restart();

  // Parses |configuration| into |properties|. Returns true on success, and
  // false otherwise.
  bool ParseConfiguration(const Configuration& configuration,
                          IPConfig::Properties *properties);

  // Returns the string representation of the IP address |address|, or an
  // empty string on failure.
  std::string GetIPv4AddressString(unsigned int address);

  // Called when the dhcpcd client process exits.
  static void ChildWatchCallback(GPid pid, gint status, gpointer data);

  // Cleans up remaining state from a running client, if any, including freeing
  // its GPid, exit watch callback, and state files.
  void CleanupClientState();

  DHCPProvider *provider_;

  // The PID of the spawned DHCP client. May be 0 if no client has been spawned
  // yet or the client has died.
  int pid_;

  // Child exit watch callback source tag.
  unsigned int child_watch_tag_;

  // The proxy for communicating with the DHCP client.
  scoped_ptr<DHCPProxyInterface> proxy_;

  // Root file path, used for testing.
  FilePath root_;

  GLib *glib_;

  DISALLOW_COPY_AND_ASSIGN(DHCPConfig);
};

}  // namespace shill

#endif  // SHILL_DHCP_CONFIG_
