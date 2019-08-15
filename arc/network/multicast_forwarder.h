// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ARC_NETWORK_MULTICAST_FORWARDER_H_
#define ARC_NETWORK_MULTICAST_FORWARDER_H_

#include <netinet/ip.h>
#include <sys/socket.h>
#include <time.h>

#include <deque>
#include <memory>
#include <string>

#include <base/macros.h>
#include <base/memory/weak_ptr.h>

#include "arc/network/multicast_socket.h"

namespace arc_networkd {

// Listens on a well-known port and forwards multicast messages between
// network interfaces.  Handles stateless mDNS messages (src port and
// dst port are both 5353) and stateful mDNS/SSDP messages (src port
// is random, so the forwarder needs to keep a table of open sessions).
class MulticastForwarder {
 public:
  MulticastForwarder() {}
  virtual ~MulticastForwarder() {}

  // Start forwarding multicast packets between the container's P2P link
  // |int_ifname| and the external LAN interface |lan_ifname|.  This
  // only forwards traffic on multicast address |mcast_addr| and UDP
  // port |port|.  If |allow_stateless| is true, packets with
  // src_port == dst_port == |port| are always passed to the other
  // interface without creating a state table entry.  If it is false,
  // sessions must be initiated from |int_ifname| and will always
  // create a state table entry; "unsolicited" traffic from
  // |lan_ifname| will be silently discarded.
  //
  // |mdns_ipaddr|, if != INADDR_ANY, will be used to rewrite mDNS A records
  // to use the IP address from |lan_ifname|.
  bool Start(const std::string& int_ifname,
             const std::string& lan_ifname,
             uint32_t mdns_ipaddr,
             uint32_t mcast_addr,
             unsigned short port,
             bool allow_stateless);

 protected:
  // Rewrite mDNS A records pointing to |arc_ip_| so that they point to
  // the IPv4 assigned to physical interface instead, so that Android can
  // advertise services to devices on the LAN.  This modifies |data|, an
  // incoming packet that is |bytes| long.
  // TODO(b/134717598) Support IPv6 translation.
  void TranslateMdnsIp(const struct in_addr& lan_ip, char* data, ssize_t bytes);

  void CleanupTask();

  std::string int_ifname_;
  struct in_addr mdns_ip_;
  std::string lan_ifname_;

  struct in_addr mcast_addr_;
  unsigned int port_;
  bool allow_stateless_;

  std::unique_ptr<MulticastSocket> int_socket_;
  std::unique_ptr<MulticastSocket> lan_socket_;
  std::deque<std::unique_ptr<MulticastSocket>> temp_sockets_;

  base::WeakPtrFactory<MulticastForwarder> weak_factory_{this};

 private:
  void OnFileCanReadWithoutBlocking(int fd);

  DISALLOW_COPY_AND_ASSIGN(MulticastForwarder);
};

}  // namespace arc_networkd

#endif  // ARC_NETWORK_MULTICAST_FORWARDER_H_
