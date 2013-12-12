// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_DHCP_CONFIG_
#define SHILL_DHCP_CONFIG_

#include <base/cancelable_callback.h>
#include <base/file_path.h>
#include <base/memory/scoped_ptr.h>
#include <base/memory/weak_ptr.h>
#include <dbus-c++/types.h>
#include <glib.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST

#include "shill/ipconfig.h"
#include "shill/minijail.h"

namespace shill {

class ControlInterface;
class DHCPProvider;
class DHCPProxyInterface;
class EventDispatcher;
class GLib;
class ProxyFactory;

// This class provides a DHCP client instance for the device |device_name|.
// If |request_hostname| is non-empty, it asks the DHCP server to register
// this hostname on our behalf, for purposes of administration or creating
// a dynamic DNS entry.
//
// The DHPCConfig instance asks the DHCP client to create a lease file
// containing the name |lease_file_suffix|.  If this suffix is the same as
// |device_name|, the lease is considered to be ephemeral, and the lease
// file is removed whenever this DHCPConfig instance is no longer needed.
// Otherwise, the lease file persists and will be re-used in future attempts.
class DHCPConfig : public IPConfig {
 public:
  typedef std::map<std::string, DBus::Variant> Configuration;

  static const int kMinMTU;

  DHCPConfig(ControlInterface *control_interface,
             EventDispatcher *dispatcher,
             DHCPProvider *provider,
             const std::string &device_name,
             const std::string &request_hostname,
             const std::string &lease_file_suffix,
             bool arp_gateway,
             bool is_minimal_config,
             GLib *glib);
  virtual ~DHCPConfig();

  // Inherited from IPConfig.
  virtual bool RequestIP();
  virtual bool RenewIP();
  virtual bool ReleaseIP(ReleaseReason reason);

  // If |proxy_| is not initialized already, sets it to a new D-Bus proxy to
  // |service|.
  void InitProxy(const std::string &service);

  // Processes an Event signal from dhcpcd.
  void ProcessEventSignal(const std::string &reason,
                          const Configuration &configuration);

 protected:
  // Overrides base clase implementation.
  virtual void UpdateProperties(const Properties &properties);
  virtual void NotifyFailure();

 private:
  friend class DHCPConfigTest;
  FRIEND_TEST(DHCPConfigCallbackTest, ProcessEventSignalFail);
  FRIEND_TEST(DHCPConfigCallbackTest, ProcessEventSignalGatewayArp);
  FRIEND_TEST(DHCPConfigCallbackTest, ProcessEventSignalGatewayArpNak);
  FRIEND_TEST(DHCPConfigCallbackTest, ProcessEventSignalSuccess);
  FRIEND_TEST(DHCPConfigCallbackTest, ProcessEventSignalUnknown);
  FRIEND_TEST(DHCPConfigCallbackTest, RequestIPTimeout);
  FRIEND_TEST(DHCPConfigCallbackTest, StartTimeout);
  FRIEND_TEST(DHCPConfigTest, GetIPv4AddressString);
  FRIEND_TEST(DHCPConfigTest, InitProxy);
  FRIEND_TEST(DHCPConfigTest, ParseClasslessStaticRoutes);
  FRIEND_TEST(DHCPConfigTest, ParseConfiguration);
  FRIEND_TEST(DHCPConfigTest, ReleaseIP);
  FRIEND_TEST(DHCPConfigTest, ReleaseIPArpGW);
  FRIEND_TEST(DHCPConfigTest, ReleaseIPStaticIPWithLease);
  FRIEND_TEST(DHCPConfigTest, ReleaseIPStaticIPWithoutLease);
  FRIEND_TEST(DHCPConfigTest, RenewIP);
  FRIEND_TEST(DHCPConfigTest, RequestIP);
  FRIEND_TEST(DHCPConfigTest, Restart);
  FRIEND_TEST(DHCPConfigTest, RestartNoClient);
  FRIEND_TEST(DHCPConfigTest, StartFail);
  FRIEND_TEST(DHCPConfigTest, StartWithHostname);
  FRIEND_TEST(DHCPConfigTest, StartWithMinimalConfig);
  FRIEND_TEST(DHCPConfigTest, StartWithoutArpGateway);
  FRIEND_TEST(DHCPConfigTest, StartWithoutHostname);
  FRIEND_TEST(DHCPConfigTest, StartWithoutLeaseSuffix);
  FRIEND_TEST(DHCPConfigTest, Stop);
  FRIEND_TEST(DHCPConfigTest, StopDuringRequestIP);
  FRIEND_TEST(DHCPProviderTest, CreateConfig);

  static const char kConfigurationKeyBroadcastAddress[];
  static const char kConfigurationKeyClasslessStaticRoutes[];
  static const char kConfigurationKeyDNS[];
  static const char kConfigurationKeyDomainName[];
  static const char kConfigurationKeyDomainSearch[];
  static const char kConfigurationKeyIPAddress[];
  static const char kConfigurationKeyMTU[];
  static const char kConfigurationKeyRouters[];
  static const char kConfigurationKeySubnetCIDR[];
  static const char kConfigurationKeyVendorEncapsulatedOptions[];
  static const char kConfigurationKeyWebProxyAutoDiscoveryUrl[];

  static const int kDHCPCDExitPollMilliseconds;
  static const int kDHCPCDExitWaitMilliseconds;
  static const char kDHCPCDPath[];
  static const char kDHCPCDPathFormatPID[];
  static const int kDHCPTimeoutSeconds;
  static const char kDHCPCDUser[];
  static const char kDHCPCDMinimalConfig[];

  static const char kReasonBound[];
  static const char kReasonFail[];
  static const char kReasonGatewayArp[];
  static const char kReasonNak[];
  static const char kReasonRebind[];
  static const char kReasonReboot[];
  static const char kReasonRenew[];

  static const char kType[];

  // Starts dhcpcd, returns true on success and false otherwise.
  bool Start();

  // Stops dhcpcd if running.
  void Stop(const char *reason);

  // Stops dhcpcd if already running and then starts it. Returns true on success
  // and false otherwise.
  bool Restart();

  // Parses |classless_routes| into |properties|.  Sets the default gateway
  // if one is supplied and |properties| does not already contain one.  It
  // also sets the "routes" parameter of the IPConfig properties for all
  // routes not converted into the default gateway.  Returns true on
  // success, and false otherwise.
  static bool ParseClasslessStaticRoutes(const std::string &classless_routes,
                                         IPConfig::Properties *properties);

  // Parses |configuration| into |properties|. Returns true on success, and
  // false otherwise.
  static bool ParseConfiguration(const Configuration &configuration,
                                 IPConfig::Properties *properties);

  // Returns the string representation of the IP address |address|, or an
  // empty string on failure.
  static std::string GetIPv4AddressString(unsigned int address);

  // Called when the dhcpcd client process exits.
  static void ChildWatchCallback(GPid pid, gint status, gpointer data);

  // Cleans up remaining state from a running client, if any, including freeing
  // its GPid, exit watch callback, and state files.
  void CleanupClientState();

  // Initialize a callback that will invoke ProcessDHCPTimeout if we
  // do not get a lease in a reasonable amount of time.
  void StartDHCPTimeout();
  // Cancel callback created by StartDHCPTimeout. One-liner included
  // for symmetry.
  void StopDHCPTimeout();
  // Called if we do not get a DHCP lease in a reasonable amount of time.
  // Informs upper layers of the failure.
  void ProcessDHCPTimeout();

  // Kills DHCP client process.
  void KillClient();

  // Store cached copies of singletons for speed/ease of testing.
  ProxyFactory *proxy_factory_;

  DHCPProvider *provider_;

  // Hostname to be used in the request.  This will be passed to the DHCP
  // server in the request.
  std::string request_hostname_;

  // DHCP lease file suffix, used to differentiate the lease of one interface
  // or network from another.
  std::string lease_file_suffix_;

  // Specifies whether to supply an argument to the DHCP client to validate
  // the acquired IP address using an ARP request to the gateway IP address.
  bool arp_gateway_;

  // Specifies whether to configure the the DHCP client to request the bare
  // minimum of options in order to escape any MTU issues with the packet size
  // of the DHCP server's reply.
  bool is_minimal_config_;

  // The PID of the spawned DHCP client. May be 0 if no client has been spawned
  // yet or the client has died.
  int pid_;

  // Child exit watch callback source tag.
  unsigned int child_watch_tag_;

  // Whether a lease has been acquired from the DHCP server or gateway ARP.
  bool is_lease_active_;

  // Whether it is valid to retain the lease acquired via gateway ARP.
  bool is_gateway_arp_active_;

  // The proxy for communicating with the DHCP client.
  scoped_ptr<DHCPProxyInterface> proxy_;

  // Called if we fail to get a DHCP lease in a timely manner.
  base::CancelableClosure lease_acquisition_timeout_callback_;

  // Time to wait for a DHCP lease. Represented as field so that it
  // can be overriden in tests.
  unsigned int lease_acquisition_timeout_seconds_;

  // Root file path, used for testing.
  base::FilePath root_;

  base::WeakPtrFactory<DHCPConfig> weak_ptr_factory_;
  EventDispatcher *dispatcher_;
  GLib *glib_;

  Minijail *minijail_;

  DISALLOW_COPY_AND_ASSIGN(DHCPConfig);
};

}  // namespace shill

#endif  // SHILL_DHCP_CONFIG_
