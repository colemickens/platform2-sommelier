// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "portier/ipv6_util.h"

#include <arpa/inet.h>
#include <netinet/icmp6.h>

#include <gtest/gtest.h>
#include <shill/net/ip_address.h>

namespace portier {

using shill::IPAddress;

using Code = Status::Code;

// Test data.
namespace {

// Checksum test data.

// The zero packet.
const IPAddress kSourceAddress1 = IPAddress("::");
const IPAddress kDestinationAddress1 = IPAddress("::");
constexpr uint8_t kNextHeader1 = 0;
constexpr uint8_t kData1[] = {};
constexpr uint16_t kExpectedChecksum1 = 0;

// A more realistic zero packet.
const IPAddress kSourceAddress2 = IPAddress("::");
const IPAddress kDestinationAddress2 =
    IPAddress("ff02::1");         // All node link-local multicast.
constexpr uint8_t kNextHeader2 = 59;  // No next header. (0x3b)
constexpr uint8_t kData2[] = {};
constexpr uint16_t kExpectedChecksum2 = 0xff3e;

// All 1s (where valid) pseudo-header packet.
const IPAddress kSourceAddress3 =
    IPAddress("ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff");
const IPAddress kDestinationAddress3 =
    IPAddress("ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff");
constexpr uint8_t kNextHeader3 = 0xff;  // No next header. (0x3b)
constexpr uint8_t kData3[] = {};
constexpr uint16_t kExpectedChecksum3 = 0x00ff;

// ICMPv6 Echo Request
const IPAddress kSourceAddress4 =
    IPAddress("2401:fa00:480:56:c1dd:402b:cc2f:7209");
const IPAddress kDestinationAddress4 = IPAddress("fe80::aef2:c5ff:fe71:17bf");
constexpr uint8_t kNextHeader4 = 58;  // ICMPv6.
// clang-format off
constexpr uint8_t kData4[] = {
  // Type=Echo Request (128), Code=0, Checksum=0.
  0x80, 0x00, 0x00, 0x00,
  // Id=1337, Sequence=9001.
  0x05, 0x39, 0x23, 0x29,
  // Data.
  0x11, 0x22, 0x33, 0x44,
  0x55, 0x66, 0x77, 0x88
};
// clang-format on
constexpr uint16_t kExpectedChecksum4 = 0xa6c0;

// Odd byte length packet.
const IPAddress kSourceAddress5 = IPAddress("fe80::1");
const IPAddress kDestinationAddress5 = IPAddress("ff02::1");
constexpr uint8_t kNextHeader5 = 58;
constexpr uint8_t kData5[] = {0x11, 0x22, 0x33};
constexpr uint16_t kExpectedChecksum5 = 0x41e5;

// IP type test data.

const IPAddress kUnspecifiedAddress = IPAddress("::");
const IPAddress kLocalHostAddress = IPAddress("::1");

const IPAddress kLinkLocalAddress1 = IPAddress("fe80::3c20:87b0:b0ce:23f4");
const IPAddress kLinkLocalAddress2 = IPAddress("fe80::f155:a038:ae18:faf2");
const IPAddress kLinkLocalAddress3 = IPAddress("fe80::5a6d:8fff:fe99:e5be");

const IPAddress kNonLinkLocalAddress1 =
    IPAddress("2620:15c:202:201:f155:a038:ae18:faf2");
const IPAddress kNonLinkLocalAddress2 =
    IPAddress("2401:fa00:480:56:5a6d:8fff:fe99:e5be");

// Well-known multicast addresses.
const IPAddress kAllNodesLinkLocalMulticastAddress = IPAddress("ff02::1");
const IPAddress kAllRoutersLinkLocalMulticastAddress = IPAddress("ff02::2");
const IPAddress kAllRoutersSiteLocalMulticastAddress = IPAddress("ff05::2");

const LLAddress kAllNodesLinkLocalMulticastLLAddress =
    LLAddress(LLAddress::Type::kEui48, "33:33:00:00:00:01");
const LLAddress kAllRouterLinkLocalMulticastLLAddress =
    LLAddress(LLAddress::Type::kEui48, "33:33:00:00:00:02");
const LLAddress kAllRouterSiteLocalMulticastLLAddress =
    LLAddress(LLAddress::Type::kEui48, "33:33:00:00:00:02");

// Addresses which are "close" to being multicast address, missing only
// a few bits.
const IPAddress kNonMulticastAddress1 = IPAddress("fe05::1");
const IPAddress kNonMulticastAddress2 = IPAddress("7f05::2");
const IPAddress kNonMulticastAddress3 = IPAddress("ee05::3");

const IPAddress kSolicitedNodeMulticast1 =
    IPAddress("ff02:0:0:0:0:1:ff18:faf2");
const IPAddress kSolicitedNodeMulticast2 =
    IPAddress("ff02:0:0:0:0:1:ff99:e5be");
const IPAddress kNotSolicitedNodeMulticast1 =
    IPAddress("ff02:0:0:0:0:0:ff18:faf2");
const IPAddress kNotSolicitedNodeMulticast2 =
    IPAddress("ff02:0:0:0:0:0:ff99:e5be");
const IPAddress kSolicitedAddress1 =
    IPAddress("2620:15c:202:201:f155:a038:ae18:faf2");
const IPAddress kSolicitedAddress2 =
    IPAddress("2401:fa00:480:56:5a6d:8fff:fe99:e5be");
const LLAddress kSolicitedNodeMulticastLL1 =
    LLAddress(LLAddress::Type::kEui48, "33:33:ff:18:fa:f2");
const LLAddress kSolicitedNodeMulticastLL2 =
    LLAddress(LLAddress::Type::kEui48, "33:33:ff:99:e5:be");

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

TEST(IPv6AddressTypeTest, UnspecifiedAddress) {
  EXPECT_TRUE(IPv6AddressIsUnspecified(kUnspecifiedAddress));
}

TEST(IPv6AddressTypeTest, NotUnspecifiedAddress) {
  EXPECT_FALSE(IPv6AddressIsUnspecified(kLocalHostAddress));
  EXPECT_FALSE(IPv6AddressIsUnspecified(kLinkLocalAddress1));
  EXPECT_FALSE(IPv6AddressIsUnspecified(kLinkLocalAddress2));
  EXPECT_FALSE(IPv6AddressIsUnspecified(kLinkLocalAddress3));
  EXPECT_FALSE(IPv6AddressIsUnspecified(kNonLinkLocalAddress1));
  EXPECT_FALSE(IPv6AddressIsUnspecified(kNonLinkLocalAddress2));
}

TEST(IPv6AddressTypeTest, LinkLocalAddress) {
  EXPECT_TRUE(IPv6AddressIsLinkLocal(kLinkLocalAddress1));
  EXPECT_TRUE(IPv6AddressIsLinkLocal(kLinkLocalAddress2));
  EXPECT_TRUE(IPv6AddressIsLinkLocal(kLinkLocalAddress3));
}

TEST(IPv6AddressTypeTest, NotLinkLocalAddress) {
  EXPECT_FALSE(IPv6AddressIsLinkLocal(kNonLinkLocalAddress1));
  EXPECT_FALSE(IPv6AddressIsLinkLocal(kNonLinkLocalAddress2));

  EXPECT_FALSE(IPv6AddressIsLinkLocal(kUnspecifiedAddress));
  EXPECT_FALSE(IPv6AddressIsLinkLocal(kLocalHostAddress));
}

TEST(IPv6AddressTypeTest, MulticastAddress) {
  EXPECT_TRUE(IPv6AddressIsMulticast(kAllNodesLinkLocalMulticastAddress));
  EXPECT_TRUE(IPv6AddressIsMulticast(kAllRoutersLinkLocalMulticastAddress));
  EXPECT_TRUE(IPv6AddressIsMulticast(kAllRoutersSiteLocalMulticastAddress));

  EXPECT_TRUE(IPv6AddressIsMulticast(kSolicitedNodeMulticast1));
  EXPECT_TRUE(IPv6AddressIsMulticast(kSolicitedNodeMulticast2));
  EXPECT_TRUE(IPv6AddressIsMulticast(kNotSolicitedNodeMulticast1));
  EXPECT_TRUE(IPv6AddressIsMulticast(kNotSolicitedNodeMulticast2));
}

TEST(IPv6AddressTypeTest, NotMulticastAddress) {
  EXPECT_FALSE(IPv6AddressIsMulticast(kUnspecifiedAddress));
  EXPECT_FALSE(IPv6AddressIsMulticast(kLocalHostAddress));

  EXPECT_FALSE(IPv6AddressIsMulticast(kLinkLocalAddress1));
  EXPECT_FALSE(IPv6AddressIsMulticast(kLinkLocalAddress2));
  EXPECT_FALSE(IPv6AddressIsMulticast(kLinkLocalAddress3));

  EXPECT_FALSE(IPv6AddressIsMulticast(kNonMulticastAddress1));
  EXPECT_FALSE(IPv6AddressIsMulticast(kNonMulticastAddress2));
  EXPECT_FALSE(IPv6AddressIsMulticast(kNonMulticastAddress3));
}

TEST(IPv6AddressTypeTest, SolicitedNode) {
  EXPECT_TRUE(IPv6AddressIsSolicitedNodeMulticast(kSolicitedNodeMulticast1,
                                                  kSolicitedAddress1));
  EXPECT_TRUE(IPv6AddressIsSolicitedNodeMulticast(kSolicitedNodeMulticast2,
                                                  kSolicitedAddress2));
}

TEST(IPv6AddressTypeTest, NotSolicitedNode) {
  // Mixed multicast and solicited.
  EXPECT_FALSE(IPv6AddressIsSolicitedNodeMulticast(kSolicitedAddress1,
                                                   kSolicitedNodeMulticast1));
  EXPECT_FALSE(IPv6AddressIsSolicitedNodeMulticast(kSolicitedAddress2,
                                                   kSolicitedNodeMulticast2));

  // Not a solicitation of provided address.
  EXPECT_FALSE(IPv6AddressIsSolicitedNodeMulticast(kSolicitedNodeMulticast1,
                                                   kSolicitedAddress2));
  EXPECT_FALSE(IPv6AddressIsSolicitedNodeMulticast(kSolicitedNodeMulticast2,
                                                   kSolicitedAddress1));

  // Not a solicited-node address, but bottom 24-bits match.
  EXPECT_FALSE(IPv6AddressIsSolicitedNodeMulticast(kNotSolicitedNodeMulticast1,
                                                   kSolicitedAddress1));
  EXPECT_FALSE(IPv6AddressIsSolicitedNodeMulticast(kNotSolicitedNodeMulticast2,
                                                   kSolicitedAddress2));
}

TEST(IPv6AddressToLinkLayerTest, MulticastLinkLayer) {
  LLAddress ll_address;

  EXPECT_TRUE(IPv6GetMulticastLinkLayerAddress(
      kAllNodesLinkLocalMulticastAddress, &ll_address));
  EXPECT_TRUE(kAllNodesLinkLocalMulticastLLAddress.Equals(ll_address));

  EXPECT_TRUE(IPv6GetMulticastLinkLayerAddress(
      kAllRoutersLinkLocalMulticastAddress, &ll_address));
  EXPECT_TRUE(kAllRouterLinkLocalMulticastLLAddress.Equals(ll_address));

  EXPECT_TRUE(IPv6GetMulticastLinkLayerAddress(
      kAllRoutersSiteLocalMulticastAddress, &ll_address));
  EXPECT_TRUE(kAllRouterSiteLocalMulticastLLAddress.Equals(ll_address));

  EXPECT_TRUE(
      IPv6GetMulticastLinkLayerAddress(kSolicitedNodeMulticast1, &ll_address));
  EXPECT_TRUE(kSolicitedNodeMulticastLL1.Equals(ll_address));

  EXPECT_TRUE(
      IPv6GetMulticastLinkLayerAddress(kSolicitedNodeMulticast2, &ll_address));
  EXPECT_TRUE(kSolicitedNodeMulticastLL2.Equals(ll_address));
}

}  // namespace portier
