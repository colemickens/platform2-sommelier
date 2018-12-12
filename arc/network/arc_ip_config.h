// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ARC_NETWORK_ARC_IP_CONFIG_H_
#define ARC_NETWORK_ARC_IP_CONFIG_H_

#include <netinet/in.h>
#include <sys/socket.h>

#include <memory>
#include <ostream>
#include <string>
#include <vector>

#include <base/files/scoped_file.h>
#include <base/macros.h>

#include "arc/network/device.h"
#include "arc/network/minijailed_process_runner.h"

namespace arc_networkd {

std::ostream& operator<<(std::ostream& stream, const struct in_addr& addr);
std::ostream& operator<<(std::ostream& stream, const struct in6_addr& addr);

// Responsible for all configuration of a network device including the setup for
// the bridge and virtual interfaces, IPVv6 addresses, routing and NDP proxying
// and the iptables rules for enabling/disabling traffic into the container.
class ArcIpConfig {
 public:
  ArcIpConfig(const std::string& ifname, const DeviceConfig& config);
  ArcIpConfig(const std::string& ifname,
              const DeviceConfig& config,
              std::unique_ptr<MinijailedProcessRunner> process_runner);
  virtual ~ArcIpConfig();

  // Open up the file descriptors needed to access the host and guest
  // namespaces, sets up the virtual interfaces and pushes one side into
  // the container. |con_netns| is the container's network namespace;
  // if 0, this configuration will be reset and uninitialized.
  bool Init(pid_t con_netns);

  // Determine whether |con_ifname_| is up.  If so, read the rt_tables
  // file written to the Android filesystem by netd.  If not, return
  // false.
  bool ContainerInit();

  // Set or Clear the IPv6 configuration/routes/rules for the containerized OS.
  bool Set(const struct in6_addr& address,
           int prefix_len,
           const struct in6_addr& router_addr,
           const std::string& lan_ifname);
  bool Clear();

  // Enable or Disable inbound connections on |lan_ifname| by manipulating
  // a DNAT rule matching unclaimed traffic on the interface.
  void EnableInbound(const std::string& lan_ifname);
  void DisableInbound();

  friend std::ostream& operator<<(std::ostream& stream,
                                  const ArcIpConfig& conf);

  // Utility functions.
  static bool GetV6Address(const std::string& ifname, struct in6_addr* address);
  static void GenerateRandom(struct in6_addr* prefix, int prefix_len);

 private:
  // Initial host bridge and iptables configuration.
  void Setup();
  void Teardown();

  int GetTableIdForInterface(const std::string& ifname);

  const std::string ifname_;
  const DeviceConfig config_;

  pid_t con_netns_;
  int routing_table_id_;

  base::ScopedFD con_netns_fd_;
  base::ScopedFD self_netns_fd_;

  bool ipv6_configured_{false};
  bool inbound_configured_{false};
  // These track the current ipv6 configuration, if any.
  std::string ipv6_dev_ifname_;
  std::string ipv6_address_;
  std::string ipv6_address_full_;
  std::string ipv6_router_;

  std::unique_ptr<MinijailedProcessRunner> process_runner_;
};

}  // namespace arc_networkd

#endif  // ARC_NETWORK_ARC_IP_CONFIG_H_
