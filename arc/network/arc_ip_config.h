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

namespace arc_networkd {

std::ostream& operator<<(std::ostream& stream, const struct in_addr& addr);
std::ostream& operator<<(std::ostream& stream, const struct in6_addr& addr);

// Sets up IPv6 addresses, routing, and NDP proxying between the guest's
// interface |con_ifname| in network namespace |con_netns|, the internal
// host<->guest interface |int_ifname|, and the external LAN interface
// |current_lan_ifname_|.
class ArcIpConfig {
 public:
  ArcIpConfig(const std::string& int_ifname,
              const std::string& con_ifname,
              pid_t con_netns);
  virtual ~ArcIpConfig();

  // Open up the file descriptors needed to access the host and guest
  // namespaces.
  bool Init();

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

 protected:
  int GetTableIdForInterface(const std::string& ifname);
  int StartProcessInMinijail(const std::vector<std::string>& argv,
                             bool log_failures);

  std::string int_ifname_;
  std::string con_ifname_;
  pid_t con_netns_;

  base::ScopedFD con_netns_fd_;
  base::ScopedFD self_netns_fd_;
  int routing_table_id_;

  bool ipv6_configured_{false};
  bool inbound_configured_{false};
  std::string current_address_;
  std::string current_address_full_;
  std::string current_router_;
  std::string current_lan_ifname_;
};

}  // namespace arc_networkd

#endif  // ARC_NETWORK_ARC_IP_CONFIG_H_
