// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <arpa/inet.h>
#include <netinet/icmp6.h>

#include <shill/net/byte_string.h>
#include <shill/net/ip_address.h>

#include <gtest/gtest.h>

#include "portier/ipv6_util.h"

namespace portier {

using shill::IPAddress;
using shill::ByteString;

using Code = Status::Code;

// Test data.
namespace {

// The zero packet.
const IPAddress kSourceAddress1 = IPAddress("::");
const IPAddress kDestinationAddress1 = IPAddress("::");
const uint8_t kNextHeader1 = 0;
const uint8_t kData1[] = {};
const uint16_t kExpectedChecksum1 = 0;

// A more realistic zero packet.
const IPAddress kSourceAddress2 = IPAddress("::");
const IPAddress kDestinationAddress2 =
    IPAddress("ff02::1");         // All node link-local multicast.
const uint8_t kNextHeader2 = 59;  // No next header. (0x3b)
const uint8_t kData2[] = {};
const uint16_t kExpectedChecksum2 = 0xff3e;

// All 1s (where valid) pseudo-header packet.
const IPAddress kSourceAddress3 =
    IPAddress("ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff");
const IPAddress kDestinationAddress3 =
    IPAddress("ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff");
const uint8_t kNextHeader3 = 0xff;  // No next header. (0x3b)
const uint8_t kData3[] = {};
const uint16_t kExpectedChecksum3 = 0x00ff;

// ICMPv6 Echo Request
const IPAddress kSourceAddress4 =
    IPAddress("2401:fa00:480:56:c1dd:402b:cc2f:7209");
const IPAddress kDestinationAddress4 = IPAddress("fe80::aef2:c5ff:fe71:17bf");
const uint8_t kNextHeader4 = 58;  // ICMPv6.
// clang-format off
const uint8_t kData4[] = {
  // Type=Echo Request (128), Code=0, Checksum=0.
  0x80, 0x00, 0x00, 0x00,
  // Id=1337, Sequence=9001.
  0x05, 0x39, 0x23, 0x29,
  // Data.
  0x11, 0x22, 0x33, 0x44,
  0x55, 0x66, 0x77, 0x88
};
// clang-format on
const uint16_t kExpectedChecksum4 = 0xa6c0;

// Odd byte length packet.
const IPAddress kSourceAddress5 = IPAddress("fe80::1");
const IPAddress kDestinationAddress5 = IPAddress("ff02::1");
const uint8_t kNextHeader5 = 58;
const uint8_t kData5[] = {0x11, 0x22, 0x33};
const uint16_t kExpectedChecksum5 = 0x41e5;

}  // namespace

// Test using valid input.
TEST(IPv6UpperLayerChecksum16Test, ZeroPacket) {
  uint16_t checksum = 0;
  const Status status =
      IPv6UpperLayerChecksum16(kSourceAddress1, kDestinationAddress1,
                               kNextHeader1, kData1, 0, &checksum);
  EXPECT_TRUE(status);
  EXPECT_EQ(ntohs(checksum), kExpectedChecksum1);
}

TEST(IPv6UpperLayerChecksum16Test, RealisticZeroPacket) {
  uint16_t checksum = 0;
  const Status status =
      IPv6UpperLayerChecksum16(kSourceAddress2, kDestinationAddress2,
                               kNextHeader2, kData2, 0, &checksum);
  EXPECT_TRUE(status);
  EXPECT_EQ(ntohs(checksum), kExpectedChecksum2);
}

TEST(IPv6UpperLayerChecksum16Test, AllOnesPacket) {
  uint16_t checksum = 0;
  const Status status =
      IPv6UpperLayerChecksum16(kSourceAddress3, kDestinationAddress3,
                               kNextHeader3, kData3, 0, &checksum);
  EXPECT_TRUE(status);
  EXPECT_EQ(ntohs(checksum), kExpectedChecksum3);
}

TEST(IPv6UpperLayerChecksum16Test, EchoRequestPacket) {
  uint16_t checksum = 0;
  const Status status =
      IPv6UpperLayerChecksum16(kSourceAddress4, kDestinationAddress4,
                               kNextHeader4, kData4, sizeof(kData4), &checksum);
  EXPECT_TRUE(status);
  EXPECT_EQ(ntohs(checksum), kExpectedChecksum4);
}

TEST(IPv6UpperLayerChecksum16Test, OddByteLength) {
  uint16_t checksum = 0;
  const Status status =
      IPv6UpperLayerChecksum16(kSourceAddress5, kDestinationAddress5,
                               kNextHeader5, kData5, sizeof(kData5), &checksum);
  EXPECT_TRUE(status);
  EXPECT_EQ(ntohs(checksum), kExpectedChecksum5);
}

TEST(IPv6UpperLayerChecksum16Test, ChecksumProcess) {
  // Create generic ICMPv6 header.
  struct icmp6_hdr icmp6_hdr;
  memset(&icmp6_hdr, 0, sizeof(icmp6_hdr));
  icmp6_hdr.icmp6_type = ICMP6_DST_UNREACH;
  icmp6_hdr.icmp6_data32[0] = 0x12345678;

  // Calculate the checksum.
  uint16_t checksum = 0;
  Status status = IPv6UpperLayerChecksum16(
      kSourceAddress5, kDestinationAddress5, kNextHeader5,
      reinterpret_cast<const uint8_t*>(&icmp6_hdr), sizeof(icmp6_hdr),
      &checksum);

  EXPECT_TRUE(status);

  // Provide the checksum to the ICMP header.
  icmp6_hdr.icmp6_cksum = ~checksum;

  // Recalculate.
  status = IPv6UpperLayerChecksum16(
      kSourceAddress5, kDestinationAddress5, kNextHeader5,
      reinterpret_cast<const uint8_t*>(&icmp6_hdr), sizeof(icmp6_hdr),
      &checksum);

  EXPECT_TRUE(status);
  EXPECT_EQ(checksum, 0);
}

}  // namespace portier
