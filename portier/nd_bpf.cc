// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits.h>
#include <stddef.h>

#include <linux/filter.h>
#include <net/ethernet.h>
#include <netinet/icmp6.h>
#include <netinet/ip6.h>

#include "portier/nd_bpf.h"

namespace portier {

namespace {

// Outgoing hop-limit of proxied IPv6 packets.  Required by RFC 4389 to
// prevent receivers from dropping what may appear to be forwarded packets.
constexpr uint8_t kProxiedHopLimit = 255;

// This constant is used by BPF programs to indicate that the entire packet
// should be returned to user-space.
constexpr uint32_t kEntirePacket = INT_MAX;

// BPF notes:
//   BPF_STMT(code, k)
//   BPF_JUMP(code, k, j true, j false)
//
// The return value of a BPF program is the number of bytes of the packet
// that should be passed to the user-space program.  Zero is a special
// value in that the packet is simply dropped, and the user-space socket
// is never notified of the packet.  Returning a number larger than the
// actaul length of the packet will cause the entire packet to be passed
// to the user-space program.
//
// Any out of bounds exceptions, illegal OPs code, divide by zero, and other
// possible errors cause the BPF program to exit as if it had returned zero.
// This is a desired feature as the BPF program can be made to assume the
// whole packet is received without doing any bound checks.  If the packet
// was corrupted or truncated during transit, then the BPF will drop it
// when accessing out of bound data.

// BPF for ICMPv6 Neighbor Solicitation.
// Algorithm:
//  ether_header = packet_buf;  // Start of frame.
//  if (ether_header->ether_type != IPv6) {
//    return 0;
//  }
//  ip6_hdr = ether_header + sizeof(struct ether_header);  // IPv6 header
//  if (ip6_hdr->ip6_nxt != ICMPv6 || ip6_hdr->ip6_hops != 255) {
//    return 0;
//  }
//  icmp6_hdr = ip6_hdr + sizeof(struct ip6_hdr);  // ICMPv6 header
//  if (icmp6_hdr->icmp6_type != NS &&
//      icmp6_hdr->icmp6_type != NA &&
//      icmp6_hdr->icmp6_type != RA &&
//      icmp6_hdr->icmp6_type != R) {
//    return 0;
//  }
//  if (icmp6_hdr->icmp6_code != 0) {
//    return 0;
//  }
//  return MAX;
struct sock_filter kNeighborDiscoveryFilterInstructions[] = {
    // Load ethernet type (16-bits, H).
    BPF_STMT(BPF_LD | BPF_H | BPF_ABS,
             offsetof(struct ether_header, ether_type)),
    // Check if it equals IPv6, skip next if true.
    BPF_JUMP(BPF_JMP | BPF_JEQ, ETHERTYPE_IPV6, 1, 0),
    // Return 0.
    BPF_STMT(BPF_RET | BPF_K, 0),
    // Move index to start of IPv6 header.
    BPF_STMT(BPF_LDX | BPF_IMM, sizeof(struct ether_header)),
    // Load IPv6 next header (8-bits, B).
    BPF_STMT(BPF_LD | BPF_B | BPF_IND, offsetof(struct ip6_hdr, ip6_nxt)),
    // Check if equals ICMPv6, if not, then goto return 0.
    BPF_JUMP(BPF_JMP | BPF_JEQ, IPPROTO_ICMPV6, 0, 2),
    // Load IPv6 hop limit (8-bit, B).
    BPF_STMT(BPF_LD | BPF_B | BPF_IND, offsetof(struct ip6_hdr, ip6_hops)),
    // Check if equal to 255, skip return if true.
    BPF_JUMP(BPF_JMP | BPF_JEQ, kProxiedHopLimit, 1, 0),
    // Return 0.
    BPF_STMT(BPF_RET | BPF_K, 0),
    // Move index to start of ICMPv6 header.
    BPF_STMT(BPF_MISC | BPF_TXA, 0),
    BPF_STMT(BPF_ALU | BPF_ADD | BPF_IMM, sizeof(struct ip6_hdr)),
    BPF_STMT(BPF_MISC | BPF_TAX, 0),
    // Load ICMPv6 type (8-bits, B).
    BPF_STMT(BPF_LD | BPF_B | BPF_IND, offsetof(struct icmp6_hdr, icmp6_type)),
    // Check if is ND ICMPv6 message.
    BPF_JUMP(BPF_JMP | BPF_JEQ, ND_ROUTER_ADVERT, 4, 0),
    BPF_JUMP(BPF_JMP | BPF_JEQ, ND_NEIGHBOR_SOLICIT, 3, 0),
    BPF_JUMP(BPF_JMP | BPF_JEQ, ND_NEIGHBOR_ADVERT, 2, 0),
    BPF_JUMP(BPF_JMP | BPF_JEQ, ND_REDIRECT, 1, 0),
    // Return 0.
    BPF_STMT(BPF_RET | BPF_K, 0),
    // Load ICMPv6 code (8-bit, B).
    BPF_STMT(BPF_LD | BPF_B | BPF_IND, offsetof(struct icmp6_hdr, icmp6_code)),
    // Check if is code 0.
    BPF_JUMP(BPF_JMP | BPF_JEQ, 0, 1, 0),
    // Return 0.
    BPF_STMT(BPF_RET | BPF_K, 0),
    // Return MAX.
    BPF_STMT(BPF_RET | BPF_K, kEntirePacket),
};

// BPF for IPv6 packets, other than ICMPv6 Neighbor Discovery.
// Algorithm is the similar to ND Filter above, except that it returns
// max if the IPv6 packet is not a one of the ND Messages that require
// special proxying rules.
struct sock_filter kNonNDFilterInstructions[] = {
    // Load ethernet type (16-bits, H).
    BPF_STMT(BPF_LD | BPF_H | BPF_ABS,
             offsetof(struct ether_header, ether_type)),
    // Check if it equals IPv6, skip next if true.
    BPF_JUMP(BPF_JMP | BPF_JEQ, ETHERTYPE_IPV6, 1, 0),
    // Return 0.
    BPF_STMT(BPF_RET | BPF_K, 0),
    // Move index to start of IPv6 header.
    BPF_STMT(BPF_LDX | BPF_IMM, sizeof(struct ether_header)),
    // Load IPv6 next header (8-bits, B).
    BPF_STMT(BPF_LD | BPF_B | BPF_IND, offsetof(struct ip6_hdr, ip6_nxt)),
    // Check if equals ICMPv6, if not, then return max.
    BPF_JUMP(BPF_JMP | BPF_JEQ, IPPROTO_ICMPV6, 1, 0),
    // Return MAX.
    BPF_STMT(BPF_RET | BPF_K, kEntirePacket),
    // Move index to start of ICMPv6 header.
    BPF_STMT(BPF_MISC | BPF_TXA, 0),
    BPF_STMT(BPF_ALU | BPF_ADD | BPF_IMM, sizeof(struct ip6_hdr)),
    BPF_STMT(BPF_MISC | BPF_TAX, 0),
    // Load ICMPv6 type (8-bits, B).
    BPF_STMT(BPF_LD | BPF_B | BPF_IND, offsetof(struct icmp6_hdr, icmp6_type)),
    // Check if is ND ICMPv6 message.
    BPF_JUMP(BPF_JMP | BPF_JEQ, ND_ROUTER_ADVERT, 4, 0),
    BPF_JUMP(BPF_JMP | BPF_JEQ, ND_NEIGHBOR_SOLICIT, 3, 0),
    BPF_JUMP(BPF_JMP | BPF_JEQ, ND_NEIGHBOR_ADVERT, 2, 0),
    BPF_JUMP(BPF_JMP | BPF_JEQ, ND_REDIRECT, 1, 0),
    // Return MAX.
    BPF_STMT(BPF_RET | BPF_K, kEntirePacket),
    // Return 0.
    BPF_STMT(BPF_RET | BPF_K, 0),
};

}  // namespace

const struct sock_fprog kNeighborDiscoveryFilter = {
    sizeof(kNeighborDiscoveryFilterInstructions) / sizeof(struct sock_filter),
    kNeighborDiscoveryFilterInstructions};

const struct sock_fprog kNonNeighborDiscoveryFilter = {
    sizeof(kNonNDFilterInstructions) / sizeof(struct sock_filter),
    kNonNDFilterInstructions};

}  // namespace portier.
