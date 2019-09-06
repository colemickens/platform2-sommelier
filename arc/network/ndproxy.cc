// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arc/network/ndproxy.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <linux/if_packet.h>
#include <linux/in6.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include <string>

namespace arc_networkd {
namespace {
const unsigned char ether_addr_broadcast[] = {0xff, 0xff, 0xff,
                                              0xff, 0xff, 0xff};
}  // namespace

const ssize_t NDProxy::kTranslateErrorNotICMPv6Frame = -1;
const ssize_t NDProxy::kTranslateErrorNotNDFrame = -2;
const ssize_t NDProxy::kTranslateErrorInsufficientLength = -3;

// RFC 1071 and RFC 8200 Section 8.1
// We are doing calculation directly in network order. Note this algorithm works
// regardless of the endianess of the host.
uint16_t NDProxy::Icmpv6Checksum(const ip6_hdr* ip6, const icmp6_hdr* icmp6) {
  uint32_t sum = 0;
  // Src and Dst IP
  for (size_t i = 0; i < (sizeof(struct in6_addr) >> 1); ++i)
    sum += ip6->ip6_src.s6_addr16[i];
  for (size_t i = 0; i < (sizeof(struct in6_addr) >> 1); ++i)
    sum += ip6->ip6_dst.s6_addr16[i];

  // Upper-Layer Packet Length
  sum += ip6->ip6_plen;
  // Next Header
  sum += IPPROTO_ICMPV6 << 8;

  // ICMP
  const uint16_t* word = reinterpret_cast<const uint16_t*>(icmp6);
  uint16_t len;
  for (len = ntohs(ip6->ip6_plen); len > 1; len -= 2)
    sum += *word++;
  if (len)
    sum += *word & htons(0x0000ffff);

  // Fold 32-bit into 16 bits
  while (sum >> 16)
    sum = (sum & 0xffff) + (sum >> 16);
  return ~sum;
}

// In an ICMPv6 Ethernet Frame *frame with length frame_len, replace the mac
// address in option opt_type into *target_mac. nd_hdr_len indicates the length
// of ICMPv6 ND message headers (so the first option starts after nd_hdr_len.)
void NDProxy::ReplaceMacInIcmpOption(uint8_t* frame,
                                     ssize_t frame_len,
                                     size_t nd_hdr_len,
                                     uint8_t opt_type,
                                     const uint8_t* target_mac) {
  nd_opt_hdr* opt;
  nd_opt_hdr* end = reinterpret_cast<nd_opt_hdr*>(&frame[frame_len]);
  for (opt = reinterpret_cast<nd_opt_hdr*>(frame + ETHER_HDR_LEN +
                                           sizeof(ip6_hdr) + nd_hdr_len);
       opt < end && opt->nd_opt_len > 0;
       opt = reinterpret_cast<nd_opt_hdr*>(reinterpret_cast<uint64_t*>(opt) +
                                           opt->nd_opt_len)) {
    if (opt->nd_opt_type == opt_type) {
      uint8_t* mac_in_opt =
          reinterpret_cast<uint8_t*>(opt) + sizeof(nd_opt_hdr);
      memcpy(mac_in_opt, target_mac, ETHER_ADDR_LEN);
    }
  }
}

// RFC 4389
// Read the input ICMPv6 frame and determine whether it should be proxied. If
// so, fill out_frame buffer with proxied frame and return the length of proxied
// frame (usually same with input frame length). Return a negative value if
// proxy is not needed or error occured.
// in_frame: buffer containing input ethernet frame;
// frame_len: the length of input frame;
// local_mac_addr: MAC address of interface that will be used to send frame;
// out_frame: buffer for output frame; should have at least space of frame_len.
ssize_t NDProxy::TranslateNDFrame(const uint8_t* in_frame,
                                  ssize_t frame_len,
                                  const uint8_t* local_mac_addr,
                                  uint8_t* out_frame) {
  if (frame_len < ETHER_HDR_LEN + sizeof(ip6_hdr)) {
    return kTranslateErrorInsufficientLength;
  }
  if (reinterpret_cast<const ethhdr*>(in_frame)->h_proto != htons(ETH_P_IPV6) ||
      reinterpret_cast<const ip6_hdr*>(in_frame + ETHER_HDR_LEN)->ip6_nxt !=
          IPPROTO_ICMPV6) {
    return kTranslateErrorNotICMPv6Frame;
  }

  memcpy(out_frame, in_frame, frame_len);
  ethhdr* eth = reinterpret_cast<ethhdr*>(out_frame);
  ip6_hdr* ip6 = reinterpret_cast<ip6_hdr*>(out_frame + ETHER_HDR_LEN);
  icmp6_hdr* icmp6 =
      reinterpret_cast<icmp6_hdr*>(out_frame + ETHER_HDR_LEN + sizeof(ip6_hdr));

  // If destination MAC is unicast (Individual/Group bit in MAC address == 0),
  // it needs to be modified into broadcast so guest OS L3 stack can see it.
  if (!(eth->h_dest[0] & 0x1)) {
    memcpy(eth->h_dest, ether_addr_broadcast, ETHER_ADDR_LEN);
  }

  switch (icmp6->icmp6_type) {
    case ND_ROUTER_SOLICIT:
      ReplaceMacInIcmpOption(out_frame, frame_len, sizeof(nd_router_solicit),
                             ND_OPT_SOURCE_LINKADDR, local_mac_addr);
      break;
    case ND_ROUTER_ADVERT: {
      // RFC 4389 Section 4.1.3.3 - Set Proxy bit
      nd_router_advert* ra = reinterpret_cast<nd_router_advert*>(icmp6);
      if (ra->nd_ra_flags_reserved & 0x04) {
        // According to RFC 4389, an RA packet with 'Proxy' bit set already
        // should not be proxied again, in order to avoid loop. However, we'll
        // need this form of proxy cascading in Crostini (Host->VM->Container)
        // so we are ignoring the check here. Note that we know we are doing RA
        // proxy in only one direction so there should be no loop.
      }
      ra->nd_ra_flags_reserved |= 0x04;

      ReplaceMacInIcmpOption(out_frame, frame_len, sizeof(nd_router_advert),
                             ND_OPT_SOURCE_LINKADDR, local_mac_addr);
      break;
    }
    case ND_NEIGHBOR_SOLICIT:
      ReplaceMacInIcmpOption(out_frame, frame_len, sizeof(nd_neighbor_solicit),
                             ND_OPT_SOURCE_LINKADDR, local_mac_addr);
      break;
    case ND_NEIGHBOR_ADVERT:
      ReplaceMacInIcmpOption(out_frame, frame_len, sizeof(nd_neighbor_advert),
                             ND_OPT_TARGET_LINKADDR, local_mac_addr);
      break;
    default:
      return kTranslateErrorNotNDFrame;
  }

  // We need to clear the old checksum first so checksum calculation does not
  // wrongly take old checksum into account.
  icmp6->icmp6_cksum = 0;
  icmp6->icmp6_cksum = Icmpv6Checksum(ip6, icmp6);

  memcpy(eth->h_source, local_mac_addr, ETHER_ADDR_LEN);
  return frame_len;
}

NDProxy::interface_mapping* NDProxy::MapForType(uint8_t type) {
  switch (type) {
    case ND_ROUTER_SOLICIT:
      return &if_map_rs_;
    case ND_ROUTER_ADVERT:
      return &if_map_ra_;
    case ND_NEIGHBOR_SOLICIT:
      return &if_map_ns_na_;
    case ND_NEIGHBOR_ADVERT:
      return &if_map_ns_na_;
    default:
      LOG(DFATAL) << "Attempt to get interface map on illegal icmpv6 type "
                  << static_cast<int>(type);
      return nullptr;
  }
}

void NDProxy::ProxyNDFrame(int target_if, ssize_t frame_len) {
  ifreq ifr = {
      .ifr_ifindex = target_if,
  };
  if (ioctl(fd_.get(), SIOCGIFNAME, &ifr) < 0) {
    PLOG(ERROR) << "ioctl() failed to get interface name on interface "
                << target_if;
    return;
  }
  if (ioctl(fd_.get(), SIOCGIFHWADDR, &ifr) < 0) {
    PLOG(ERROR) << "ioctl() failed to get MAC address on interface "
                << target_if;
    return;
  }

  int result =
      TranslateNDFrame(in_frame_buffer_, frame_len,
                       reinterpret_cast<const uint8_t*>(ifr.ifr_addr.sa_data),
                       out_frame_buffer_);
  if (result < 0) {
    switch (result) {
      case kTranslateErrorNotICMPv6Frame:
        LOG(DFATAL) << "Attempt to TranslateNDFrame on a non-ICMPv6 frame";
        return;
      case kTranslateErrorNotNDFrame:
        LOG(DFATAL)
            << "Attempt to TranslateNDFrame on a non-NDP frame, icmpv6 type = "
            << static_cast<int>(reinterpret_cast<icmp6_hdr*>(in_frame_buffer_ +
                                                             ETHER_HDR_LEN +
                                                             sizeof(ip6_hdr))
                                    ->icmp6_type);
        return;
      case kTranslateErrorInsufficientLength:
        LOG(DFATAL) << "TranslateNDFrame failed: frame_len = " << frame_len
                    << " is too small";
        return;
      default:
        LOG(DFATAL) << "Unknown error in TranslateNDFrame";
        return;
    }
  }

  struct iovec iov = {
      .iov_base = out_frame_buffer_,
      .iov_len = static_cast<size_t>(frame_len),
  };
  sockaddr_ll addr = {
      .sll_family = AF_PACKET,
      .sll_ifindex = target_if,
      .sll_halen = ETHER_ADDR_LEN,
      .sll_protocol = htons(ETH_P_IPV6),
  };
  memcpy(addr.sll_addr, reinterpret_cast<ethhdr*>(out_frame_buffer_)->h_dest,
         ETHER_ADDR_LEN);
  msghdr hdr = {
      .msg_name = &addr,
      .msg_namelen = sizeof(addr),
      .msg_iov = &iov,
      .msg_iovlen = 1,
      .msg_control = nullptr,
      .msg_controllen = 0,
  };

  if (sendmsg(fd_.get(), &hdr, 0) < 0) {
    PLOG(ERROR) << "sendmsg() failed on interface " << target_if;
  }
}

void NDProxy::Start() {
  fd_ = base::ScopedFD(socket(AF_PACKET, SOCK_RAW, htons(ETH_P_IPV6)));
  if (!fd_.is_valid()) {
    PLOG(ERROR) << "socket() failed";
    return;
  }

  sockaddr_ll dst_addr;
  struct iovec iov = {
      .iov_base = in_frame_buffer_,
      .iov_len = IP_MAXPACKET,
  };
  msghdr hdr = {
      .msg_name = &dst_addr,
      .msg_namelen = sizeof(dst_addr),
      .msg_iov = &iov,
      .msg_iovlen = 1,
      .msg_control = nullptr,
      .msg_controllen = 0,
      .msg_flags = 0,
  };

  ssize_t len;
  while (true) {
    if ((len = recvmsg(fd_.get(), &hdr, 0)) < 0) {
      PLOG(ERROR) << "recvmsg() failed";
      return;
    }
    ip6_hdr* ip6 = reinterpret_cast<ip6_hdr*>(in_frame_buffer_ + ETH_HLEN);
    icmp6_hdr* icmp6 = reinterpret_cast<icmp6_hdr*>(
        in_frame_buffer_ + ETHER_HDR_LEN + sizeof(ip6_hdr));
    if (ip6->ip6_nxt == IPPROTO_ICMPV6 &&
        icmp6->icmp6_type >= ND_ROUTER_SOLICIT &&
        icmp6->icmp6_type <= ND_NEIGHBOR_ADVERT) {
      auto map_entry =
          MapForType(icmp6->icmp6_type)->find(dst_addr.sll_ifindex);
      if (map_entry != MapForType(icmp6->icmp6_type)->end()) {
        const auto& target_ifs = map_entry->second;
        for (auto target_if : target_ifs) {
          ProxyNDFrame(target_if, len);
        }
      }
    }
  }
}

bool NDProxy::AddRouterInterfacePair(const std::string& ifname_physical,
                                     const std::string& ifname_guest) {
  return AddInterfacePairInternal(ifname_physical, ifname_guest, true);
}

bool NDProxy::AddPeeringInterfacePair(const std::string& ifname1,
                                      const std::string& ifname2) {
  return AddInterfacePairInternal(ifname1, ifname2, false);
}

bool NDProxy::AddInterfacePairInternal(const std::string& ifname1,
                                       const std::string& ifname2,
                                       bool proxy_rs_ra) {
  int ifindex1 = if_nametoindex(ifname1.c_str());
  if (ifindex1 == 0) {
    PLOG(ERROR) << "Get interface index failed on " << ifname1;
    return false;
  }
  int ifindex2 = if_nametoindex(ifname2.c_str());
  if (ifindex2 == 0) {
    PLOG(ERROR) << "Get interface index failed on " << ifname2;
    return false;
  }
  if (ifindex1 == ifindex2) {
    LOG(ERROR) << "Rejected attempt to forward between same interface "
               << ifname1 << " and " << ifname2;
    return false;
  }
  if (proxy_rs_ra) {
    if_map_rs_[ifindex2].insert(ifindex1);
    if_map_ra_[ifindex1].insert(ifindex2);
  }
  if_map_ns_na_[ifindex1].insert(ifindex2);
  if_map_ns_na_[ifindex2].insert(ifindex1);
  return true;
}

bool NDProxy::RemoveInterface(const std::string& ifname) {
  int ifindex = if_nametoindex(ifname.c_str());
  if (ifindex == 0) {
    PLOG(ERROR) << "Get interface index failed on " << ifname;
    return false;
  }
  if_map_rs_.erase(ifindex);
  for (auto& kv : if_map_rs_)
    kv.second.erase(ifindex);
  if_map_ra_.erase(ifindex);
  for (auto& kv : if_map_ra_)
    kv.second.erase(ifindex);
  if_map_ns_na_.erase(ifindex);
  for (auto& kv : if_map_ns_na_)
    kv.second.erase(ifindex);
  return true;
}

}  // namespace arc_networkd
