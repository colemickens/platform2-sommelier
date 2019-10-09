// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ARC_NETWORK_MULTICAST_FORWARDER_H_
#define ARC_NETWORK_MULTICAST_FORWARDER_H_

#include <netinet/ip.h>
#include <sys/socket.h>

#include <memory>
#include <string>

#include <base/macros.h>

#include "arc/network/multicast_socket.h"
#include "arc/network/net_util.h"

namespace arc_networkd {

constexpr uint32_t kMdnsMcastAddress = Ipv4Addr(224, 0, 0, 251);
constexpr uint16_t kMdnsPort = 5353;
constexpr uint32_t kSsdpMcastAddress = Ipv4Addr(239, 255, 255, 250);
constexpr uint16_t kSsdpPort = 1900;

// Listens on a well-known port and forwards multicast messages between
// network interfaces.  Handles mDNS, legacy mDNS, and SSDP messages.
class MulticastForwarder {
 public:
  MulticastForwarder() = default;
  virtual ~MulticastForwarder() = default;

  // Start forwarding multicast packets between the container's P2P link
  // |int_ifname| and the external LAN interface |lan_ifname|.  This
  // only forwards traffic on multicast address |mcast_addr| and UDP
  // port |port|.
  //
  // |mdns_ipaddr|, if != INADDR_ANY, will be used to rewrite mDNS A records
  // to use the IP address from |lan_ifname|.
  bool Start(const std::string& int_ifname,
             const std::string& lan_ifname,
             uint32_t mdns_ipaddr,
             uint32_t mcast_addr,
             uint16_t port);

  // Rewrite mDNS A records pointing to |guest_ip| so that they point to
  // the IPv4 |lan_ip| assigned to physical interface instead, so that Android
  // can advertise services to devices on the LAN.  This modifies |data|, an
  // incoming packet that is |bytes| long.
  // TODO(b/134717598) Support IPv6 translation.
  static void TranslateMdnsIp(const struct in_addr& lan_ip,
                              const struct in_addr& guest_ip,
                              char* data,
                              ssize_t len);

  void TranslateMdnsIp(const struct in_addr& lan_ip, char* data, ssize_t len);

  // SendTo sends |data| using a socket bound to |src_port| and |lan_ifname_|.
  // If |src_port| is equal to |port_|, we will use |lan_socket_|. Otherwise,
  // create a temporary socket.
  bool SendTo(uint16_t src_port,
              const void* data,
              ssize_t len,
              const struct sockaddr_in& dst);

  std::string int_ifname_;
  struct in_addr mdns_ip_;
  std::string lan_ifname_;

  struct in_addr mcast_addr_;
  unsigned int port_;

  std::unique_ptr<MulticastSocket> int_socket_;
  std::unique_ptr<MulticastSocket> lan_socket_;

 private:
  void OnFileCanReadWithoutBlocking(int fd);

  DISALLOW_COPY_AND_ASSIGN(MulticastForwarder);
};

}  // namespace arc_networkd

#endif  // ARC_NETWORK_MULTICAST_FORWARDER_H_
