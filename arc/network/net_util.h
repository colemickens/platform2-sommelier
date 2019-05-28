// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <string>

#ifndef ARC_NETWORK_NET_UTIL_H_
#define ARC_NETWORK_NET_UTIL_H_

namespace arc_networkd {

// Reverses the byte order of the argument.
constexpr uint32_t Byteswap32(uint32_t x) {
  return (x >> 24) | (x << 24) | ((x >> 8) & 0xff00) | ((x << 8) & 0xff0000);
}

// Reverses the byte order of the argument.
constexpr uint16_t Byteswap16(uint16_t x) {
  return (x >> 8) | (x << 8);
}

// Constexpr version of ntohl().
constexpr uint32_t Ntohl(uint32_t x) {
  return Byteswap32(x);
}

// Constexpr version of htonl().
constexpr uint32_t Htonl(uint32_t x) {
  return Byteswap32(x);
}

// Constexpr version of ntohs().
constexpr uint16_t Ntohs(uint16_t x) {
  return Byteswap16(x);
}

// Constexpr version of htons().
constexpr uint16_t Htons(uint16_t x) {
  return Byteswap16(x);
}

// Returns the network-byte order int32 representation of the IPv4 address given
// byte per byte, most significant bytes first.
constexpr uint32_t Ipv4Addr(uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3) {
  return (b3 << 24) | (b2 << 16) | (b1 << 8) | b0;
}

// Returns the literal representation of the IPv4 address given in network byte
// order.
std::string IPv4AddressToString(uint32_t addr);

// Returns the CIDR representation of an IPv4 address given in network byte
// order.
std::string IPv4AddressToCidrString(uint32_t addr, uint32_t prefix_length);

}  // namespace arc_networkd

#endif  // ARC_NETWORK_NET_UTIL_H_
