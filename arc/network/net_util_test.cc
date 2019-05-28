// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arc/network/net_util.h"

#include <arpa/inet.h>
#include <byteswap.h>

#include <gtest/gtest.h>

namespace arc_networkd {

TEST(Byteswap, 16bits) {
  uint32_t test_cases[] = {
      0x0000, 0x0001, 0x1000, 0xffff, 0x2244, 0xfffe,
  };

  for (uint32_t value : test_cases) {
    EXPECT_EQ(Byteswap16(value), bswap_16(value));
    EXPECT_EQ(ntohs(value), Ntohs(value));
    EXPECT_EQ(htons(value), Htons(value));
  }
}

TEST(Byteswap, 32bits) {
  uint32_t test_cases[] = {
      0x00000000, 0x00000001, 0x10000000, 0xffffffff, 0x11335577, 0xdeadbeef,
  };

  for (uint32_t value : test_cases) {
    EXPECT_EQ(Byteswap32(value), bswap_32(value));
    EXPECT_EQ(ntohl(value), Ntohl(value));
    EXPECT_EQ(htonl(value), Htonl(value));
  }
}

TEST(Ipv4, CreationAndStringConversion) {
  struct {
    std::string literal_address;
    uint8_t bytes[4];
  } test_cases[] = {
      {"0.0.0.0", {0, 0, 0, 0}},
      {"8.8.8.8", {8, 8, 8, 8}},
      {"8.8.4.4", {8, 8, 4, 4}},
      {"192.168.0.0", {192, 168, 0, 0}},
      {"100.115.92.5", {100, 115, 92, 5}},
      {"100.115.92.6", {100, 115, 92, 6}},
      {"224.0.0.251", {224, 0, 0, 251}},
      {"255.255.255.255", {255, 255, 255, 255}},
  };

  for (auto const& test_case : test_cases) {
    uint32_t addr = Ipv4Addr(test_case.bytes[0], test_case.bytes[1],
                             test_case.bytes[2], test_case.bytes[3]);
    EXPECT_EQ(test_case.literal_address, IPv4AddressToString(addr));
  }
}

TEST(Ipv4, CreationAndCidrStringConversion) {
  struct {
    std::string literal_address;
    uint8_t bytes[4];
    uint32_t prefix_length;
  } test_cases[] = {
      {"0.0.0.0/0", {0, 0, 0, 0}, 0},
      {"192.168.0.0/24", {192, 168, 0, 0}, 24},
      {"100.115.92.5/30", {100, 115, 92, 5}, 30},
      {"100.115.92.6/30", {100, 115, 92, 6}, 30},
  };

  for (auto const& test_case : test_cases) {
    uint32_t addr = Ipv4Addr(test_case.bytes[0], test_case.bytes[1],
                             test_case.bytes[2], test_case.bytes[3]);
    EXPECT_EQ(test_case.literal_address,
              IPv4AddressToCidrString(addr, test_case.prefix_length));
  }
}

}  // namespace arc_networkd
