// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ARC_NETWORK_NDPROXY_H_
#define ARC_NETWORK_NDPROXY_H_

#include <stdint.h>

#include <netinet/icmp6.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>

#include <map>
#include <set>
#include <string>

#include <base/files/scoped_file.h>
#include <base/macros.h>

namespace arc_networkd {

// Forward ICMPv6 RS/RA/NS/NA mssages between network interfaces according to
// RFC 4389. Support asymmetric proxy that RS will be proxied one-way from
// guest interface to physical interface ('Outbound') and RA the other way back
// ('Inbound'), as well as symmetric proxy among guest interfaces that only
// NS/NA will be proxied. Sample Usage:
//   arc_networkd::NDProxy nd_proxy;
//   nd_proxy.AddRouterInterfacePair("eth0", "arc_eth0");
//   nd_proxy.Start();
class NDProxy {
 public:
  NDProxy() = default;
  ~NDProxy() = default;

  static const ssize_t kTranslateErrorNotICMPv6Frame;
  static const ssize_t kTranslateErrorNotNDFrame;
  static const ssize_t kTranslateErrorInsufficientLength;

  // To proxy between upstream interface and guest OS interface (eth0-arc_eth0)
  // Outbound RS, inbound RA, and bidirectional NS/NA will be proxied.
  bool AddRouterInterfacePair(const std::string& ifname_physical,
                              const std::string& ifname_guest);

  // To proxy between two different guest OS interface (arc_eth0-vmtap0)
  // Only NS/NA will be proxied (bidirectionally).
  bool AddPeeringInterfacePair(const std::string& ifname1,
                               const std::string& ifname2);

  // Remove all proxy interface pair with ifindex.
  bool RemoveInterface(const std::string& ifname);

  // Start proxying. This is a blocking call.
  void Start();

  static uint16_t Icmpv6Checksum(const ip6_hdr* ip6, const icmp6_hdr* icmp6);
  static void ReplaceMacInIcmpOption(uint8_t* frame,
                                     ssize_t frame_len,
                                     size_t nd_hdr_len,
                                     uint8_t opt_type,
                                     const uint8_t* target_mac);
  static ssize_t TranslateNDFrame(const uint8_t* in_frame,
                                  ssize_t frame_len,
                                  const uint8_t* local_mac_addr,
                                  uint8_t* out_frame);

 private:
  // Data structure to store interface mapping for a certain kind of packet to
  // be proxied. For example, {1: {2}, 2: {1}} means that packet from interfaces
  // 1 and 2 will be proxied to each other.
  using interface_mapping = std::map<int, std::set<int>>;

  interface_mapping* MapForType(uint8_t type);
  bool AddInterfacePairInternal(const std::string& ifname1,
                                const std::string& ifname2,
                                bool proxy_rs_ra);

  void ProxyNDFrame(int target_if, ssize_t frame_len);

  base::ScopedFD fd_;
  uint8_t in_frame_buffer_[IP_MAXPACKET];
  uint8_t out_frame_buffer_[IP_MAXPACKET];

  interface_mapping if_map_rs_;
  interface_mapping if_map_ra_;
  interface_mapping if_map_ns_na_;

  DISALLOW_COPY_AND_ASSIGN(NDProxy);
};

}  // namespace arc_networkd

#endif  // ARC_NETWORK_NDPROXY_H_
