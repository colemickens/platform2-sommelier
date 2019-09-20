// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "portier/nd_msg.h"

#include <arpa/inet.h>

#include <base/time/time.h>
#include <gtest/gtest.h>
#include <shill/net/byte_string.h>
#include <shill/net/ip_address.h>

namespace portier {

using base::TimeDelta;
using shill::IPAddress;
using shill::ByteString;

// Testing constants.
namespace {

// clang-format off

constexpr uint8_t kRSMessage1[] = {
  // Type=RS (133), Code=0, Checksum=0x1234.
  0x85, 0x00, 0x12, 0x34,
  // Reserved
  0x00, 0x00, 0x00, 0x00
};

constexpr uint16_t kRSChecksum1 = 0x1234;

constexpr uint8_t kRAMessage1[] = {
  // Type=RA (134), Code=0, Checksum=0x78ab.
  0x86, 0x00, 0x78, 0xab,
  // Cur Hop Limit=255, M=1, O=0, P=1, Router Lifetime=9000 s,
  0xff, 0x84, 0x23, 0x28,
  // Reachable Time=1 day (86400000 ms)
  0x05, 0x26, 0x5c, 0x00,
  // Retrans Timer=10 minutes (600000 ms)
  0x00, 0x09, 0x27, 0xc0
};

constexpr uint16_t kRAChecksum1 = 0x78ab;
constexpr uint8_t kRACurHopLimit1 = 0xff;
constexpr bool kRAManagedFlag1 = true;
constexpr bool kRAOtherFlag1 = false;
constexpr bool kRAProxyFlag1 = true;
constexpr TimeDelta kRARouterLifetime1 = TimeDelta::FromSeconds(9000);
constexpr TimeDelta kRAReachableTime1 = TimeDelta::FromDays(1);
constexpr TimeDelta kRARetransTimer1 = TimeDelta::FromMinutes(10);

// Target:
constexpr uint8_t kNSMessage1[] = {
  // Type=NS (135), Code=0, Checksum=0x8999.
  0x87, 0x00, 0x89, 0x99,
  // Reserved
  0x00, 0x00, 0x00, 0x00,
  // Target Address=fe80::9832:3d50:3aa3:5af9
  0xfe, 0x80, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,
  0x98, 0x32, 0x3d, 0x50,
  0x3a, 0xa3, 0x5a, 0xf9
};

constexpr uint16_t kNSChecksum1 = 0x8999;
const IPAddress kNSTargetAddress1 = IPAddress("fe80::9832:3d50:3aa3:5af9");

constexpr uint8_t kNAMessage1[] = {
  // Type=NA (136), Code=0, Checksum=1.
  0x88, 0x00, 0x00, 0x01,
  // R=0, S=1, O=0
  0x40, 0x00, 0x00, 0x00,
  // Target Address=fe80::846d:e6ff:fe2d:acf3
  0xfe, 0x80, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,
  0x84, 0x6d, 0xe6, 0xff,
  0xfe, 0x2d, 0xac, 0xf3
};

constexpr uint16_t kNAChecksum1 = 0x1;
constexpr bool kNARouterFlag1 = false;
constexpr bool kNASolicitedFlag1 = true;
constexpr bool kNAOverrideFlag1 = false;
const IPAddress kNATargetAddress1 = IPAddress("fe80::846d:e6ff:fe2d:acf3");

constexpr uint8_t kRMessage1[] = {
  // Type=R (137), Code=0, Checksum=0x100.
  0x89, 0x00, 0x01, 0x00,
  // Reserved
  0x00, 0x00, 0x00, 0x00,
  // Target Address=2401:fa00:480:56:c5f1:8aa4:c5c2:5972
  0x24, 0x01, 0xfa, 0x00,
  0x04, 0x80, 0x00, 0x56,
  0xc5, 0xf1, 0x8a, 0xa4,
  0xc5, 0xc2, 0x59, 0x72,
  // Destination Address=2401:fa00:480:56:495e:b40c:9318:3ca5
  0x24, 0x01, 0xfa, 0x00,
  0x04, 0x80, 0x00, 0x56,
  0x49, 0x5e, 0xb4, 0x0c,
  0x93, 0x18, 0x3c, 0xa5
};

constexpr uint16_t kRChecksum1 = 0x100;
const IPAddress kRTargetAddress1 =
    IPAddress("2401:fa00:480:56:c5f1:8aa4:c5c2:5972");
const IPAddress kRDestinationAddress1 =
    IPAddress("2401:fa00:480:56:495e:b40c:9318:3ca5");

constexpr uint8_t kNonNDMessage[] = {
  // Type=Ping Request (128),
  0x80, 0x00, 0x00, 0x00,
  // ID=1337, Seq=9001
  0x05, 0x39, 0x23, 0x29
};

constexpr uint8_t kNAMessageBadSize[] = {
  // Type=NA (136), Code=0, Checksum=0 (ignored)
  0x88, 0x00, 0x00, 0x00,
  // R=0, S=0, O=0
  0x00, 0x00, 0x00, 0x00,
  // Target Address=fe80:: (but only 64-bits)
  0xfe, 0x80, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,
  // Cut short.
};

// Option - Source link-layer - a0:8c:fd:c3:b3:bf
constexpr uint8_t kSourceLinkLayerOptionRaw1[] = {
  // Type=Src LL Addr (1), Length=8 bytes (1), MAC=a0:8c:fd:c3:b3:bf
  0x01, 0x01, 0xa0, 0x8c,
  0xfd, 0xc3, 0xb3, 0xbf
};

const LLAddress kSourceLL1(LLAddress::Type::kEui48, "a0:8c:fd:c3:b3:bf");

// Option - Source link-layer - a0:8c:fd:c3:b3:c0
constexpr uint8_t kSourceLinkLayerOptionRaw2[] = {
  // Type=Src LL Addr (1), Length=8 bytes (1), MAC=a0:8c:fd:c3:b3:c0
  0x01, 0x01, 0xa0, 0x8c,
  0xfd, 0xc3, 0xb3, 0xc0
};

const LLAddress kSourceLL2(LLAddress::Type::kEui48, "a0:8c:fd:c3:b3:c0");

// Bad Option - Source link-layer - 00:00:00:00:00:00 - BAD SIZE
constexpr uint8_t kSourceLinkLayerOptionZeroSizeRaw[] = {
  // Length 0
  0x01, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00
};

// Option - Target link-layer - 32:85:6c:5b:a1:ca
constexpr uint8_t kTargetLinkLayerOptionRaw1[] = {
  // Type=Targ LL Addr (2), Length=8 bytes (1), MAC=32:85:6c:5b:a1:ca
  0x02, 0x01, 0x32, 0x85,
  0x6c, 0x5b, 0xa1, 0xca
};

const LLAddress kTargetLL1(LLAddress::Type::kEui48, "32:85:6c:5b:a1:ca");

// Option - Target link-layer - d4:25:8b:b2:cc:cb
constexpr uint8_t kTargetLinkLayerOptionRaw2[] = {
  // Type=Targ LL Addr (2), Length=8 bytes (1), MAC=d4:25:8b:b2:cc:cb
  0x02, 0x01, 0xd4, 0x25,
  0x8b, 0xb2, 0xcc, 0xcb
};

const LLAddress kTargetLL2(LLAddress::Type::kEui48, "d4:25:8b:b2:cc:cb");

// Warn Option - Target link-layer - 11:22:33:44:55:66:77:88:99 - OVERSIZE
constexpr uint8_t kTargetLinkLayerOptionOverSizeRaw[] = {
  0x02, 0x02, 0x11, 0x22,
  0x33, 0x44, 0x55, 0x66,
  0x77, 0x88, 0x99, 0x00,
  0x00, 0x00, 0x00, 0x00
};

// Option - Prefix Information - 2620:0:1000:1511::/64
//    On-Link, Autonomous, Valid for 72 hours, Preferred for 70 hours
constexpr uint8_t kPrefixOptionRaw1[] = {
  // Type=Prefix (3), Length=32-bytes (4), Prefix Length=64, L=1, A=1
  0x03, 0x04, 0x40, 0xc0,
  // Valid Lifetime= 72 hr (259200 s)
  0x00, 0x03, 0xf4, 0x80,
  // Preferred Lifetime= 70 hr (252000 s)
  0x00, 0x03, 0xd8, 0x60,
  // Reserved2
  0x00, 0x00, 0x00, 0x00,
  // Prefix=2620:0:1000:1511::
  0x26, 0x20, 0x00, 0x00,
  0x10, 0x00, 0x15, 0x11,
  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00
};

constexpr uint8_t kPrefixLength1 = 64;
constexpr bool kOnLinkFlag1 = true;
constexpr bool kAutonomousFlag1 = true;
constexpr TimeDelta kValidLifetime1 = TimeDelta::FromHours(72);
constexpr TimeDelta kPreferredLifetime1 = TimeDelta::FromHours(70);
const IPAddress kPrefix1 = IPAddress("2620:0:1000:1511::");

// Option - Prefix Information - 2401:fa00:480::/48
//    Valid for 24 hours, Preferred for 12 hours
constexpr uint8_t kPrefixOptionRaw2[] = {
  // Type=Prefix (3), Length=32-bytes (4), Prefix Length=48, L=0, A=0
  0x03, 0x04, 0x30, 0x00,
  // Valid Lifetime= 24 hr (86400 s)
  0x00, 0x01, 0x51, 0x80,
  // Preferred Lifetime= 12 hr (43200 s)
  0x00, 0x00, 0xa8, 0xc0,
  // Reserved2
  0x00, 0x00, 0x00, 0x00,
  // Prefix=2401:fa00:480::
  0x24, 0x01, 0xfa, 0x00,
  0x04, 0x80, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00
};

constexpr uint8_t kPrefixLength2 = 48;
constexpr bool kOnLinkFlag2 = false;
constexpr bool kAutonomousFlag2 = false;
constexpr TimeDelta kValidLifetime2 = TimeDelta::FromHours(24);
constexpr TimeDelta kPreferredLifetime2 = TimeDelta::FromHours(12);
const IPAddress kPrefix2 = IPAddress("2401:fa00:480::");


// Option - Redirected Header
//    Length = 32 bytes (payload 24 bytes)
constexpr uint8_t kRedirectedHeader1[] = {
  // Type=Redirected Header (4), Length=32 bytes (4)
  0x04, 0x04, 0x00, 0x00,
  // Reserved
  0x00, 0x00, 0x00, 0x00,
  // Data
  0x49, 0x20, 0x6c, 0x69,
  0x6b, 0x65, 0x20, 0x63,
  0x68, 0x65, 0x65, 0x73,
  0x65, 0x20, 0x6e, 0x20,
  0x63, 0x72, 0x61, 0x63,
  0x6b, 0x65, 0x72, 0x73
};

constexpr uint8_t kIPHeaderAndData1[] = {
  0x49, 0x20, 0x6c, 0x69,
  0x6b, 0x65, 0x20, 0x63,
  0x68, 0x65, 0x65, 0x73,
  0x65, 0x20, 0x6e, 0x20,
  0x63, 0x72, 0x61, 0x63,
  0x6b, 0x65, 0x72, 0x73
};

// Option - MTU - 1500
constexpr uint8_t kMTUOptionRaw1[] = {
  // Type=MTU (5), Length=8 bytes (1)
  0x05, 0x01, 0x00, 0x00,
  // MTU=1500
  0x00, 0x00, 0x05, 0xdc
};

constexpr uint32_t kMTU1 = 1500;

// Option - Unknown (7)
constexpr uint8_t kUnknownOption[] = {
  // Type=7, Length=8 bytes (1)
  0x07, 0x01, 0xde, 0xad,
  0xbe, 0xef, 0x13, 0x37
};

// clang-format on

constexpr NeighborDiscoveryMessage::OptionType kUnknownOptionType = 0x07;

}  // namespace

TEST(NeighborDiscoveryMessageTest, TestEmptyInstance) {
  // Test all the methods of an empty message to ensure that everything
  // is being checked / validated correctly.
  const ByteString empty_packet;
  NeighborDiscoveryMessage message(empty_packet);

  EXPECT_FALSE(message.IsValid());
  EXPECT_EQ(message.GetLength(), 0);

  // Stored types.
  EXPECT_NE(message.type(), NeighborDiscoveryMessage::kTypeRouterSolicit);
  EXPECT_NE(message.type(), NeighborDiscoveryMessage::kTypeRouterAdvert);
  EXPECT_NE(message.type(), NeighborDiscoveryMessage::kTypeNeighborSolicit);
  EXPECT_NE(message.type(), NeighborDiscoveryMessage::kTypeNeighborAdvert);
  EXPECT_NE(message.type(), NeighborDiscoveryMessage::kTypeRedirect);

  EXPECT_FALSE(message.GetChecksum(nullptr));
  uint16_t checksum = 0x5f5f;
  EXPECT_FALSE(message.GetChecksum(&checksum));
  EXPECT_EQ(checksum, 0x5f5f);
  EXPECT_FALSE(message.SetChecksum(checksum));

  // Test all methods related to Type specific data.  All should fail
  // and output parameters should not be changed.

  EXPECT_FALSE(message.GetCurrentHopLimit(nullptr));
  uint8_t cur_hop_limit = 0xff;
  EXPECT_FALSE(message.GetCurrentHopLimit(&cur_hop_limit));
  EXPECT_EQ(0xff, cur_hop_limit);

  EXPECT_FALSE(message.GetManagedAddressConfigurationFlag(nullptr));
  bool managed_flag = true;
  EXPECT_FALSE(message.GetManagedAddressConfigurationFlag(&managed_flag));
  EXPECT_EQ(managed_flag, true);

  EXPECT_FALSE(message.GetOtherConfigurationFlag(nullptr));
  bool other_flag = false;
  EXPECT_FALSE(message.GetOtherConfigurationFlag(&other_flag));
  EXPECT_EQ(other_flag, false);

  EXPECT_FALSE(message.GetProxyFlag(nullptr));
  bool proxy_flag = false;
  EXPECT_FALSE(message.GetProxyFlag(&proxy_flag));
  EXPECT_EQ(proxy_flag, false);
  EXPECT_FALSE(message.SetProxyFlag(true));
  EXPECT_FALSE(message.SetProxyFlag(false));

  EXPECT_FALSE(message.GetRouterLifetime(nullptr));
  TimeDelta router_lifetime = TimeDelta::FromSeconds(8888);
  EXPECT_FALSE(message.GetRouterLifetime(&router_lifetime));
  EXPECT_EQ(router_lifetime.InSeconds(), 8888);

  EXPECT_FALSE(message.GetReachableTime(nullptr));
  TimeDelta reachable_time = TimeDelta::FromMilliseconds(123456);
  EXPECT_FALSE(message.GetReachableTime(&reachable_time));
  EXPECT_EQ(reachable_time.InMilliseconds(), 123456);

  EXPECT_FALSE(message.GetRetransmitTimer(nullptr));
  TimeDelta retransmit_timer = TimeDelta::FromMilliseconds(789456);
  EXPECT_FALSE(message.GetRetransmitTimer(&retransmit_timer));
  EXPECT_EQ(retransmit_timer.InMilliseconds(), 789456);

  EXPECT_FALSE(message.GetTargetAddress(nullptr));
  IPAddress target_address = IPAddress("fe80::");
  EXPECT_FALSE(message.GetTargetAddress(&target_address));
  EXPECT_TRUE(IPAddress("fe80::").Equals(target_address));

  EXPECT_FALSE(message.GetRouterFlag(nullptr));
  bool router_flag = true;
  EXPECT_FALSE(message.GetRouterFlag(&router_flag));
  EXPECT_EQ(router_flag, true);

  EXPECT_FALSE(message.GetSolicitedFlag(nullptr));
  bool solicited_flag = false;
  EXPECT_FALSE(message.GetSolicitedFlag(&solicited_flag));
  EXPECT_EQ(solicited_flag, false);

  EXPECT_FALSE(message.GetOverrideFlag(nullptr));
  bool override_flag = false;
  EXPECT_FALSE(message.GetOverrideFlag(&override_flag));
  EXPECT_EQ(override_flag, false);

  EXPECT_FALSE(message.GetDestinationAddress(nullptr));
  IPAddress destination_address = IPAddress("ff02::");
  EXPECT_FALSE(message.GetDestinationAddress(&destination_address));
  EXPECT_TRUE(IPAddress("ff02::").Equals(destination_address));

  // Test all methods related to Options.  All accessors should fail
  // and output parameters should not be changed.  Get counts and
  // such should be zero.

  // Loop over all possible option types.
  using OptionType = NeighborDiscoveryMessage::OptionType;
  for (uint32_t i = 0; i <= 0xff; i++) {
    const OptionType opt_type = static_cast<OptionType>(i);
    EXPECT_FALSE(message.HasOption(opt_type))
        << "Found an option of type number " << i;
    EXPECT_EQ(message.OptionCount(opt_type), 0)
        << "Found a non-zero option count of type number " << i;
  }

  EXPECT_FALSE(message.HasSourceLinkLayerAddress());
  LLAddress source_ll_address(LLAddress::Type::kEui48, "58:6d:8f:99:e5:be");
  EXPECT_FALSE(message.GetSourceLinkLayerAddress(0, &source_ll_address));
  EXPECT_FALSE(message.GetSourceLinkLayerAddress(1000, &source_ll_address));
  EXPECT_TRUE(LLAddress(LLAddress::Type::kEui48, "58:6d:8f:99:e5:be")
                  .Equals(source_ll_address));

  EXPECT_FALSE(message.HasTargetLinkLayerAddress());
  LLAddress target_ll_address(LLAddress::Type::kEui48, "32:85:6c:5b:a1:ca");
  EXPECT_FALSE(message.GetTargetLinkLayerAddress(0, &target_ll_address));
  EXPECT_FALSE(message.GetTargetLinkLayerAddress(1000, &target_ll_address));
  EXPECT_TRUE(LLAddress(LLAddress::Type::kEui48, "32:85:6c:5b:a1:ca")
                  .Equals(target_ll_address));

  EXPECT_FALSE(message.HasPrefixInformation());
  EXPECT_EQ(message.PrefixInformationCount(), 0);
  uint8_t prefix_length = 0x4f;
  EXPECT_FALSE(message.GetPrefixLength(0, &prefix_length));
  EXPECT_FALSE(message.GetPrefixLength(250, &prefix_length));
  EXPECT_EQ(prefix_length, 0x4f);
  bool on_link_flag = false;
  EXPECT_FALSE(message.GetOnLinkFlag(0, &on_link_flag));
  EXPECT_FALSE(message.GetOnLinkFlag(560, &on_link_flag));
  EXPECT_EQ(on_link_flag, false);
  bool autonomous_flag = true;
  EXPECT_FALSE(
      message.GetAutonomousAddressConfigurationFlag(0, &autonomous_flag));
  EXPECT_FALSE(
      message.GetAutonomousAddressConfigurationFlag(890, &autonomous_flag));
  EXPECT_EQ(autonomous_flag, true);
  TimeDelta valid_lifetime = TimeDelta::FromSeconds(90000);
  EXPECT_FALSE(message.GetPrefixValidLifetime(0, &valid_lifetime));
  EXPECT_FALSE(message.GetPrefixValidLifetime(743, &valid_lifetime));
  EXPECT_EQ(valid_lifetime.InSeconds(), 90000);
  TimeDelta preferred_lifetime = TimeDelta::FromSeconds(85000);
  EXPECT_FALSE(message.GetPrefixPreferredLifetime(0, &preferred_lifetime));
  EXPECT_FALSE(message.GetPrefixPreferredLifetime(123, &preferred_lifetime));
  EXPECT_EQ(preferred_lifetime.InSeconds(), 85000);
  IPAddress prefix = IPAddress("2401:fa00:480:56::");
  EXPECT_FALSE(message.GetPrefix(0, &prefix));
  EXPECT_FALSE(message.GetPrefix(5555, &prefix));
  EXPECT_TRUE(IPAddress("2401:fa00:480:56::").Equals(prefix));

  EXPECT_FALSE(message.HasRedirectedHeader());
  ByteString ip_header_and_data(8);
  EXPECT_FALSE(message.GetIpHeaderAndData(0, &ip_header_and_data));
  EXPECT_FALSE(message.GetIpHeaderAndData(8999, &ip_header_and_data));
  EXPECT_TRUE(ByteString(8).Equals(ip_header_and_data));

  EXPECT_FALSE(message.HasMTU());
  uint32_t mtu = 1280;
  EXPECT_FALSE(message.GetMTU(0, &mtu));
  EXPECT_FALSE(message.GetMTU(14, &mtu));
  EXPECT_EQ(mtu, 1280);
}

TEST(NeighborDiscoveryMessageTest, NonNDMessage) {
  const ByteString nnd_message(kNonNDMessage, sizeof(kNonNDMessage));
  NeighborDiscoveryMessage message(nnd_message);

  EXPECT_FALSE(message.IsValid());
  EXPECT_EQ(message.GetLength(), 0);

  EXPECT_NE(message.type(), NeighborDiscoveryMessage::kTypeRouterSolicit);
  EXPECT_NE(message.type(), NeighborDiscoveryMessage::kTypeRouterAdvert);
  EXPECT_NE(message.type(), NeighborDiscoveryMessage::kTypeNeighborSolicit);
  EXPECT_NE(message.type(), NeighborDiscoveryMessage::kTypeNeighborAdvert);
  EXPECT_NE(message.type(), NeighborDiscoveryMessage::kTypeRedirect);
}

TEST(NeighborDiscoveryMessageTest, BadNDMessageSize) {
  const ByteString bad_nd_message_size(kNAMessageBadSize,
                                       sizeof(kNAMessageBadSize));
  NeighborDiscoveryMessage message(bad_nd_message_size);

  EXPECT_FALSE(message.IsValid());
  EXPECT_EQ(message.GetLength(), 0);
}

TEST(NeighborDiscoveryMessageTest, UnknownOption) {
  ByteString unknown_option_message(kNSMessage1, sizeof(kNSMessage1));

  // Test that this will work without the unknown option.
  NeighborDiscoveryMessage pre_message(unknown_option_message);
  ASSERT_TRUE(pre_message.IsValid())
      << "Cannot test unknown option if failure is in header";

  const ByteString unknown_option(kUnknownOption, sizeof(kUnknownOption));
  unknown_option_message.Append(unknown_option);

  NeighborDiscoveryMessage message(unknown_option_message);

  ASSERT_TRUE(message.IsValid()) << "Unknown option has caused an issue.";

  EXPECT_TRUE(message.HasOption(kUnknownOptionType));
  EXPECT_EQ(message.OptionCount(kUnknownOptionType), 1);

  ByteString recovered_unknown_option;
  EXPECT_TRUE(
      message.GetRawOption(kUnknownOptionType, 0, &recovered_unknown_option));
  EXPECT_TRUE(unknown_option.Equals(recovered_unknown_option));
}

TEST(NeighborDiscoveryMessageTest, ZeroSizeOption) {
  ByteString zero_size_option_message(kRMessage1, sizeof(kRMessage1));

  NeighborDiscoveryMessage pre_message(zero_size_option_message);
  ASSERT_TRUE(pre_message.IsValid())
      << "Cannot test zero option size if failure is in header";

  const ByteString zero_size_option(kSourceLinkLayerOptionZeroSizeRaw,
                                    sizeof(kSourceLinkLayerOptionZeroSizeRaw));
  zero_size_option_message.Append(zero_size_option);

  NeighborDiscoveryMessage message(zero_size_option_message);

  ASSERT_FALSE(message.IsValid())
      << "A zero sized option should had caused a failed.";
  EXPECT_FALSE(message.HasSourceLinkLayerAddress());
}

TEST(NeighborDiscoveryMessageTest, OversizeTargetOption) {
  // Although we may not support arbitary link-layer address
  // types, they should still be recognoized as valid.
  ByteString oversize_option_message(kNAMessage1, sizeof(kNAMessage1));

  NeighborDiscoveryMessage pre_message(oversize_option_message);
  ASSERT_TRUE(pre_message.IsValid())
      << "Cannot test oversize option size if failure is in header";

  const ByteString oversize_option(kTargetLinkLayerOptionOverSizeRaw,
                                   sizeof(kTargetLinkLayerOptionOverSizeRaw));
  oversize_option_message.Append(oversize_option);

  NeighborDiscoveryMessage message(oversize_option_message);

  ASSERT_TRUE(message.IsValid()) << "Oversize option has caused an issue";

  EXPECT_TRUE(message.HasTargetLinkLayerAddress());
  EXPECT_EQ(message.OptionCount(
                NeighborDiscoveryMessage::kOptionTypeTargetLinkLayerAddress),
            1);

  ByteString recovered_oversize_option;
  EXPECT_TRUE(message.GetRawOption(
      NeighborDiscoveryMessage::kOptionTypeTargetLinkLayerAddress, 0,
      &recovered_oversize_option));
  EXPECT_TRUE(oversize_option.Equals(recovered_oversize_option));
}

// Constructor Tests.

TEST(CreateNeighborDiscoveryMessageTest, RouterSolicit) {
  NeighborDiscoveryMessage rs_message =
      NeighborDiscoveryMessage::RouterSolicit();

  ASSERT_TRUE(rs_message.IsValid());
  EXPECT_EQ(rs_message.type(), NeighborDiscoveryMessage::kTypeRouterSolicit);

  // Set and validate checksum.
  uint16_t checksum;
  EXPECT_TRUE(rs_message.GetChecksum(&checksum));
  EXPECT_EQ(checksum, 0);
  EXPECT_TRUE(rs_message.SetChecksum(htons(kRSChecksum1)));
  EXPECT_TRUE(rs_message.GetChecksum(&checksum));
  EXPECT_EQ(checksum, htons(kRSChecksum1));

  // Push options.
  EXPECT_TRUE(rs_message.PushSourceLinkLayerAddress(kSourceLL1));

  // Validate options.
  EXPECT_TRUE(rs_message.HasSourceLinkLayerAddress());
  EXPECT_EQ(rs_message.OptionCount(
                NeighborDiscoveryMessage::kOptionTypeSourceLinkLayerAddress),
            1);

  LLAddress source_ll_address;
  EXPECT_TRUE(rs_message.GetSourceLinkLayerAddress(0, &source_ll_address));
  EXPECT_TRUE(kSourceLL1.Equals(source_ll_address));
}

TEST(CreateNeighborDiscoveryMessageTest, RouterAdvert) {
  // Validate that assemblying the Router Advert results
  // in the same output.
  NeighborDiscoveryMessage ra_message = NeighborDiscoveryMessage::RouterAdvert(
      kRACurHopLimit1, kRAManagedFlag1, kRAOtherFlag1, kRAProxyFlag1,
      kRARouterLifetime1, kRAReachableTime1, kRARetransTimer1);

  ASSERT_TRUE(ra_message.IsValid());
  EXPECT_EQ(ra_message.type(), NeighborDiscoveryMessage::kTypeRouterAdvert);

  // Set and validate checksum.
  uint16_t checksum;
  EXPECT_TRUE(ra_message.GetChecksum(&checksum));
  EXPECT_EQ(checksum, 0);
  EXPECT_TRUE(ra_message.SetChecksum(htons(kRAChecksum1)));
  EXPECT_TRUE(ra_message.GetChecksum(&checksum));
  EXPECT_EQ(checksum, htons(kRAChecksum1));

  // Push options.
  EXPECT_TRUE(ra_message.PushSourceLinkLayerAddress(kSourceLL1));
  EXPECT_TRUE(ra_message.PushMTU(kMTU1));
  EXPECT_TRUE(ra_message.PushPrefixInformation(
      kPrefixLength1, kOnLinkFlag1, kAutonomousFlag1, kValidLifetime1,
      kPreferredLifetime1, kPrefix1));
  EXPECT_TRUE(ra_message.PushPrefixInformation(
      kPrefixLength2, kOnLinkFlag2, kAutonomousFlag2, kValidLifetime2,
      kPreferredLifetime2, kPrefix2));

  // Verify header.
  // Cur Hop Limit.
  uint8_t cur_hop_limit = 0;
  EXPECT_TRUE(ra_message.GetCurrentHopLimit(&cur_hop_limit));
  EXPECT_EQ(cur_hop_limit, kRACurHopLimit1);
  // Managed flag.
  bool managed_flag = !kRAManagedFlag1;
  EXPECT_TRUE(ra_message.GetManagedAddressConfigurationFlag(&managed_flag));
  EXPECT_EQ(managed_flag, kRAManagedFlag1);
  // Other flag.
  bool other_flag = !kRAOtherFlag1;
  EXPECT_TRUE(ra_message.GetOtherConfigurationFlag(&other_flag));
  EXPECT_EQ(other_flag, kRAOtherFlag1);
  // Proxy flag.
  bool proxy_flag = !kRAProxyFlag1;
  EXPECT_TRUE(ra_message.GetProxyFlag(&proxy_flag));
  EXPECT_EQ(proxy_flag, kRAProxyFlag1);
  // Router Lifetime.
  TimeDelta router_lifetime;
  EXPECT_TRUE(ra_message.GetRouterLifetime(&router_lifetime));
  EXPECT_EQ(router_lifetime, kRARouterLifetime1);
  // Reachable Time.
  TimeDelta reachable_time;
  EXPECT_TRUE(ra_message.GetReachableTime(&reachable_time));
  EXPECT_EQ(reachable_time, kRAReachableTime1);
  // Retrans Timer.
  TimeDelta retransmit_timer;
  EXPECT_TRUE(ra_message.GetRetransmitTimer(&retransmit_timer));
  EXPECT_EQ(retransmit_timer, kRARetransTimer1);

  // Verify options.
  // Source LL option.
  EXPECT_TRUE(ra_message.HasSourceLinkLayerAddress());
  EXPECT_EQ(ra_message.OptionCount(
                NeighborDiscoveryMessage::kOptionTypeSourceLinkLayerAddress),
            1);
  LLAddress source_ll;
  EXPECT_TRUE(ra_message.GetSourceLinkLayerAddress(0, &source_ll));
  EXPECT_TRUE(kSourceLL1.Equals(source_ll));

  // MTU option.
  EXPECT_TRUE(ra_message.HasMTU());
  EXPECT_EQ(ra_message.OptionCount(NeighborDiscoveryMessage::kOptionTypeMTU),
            1);
  uint32_t mtu;
  EXPECT_TRUE(ra_message.GetMTU(0, &mtu));
  EXPECT_EQ(mtu, kMTU1);

  // Prefix option.
  EXPECT_TRUE(ra_message.HasPrefixInformation());
  EXPECT_EQ(ra_message.OptionCount(
                NeighborDiscoveryMessage::kOptionTypePrefixInformation),
            2);
  EXPECT_EQ(ra_message.PrefixInformationCount(), 2);

  // Prefix option 1.
  // Prefix length.
  uint8_t prefix_length;
  EXPECT_TRUE(ra_message.GetPrefixLength(0, &prefix_length));
  EXPECT_EQ(prefix_length, kPrefixLength1);
  // On-Link flag.
  bool on_link_flag;
  EXPECT_TRUE(ra_message.GetOnLinkFlag(0, &on_link_flag));
  EXPECT_EQ(on_link_flag, kOnLinkFlag1);
  // Autonomous config flag.
  bool autonomous_flag;
  EXPECT_TRUE(
      ra_message.GetAutonomousAddressConfigurationFlag(0, &autonomous_flag));
  EXPECT_EQ(autonomous_flag, kAutonomousFlag1);
  // Valid lifetime.
  TimeDelta valid_lifetime;
  EXPECT_TRUE(ra_message.GetPrefixValidLifetime(0, &valid_lifetime));
  EXPECT_EQ(valid_lifetime, kValidLifetime1);
  // Preferred lifetime.
  TimeDelta preferred_lifetime;
  EXPECT_TRUE(ra_message.GetPrefixPreferredLifetime(0, &preferred_lifetime));
  EXPECT_EQ(preferred_lifetime, kPreferredLifetime1);
  // Prefix.
  IPAddress prefix;
  EXPECT_TRUE(ra_message.GetPrefix(0, &prefix));
  EXPECT_TRUE(kPrefix1.Equals(prefix));

  // Prefix option 2.
  // Prefix length.
  EXPECT_TRUE(ra_message.GetPrefixLength(1, &prefix_length));
  EXPECT_EQ(prefix_length, kPrefixLength2);
  // On-Link flag.
  EXPECT_TRUE(ra_message.GetOnLinkFlag(1, &on_link_flag));
  EXPECT_EQ(on_link_flag, kOnLinkFlag2);
  // Autonomous config flag.
  EXPECT_TRUE(
      ra_message.GetAutonomousAddressConfigurationFlag(1, &autonomous_flag));
  EXPECT_EQ(autonomous_flag, kAutonomousFlag2);
  // Valid lifetime.
  EXPECT_TRUE(ra_message.GetPrefixValidLifetime(1, &valid_lifetime));
  EXPECT_EQ(valid_lifetime, kValidLifetime2);
  // Preferred lifetime.
  EXPECT_TRUE(ra_message.GetPrefixPreferredLifetime(1, &preferred_lifetime));
  EXPECT_EQ(preferred_lifetime, kPreferredLifetime2);
  // Prefix.
  EXPECT_TRUE(ra_message.GetPrefix(1, &prefix));
  EXPECT_TRUE(kPrefix2.Equals(prefix));
}

TEST(CreateNeighborDiscoveryMessageTest, NeighborSolicit) {
  // Validate that assemblying the Neighbor Solicitation results
  // in the same output.
  NeighborDiscoveryMessage ns_message =
      NeighborDiscoveryMessage::NeighborSolicit(kNSTargetAddress1);

  ASSERT_TRUE(ns_message.IsValid());
  EXPECT_EQ(ns_message.type(), NeighborDiscoveryMessage::kTypeNeighborSolicit);

  // Set and validate checksum.
  uint16_t checksum;
  EXPECT_TRUE(ns_message.GetChecksum(&checksum));
  EXPECT_EQ(checksum, 0);
  EXPECT_TRUE(ns_message.SetChecksum(htons(kNSChecksum1)));
  EXPECT_TRUE(ns_message.GetChecksum(&checksum));
  EXPECT_EQ(checksum, htons(kNSChecksum1));

  // Push opotions.
  EXPECT_TRUE(ns_message.PushSourceLinkLayerAddress(kSourceLL2));

  // Validate header.
  IPAddress target_address;
  EXPECT_TRUE(ns_message.GetTargetAddress(&target_address));

  // Validate options.
  EXPECT_TRUE(ns_message.HasSourceLinkLayerAddress());
  EXPECT_EQ(ns_message.OptionCount(
                NeighborDiscoveryMessage::kOptionTypeSourceLinkLayerAddress),
            1);

  LLAddress source_ll_address;
  EXPECT_TRUE(ns_message.GetSourceLinkLayerAddress(0, &source_ll_address));
  EXPECT_TRUE(kSourceLL2.Equals(source_ll_address));
}

TEST(CreateNeighborDiscoveryMessageTest, NeighborAdvert) {
  // Validate that assemblying the Neighbor Advertisement results
  // in the same output.
  NeighborDiscoveryMessage na_message =
      NeighborDiscoveryMessage::NeighborAdvert(
          kNARouterFlag1, kNASolicitedFlag1, kNAOverrideFlag1,
          kNATargetAddress1);

  ASSERT_TRUE(na_message.IsValid());
  EXPECT_EQ(na_message.type(), NeighborDiscoveryMessage::kTypeNeighborAdvert);

  // Set and validate checksum.
  uint16_t checksum;
  EXPECT_TRUE(na_message.GetChecksum(&checksum));
  EXPECT_EQ(checksum, 0);
  EXPECT_TRUE(na_message.SetChecksum(htons(kNAChecksum1)));
  EXPECT_TRUE(na_message.GetChecksum(&checksum));
  EXPECT_EQ(checksum, htons(kNAChecksum1));

  // Push options.
  EXPECT_TRUE(na_message.PushTargetLinkLayerAddress(kTargetLL1));

  // Validate header.
  // Router flag.
  bool router_flag = !kNARouterFlag1;
  EXPECT_TRUE(na_message.GetRouterFlag(&router_flag));
  EXPECT_EQ(router_flag, kNARouterFlag1);
  // Solicited flag.
  bool solicited_flag = !kNASolicitedFlag1;
  EXPECT_TRUE(na_message.GetSolicitedFlag(&solicited_flag));
  EXPECT_EQ(solicited_flag, kNASolicitedFlag1);
  // Override flag.
  bool override_flag = !kNAOverrideFlag1;
  EXPECT_TRUE(na_message.GetOverrideFlag(&override_flag));
  EXPECT_EQ(override_flag, kNAOverrideFlag1);
  // Target address.
  IPAddress target_address;
  EXPECT_TRUE(na_message.GetTargetAddress(&target_address));
  EXPECT_TRUE(kNATargetAddress1.Equals(target_address));

  // Validate options.
  EXPECT_TRUE(na_message.HasTargetLinkLayerAddress());
  EXPECT_EQ(na_message.OptionCount(
                NeighborDiscoveryMessage::kOptionTypeTargetLinkLayerAddress),
            1);
  LLAddress target_ll_address;
  EXPECT_TRUE(na_message.GetTargetLinkLayerAddress(0, &target_ll_address));
  EXPECT_TRUE(kTargetLL1.Equals(target_ll_address));
}

TEST(CreateNeighborDiscoveryMessageTest, Redirect) {
  // Validate that assemblying the Redirect results
  // in the same output.
  NeighborDiscoveryMessage rd_message = NeighborDiscoveryMessage::Redirect(
      kRTargetAddress1, kRDestinationAddress1);

  ASSERT_TRUE(rd_message.IsValid());
  EXPECT_EQ(rd_message.type(), NeighborDiscoveryMessage::kTypeRedirect);

  // Set and validate checksum.
  uint16_t checksum;
  EXPECT_TRUE(rd_message.GetChecksum(&checksum));
  EXPECT_EQ(checksum, 0);
  EXPECT_TRUE(rd_message.SetChecksum(htons(kRChecksum1)));
  EXPECT_TRUE(rd_message.GetChecksum(&checksum));
  EXPECT_EQ(checksum, htons(kRChecksum1));

  // Push options.
  EXPECT_TRUE(rd_message.PushTargetLinkLayerAddress(kTargetLL2));
  const ByteString expected_ip_header_and_data(kIPHeaderAndData1,
                                               sizeof(kIPHeaderAndData1));
  EXPECT_TRUE(rd_message.PushRedirectedHeader(expected_ip_header_and_data));

  // Validate header.
  // Target address.
  IPAddress target_address;
  EXPECT_TRUE(rd_message.GetTargetAddress(&target_address));
  EXPECT_TRUE(kRTargetAddress1.Equals(target_address));
  // Destination address.
  IPAddress destination_address;
  EXPECT_TRUE(rd_message.GetDestinationAddress(&destination_address));
  EXPECT_TRUE(kRDestinationAddress1.Equals(destination_address));

  // Validate options.
  // Target link-layer option.
  EXPECT_TRUE(rd_message.HasTargetLinkLayerAddress());
  EXPECT_EQ(rd_message.OptionCount(
                NeighborDiscoveryMessage::kOptionTypeTargetLinkLayerAddress),
            1);
  LLAddress target_ll_address;
  EXPECT_TRUE(rd_message.GetTargetLinkLayerAddress(0, &target_ll_address));
  EXPECT_TRUE(kTargetLL2.Equals(target_ll_address));
  // Redirected header option.
  EXPECT_TRUE(rd_message.HasRedirectedHeader());
  EXPECT_EQ(rd_message.OptionCount(
                NeighborDiscoveryMessage::kOptionTypeRedirectHeader),
            1);
  ByteString ip_header_and_data;
  EXPECT_TRUE(rd_message.GetIpHeaderAndData(0, &ip_header_and_data));
  EXPECT_TRUE(expected_ip_header_and_data.Equals(ip_header_and_data));
}

TEST(ModifyNeighborDiscoveryMessageTest, SetSourceLinkLayerOption) {
  // Create a base RS message.
  NeighborDiscoveryMessage rs_message =
      NeighborDiscoveryMessage::RouterSolicit();
  EXPECT_TRUE(rs_message.IsValid());

  // Push option.
  EXPECT_TRUE(rs_message.PushSourceLinkLayerAddress(kSourceLL1));

  // Modify option.
  ASSERT_TRUE(rs_message.HasSourceLinkLayerAddress());
  EXPECT_TRUE(rs_message.SetSourceLinkLayerAddress(0, kSourceLL2));

  // Verify modification.
  EXPECT_EQ(rs_message.OptionCount(
                NeighborDiscoveryMessage::kOptionTypeSourceLinkLayerAddress),
            1);

  LLAddress source_ll;
  EXPECT_TRUE(rs_message.GetSourceLinkLayerAddress(0, &source_ll));

  EXPECT_TRUE(source_ll.IsValid());
  EXPECT_TRUE(kSourceLL2.Equals(source_ll));
}

TEST(ModifyNeighborDiscoveryMessageTest, SetTargetLinkLayerOption) {
  NeighborDiscoveryMessage na_message =
      NeighborDiscoveryMessage::NeighborAdvert(
          kNARouterFlag1, kNASolicitedFlag1, kNAOverrideFlag1,
          kNATargetAddress1);

  EXPECT_TRUE(na_message.IsValid());

  // Push options.
  EXPECT_TRUE(na_message.PushTargetLinkLayerAddress(kTargetLL1));

  // Modify option.
  ASSERT_TRUE(na_message.HasTargetLinkLayerAddress());
  EXPECT_TRUE(na_message.SetTargetLinkLayerAddress(0, kTargetLL2));

  // Verify modification.
  EXPECT_EQ(na_message.OptionCount(
                NeighborDiscoveryMessage::kOptionTypeTargetLinkLayerAddress),
            1);

  LLAddress target_ll;
  EXPECT_TRUE(na_message.GetTargetLinkLayerAddress(0, &target_ll));

  EXPECT_TRUE(target_ll.IsValid());
  EXPECT_TRUE(kTargetLL2.Equals(target_ll));
}

TEST(ModifyNeighborDiscoveryMessageTest, SetProxyFlag) {
  NeighborDiscoveryMessage ra_message = NeighborDiscoveryMessage::RouterAdvert(
      kRACurHopLimit1, kRAManagedFlag1, kRAOtherFlag1, kRAProxyFlag1,
      kRARouterLifetime1, kRAReachableTime1, kRARetransTimer1);

  EXPECT_TRUE(ra_message.IsValid());

  // Modify flag.
  EXPECT_TRUE(ra_message.SetProxyFlag(!kRAProxyFlag1));

  // Verify change to proxy flag.
  bool proxy_flag;
  EXPECT_TRUE(ra_message.GetProxyFlag(&proxy_flag));
  EXPECT_EQ(proxy_flag, !kRAProxyFlag1);

  // Verify no changes to non-proxy flags.
  bool managed_flag;
  EXPECT_TRUE(ra_message.GetManagedAddressConfigurationFlag(&managed_flag));
  EXPECT_EQ(managed_flag, kRAManagedFlag1);
  bool other_flag;
  EXPECT_TRUE(ra_message.GetOtherConfigurationFlag(&other_flag));
  EXPECT_EQ(other_flag, kRAOtherFlag1);

  // Modify back.
  EXPECT_TRUE(ra_message.SetProxyFlag(kRAProxyFlag1));

  // Verify change back.
  EXPECT_TRUE(ra_message.GetProxyFlag(&proxy_flag));
  EXPECT_EQ(proxy_flag, kRAProxyFlag1);

  // Verify no changes to non-proxy flags.
  EXPECT_TRUE(ra_message.GetManagedAddressConfigurationFlag(&managed_flag));
  EXPECT_EQ(managed_flag, kRAManagedFlag1);
  EXPECT_TRUE(ra_message.GetOtherConfigurationFlag(&other_flag));
  EXPECT_EQ(other_flag, kRAOtherFlag1);
}

// Router Solicitation Test.

class ParsedRouterSolicitationTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Start with the base message.
    ByteString rs_packet(kRSMessage1, sizeof(kRSMessage1));

    // Add a source link-layer option.
    const ByteString source_ll_option(kSourceLinkLayerOptionRaw1,
                                      sizeof(kSourceLinkLayerOptionRaw1));
    rs_packet.Append(source_ll_option);

    // Parse.
    rs_message_ = NeighborDiscoveryMessage(rs_packet);
  }

  NeighborDiscoveryMessage rs_message_;
};

TEST_F(ParsedRouterSolicitationTest, HeaderCorrect) {
  ASSERT_TRUE(rs_message_.IsValid()) << "RS Message is invalid.";

  // Verify the RS header was parsed corretly.
  EXPECT_EQ(rs_message_.type(), NeighborDiscoveryMessage::kTypeRouterSolicit);

  // Verify checksum.
  uint16_t checksum;
  EXPECT_TRUE(rs_message_.GetChecksum(&checksum));
  EXPECT_EQ(checksum, htons(kRSChecksum1));
}

TEST_F(ParsedRouterSolicitationTest, SourceLinkLayerOptionCorrect) {
  // Verify the options were parsed correctly.
  ASSERT_TRUE(rs_message_.HasSourceLinkLayerAddress());

  EXPECT_EQ(rs_message_.OptionCount(
                NeighborDiscoveryMessage::kOptionTypeSourceLinkLayerAddress),
            1);

  // Check that the indexed option bytes are the correct values.
  ByteString raw_source_ll_option;
  EXPECT_TRUE(rs_message_.GetRawOption(
      NeighborDiscoveryMessage::kOptionTypeSourceLinkLayerAddress, 0,
      &raw_source_ll_option));
  const ByteString original_source_ll_option(
      kSourceLinkLayerOptionRaw1, sizeof(kSourceLinkLayerOptionRaw1));
  EXPECT_TRUE(raw_source_ll_option.Equals(original_source_ll_option));

  // Getting option.
  LLAddress source_ll;
  EXPECT_TRUE(rs_message_.GetSourceLinkLayerAddress(0, &source_ll));

  EXPECT_TRUE(source_ll.IsValid());
  EXPECT_TRUE(kSourceLL1.Equals(source_ll));
}

// Router Advertisement Test.

class ParsedRouterAdvertisementTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ByteString ra_packet(kRAMessage1, sizeof(kRAMessage1));

    // Add a source link-layer option.
    const ByteString source_ll_option(kSourceLinkLayerOptionRaw1,
                                      sizeof(kSourceLinkLayerOptionRaw1));
    ra_packet.Append(source_ll_option);

    // Add an MTU option.
    const ByteString mtu_option(kMTUOptionRaw1, sizeof(kMTUOptionRaw1));
    ra_packet.Append(mtu_option);

    // Add two prefix information options.
    const ByteString prefix_info_option_1(kPrefixOptionRaw1,
                                          sizeof(kPrefixOptionRaw1));
    const ByteString prefix_info_option_2(kPrefixOptionRaw2,
                                          sizeof(kPrefixOptionRaw2));
    ra_packet.Append(prefix_info_option_1);
    ra_packet.Append(prefix_info_option_2);

    ra_message_ = NeighborDiscoveryMessage(ra_packet);
  }

  NeighborDiscoveryMessage ra_message_;
};

TEST_F(ParsedRouterAdvertisementTest, HeaderCorrect) {
  ASSERT_TRUE(ra_message_.IsValid()) << "RA Message is invalid.";

  // Verify the header was parsed correctly.
  EXPECT_EQ(ra_message_.type(), NeighborDiscoveryMessage::kTypeRouterAdvert);
  // Verify checksum.
  uint16_t checksum;
  EXPECT_TRUE(ra_message_.GetChecksum(&checksum));
  EXPECT_EQ(checksum, htons(kRAChecksum1));
  // Cur Hop Limit.
  uint8_t cur_hop_limit = 0;
  EXPECT_TRUE(ra_message_.GetCurrentHopLimit(&cur_hop_limit));
  EXPECT_EQ(cur_hop_limit, kRACurHopLimit1);
  // Managed flag.
  bool managed_flag = !kRAManagedFlag1;
  EXPECT_TRUE(ra_message_.GetManagedAddressConfigurationFlag(&managed_flag));
  EXPECT_EQ(managed_flag, kRAManagedFlag1);
  // Other flag.
  bool other_flag = !kRAOtherFlag1;
  EXPECT_TRUE(ra_message_.GetOtherConfigurationFlag(&other_flag));
  EXPECT_EQ(other_flag, kRAOtherFlag1);
  // Proxy flag.
  bool proxy_flag = !kRAProxyFlag1;
  EXPECT_TRUE(ra_message_.GetProxyFlag(&proxy_flag));
  EXPECT_EQ(proxy_flag, kRAProxyFlag1);
  // Router Lifetime.
  TimeDelta router_lifetime;
  EXPECT_TRUE(ra_message_.GetRouterLifetime(&router_lifetime));
  EXPECT_EQ(router_lifetime, kRARouterLifetime1);
  // Reachable Time.
  TimeDelta reachable_time;
  EXPECT_TRUE(ra_message_.GetReachableTime(&reachable_time));
  EXPECT_EQ(reachable_time, kRAReachableTime1);
  // Retrans Timer.
  TimeDelta retransmit_timer;
  EXPECT_TRUE(ra_message_.GetRetransmitTimer(&retransmit_timer));
  EXPECT_EQ(retransmit_timer, kRARetransTimer1);
}

TEST_F(ParsedRouterAdvertisementTest, SourceLinkLayerOptionCorrect) {
  // Source link-layer option.
  ASSERT_TRUE(ra_message_.HasSourceLinkLayerAddress());

  EXPECT_EQ(ra_message_.OptionCount(
                NeighborDiscoveryMessage::kOptionTypeSourceLinkLayerAddress),
            1);
  LLAddress source_ll;
  EXPECT_TRUE(ra_message_.GetSourceLinkLayerAddress(0, &source_ll));
  EXPECT_TRUE(kSourceLL1.Equals(source_ll));
}

TEST_F(ParsedRouterAdvertisementTest, MTUOptionCorrect) {
  // MTU option.
  ASSERT_TRUE(ra_message_.HasMTU());

  EXPECT_EQ(ra_message_.OptionCount(NeighborDiscoveryMessage::kOptionTypeMTU),
            1);
  uint32_t mtu;
  EXPECT_TRUE(ra_message_.GetMTU(0, &mtu));
  EXPECT_EQ(mtu, kMTU1);
}

TEST_F(ParsedRouterAdvertisementTest, PrefixOptionCorrect) {
  // Prefix options.
  ASSERT_TRUE(ra_message_.HasPrefixInformation());
  ASSERT_EQ(ra_message_.OptionCount(
                NeighborDiscoveryMessage::kOptionTypePrefixInformation),
            2);
  EXPECT_EQ(ra_message_.PrefixInformationCount(), 2);

  // Prefix option 1.
  // Prefix length.
  uint8_t prefix_length;
  EXPECT_TRUE(ra_message_.GetPrefixLength(0, &prefix_length));
  EXPECT_EQ(prefix_length, kPrefixLength1);
  // On-Link flag.
  bool on_link_flag;
  EXPECT_TRUE(ra_message_.GetOnLinkFlag(0, &on_link_flag));
  EXPECT_EQ(on_link_flag, kOnLinkFlag1);
  // Autonomous config flag.
  bool autonomous_flag;
  EXPECT_TRUE(
      ra_message_.GetAutonomousAddressConfigurationFlag(0, &autonomous_flag));
  EXPECT_EQ(autonomous_flag, kAutonomousFlag1);
  // Valid lifetime.
  TimeDelta valid_lifetime;
  EXPECT_TRUE(ra_message_.GetPrefixValidLifetime(0, &valid_lifetime));
  EXPECT_EQ(valid_lifetime, kValidLifetime1);
  // Preferred lifetime.
  TimeDelta preferred_lifetime;
  EXPECT_TRUE(ra_message_.GetPrefixPreferredLifetime(0, &preferred_lifetime));
  EXPECT_EQ(preferred_lifetime, kPreferredLifetime1);
  // Prefix.
  IPAddress prefix;
  EXPECT_TRUE(ra_message_.GetPrefix(0, &prefix));
  EXPECT_TRUE(kPrefix1.Equals(prefix));

  // Prefix option 2.
  // Prefix length.
  EXPECT_TRUE(ra_message_.GetPrefixLength(1, &prefix_length));
  EXPECT_EQ(prefix_length, kPrefixLength2);
  // On-Link flag.
  EXPECT_TRUE(ra_message_.GetOnLinkFlag(1, &on_link_flag));
  EXPECT_EQ(on_link_flag, kOnLinkFlag2);
  // Autonomous config flag.
  EXPECT_TRUE(
      ra_message_.GetAutonomousAddressConfigurationFlag(1, &autonomous_flag));
  EXPECT_EQ(autonomous_flag, kAutonomousFlag2);
  // Valid lifetime.
  EXPECT_TRUE(ra_message_.GetPrefixValidLifetime(1, &valid_lifetime));
  EXPECT_EQ(valid_lifetime, kValidLifetime2);
  // Preferred lifetime.
  EXPECT_TRUE(ra_message_.GetPrefixPreferredLifetime(1, &preferred_lifetime));
  EXPECT_EQ(preferred_lifetime, kPreferredLifetime2);
  // Prefix.
  EXPECT_TRUE(ra_message_.GetPrefix(1, &prefix));
  EXPECT_TRUE(kPrefix2.Equals(prefix));
}

// Neighbor Solicitation Test.

class ParsedNeighborSolicitTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Start with the base message.
    ByteString ns_packet(kNSMessage1, sizeof(kNSMessage1));

    // Add a source link-layer option.
    const ByteString source_ll_option(kSourceLinkLayerOptionRaw2,
                                      sizeof(kSourceLinkLayerOptionRaw2));
    ns_packet.Append(source_ll_option);

    ns_message_ = NeighborDiscoveryMessage(ns_packet);
  }

  NeighborDiscoveryMessage ns_message_;
};

TEST_F(ParsedNeighborSolicitTest, HeaderCorrect) {
  ASSERT_TRUE(ns_message_.IsValid()) << "NS Message is invalid.";

  // Verify the header was parsed corretly.
  EXPECT_EQ(ns_message_.type(), NeighborDiscoveryMessage::kTypeNeighborSolicit);
  // Verify checksum.
  uint16_t checksum;
  EXPECT_TRUE(ns_message_.GetChecksum(&checksum));
  EXPECT_EQ(checksum, htons(kNSChecksum1));
  // Target address.
  IPAddress target_address;
  EXPECT_TRUE(ns_message_.GetTargetAddress(&target_address));
  EXPECT_TRUE(kNSTargetAddress1.Equals(target_address));
}

TEST_F(ParsedNeighborSolicitTest, SourceLinkLayerOptionCorrect) {
  // Source link-layer option.
  ASSERT_TRUE(ns_message_.HasSourceLinkLayerAddress());

  EXPECT_EQ(ns_message_.OptionCount(
                NeighborDiscoveryMessage::kOptionTypeSourceLinkLayerAddress),
            1);
  LLAddress source_ll;
  EXPECT_TRUE(ns_message_.GetSourceLinkLayerAddress(0, &source_ll));
  EXPECT_TRUE(kSourceLL2.Equals(source_ll));
}

// Neighbor Adertisement Test.

class ParsedNeighborAdvertTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Start with the base message.
    ByteString na_packet(kNAMessage1, sizeof(kNAMessage1));

    // Add a target link-layer option.
    const ByteString target_ll_option(kTargetLinkLayerOptionRaw1,
                                      sizeof(kTargetLinkLayerOptionRaw1));
    na_packet.Append(target_ll_option);

    na_message_ = NeighborDiscoveryMessage(na_packet);
  }

  NeighborDiscoveryMessage na_message_;
};

TEST_F(ParsedNeighborAdvertTest, HeaderCorrect) {
  ASSERT_TRUE(na_message_.IsValid()) << "NA Message is invalid.";

  EXPECT_EQ(na_message_.type(), NeighborDiscoveryMessage::kTypeNeighborAdvert);
  // Verify checksum.
  uint16_t checksum;
  EXPECT_TRUE(na_message_.GetChecksum(&checksum));
  EXPECT_EQ(checksum, htons(kNAChecksum1));
  // Router flag.
  bool router_flag = !kNARouterFlag1;
  EXPECT_TRUE(na_message_.GetRouterFlag(&router_flag));
  EXPECT_EQ(router_flag, kNARouterFlag1);
  // Solicited flag.
  bool solicited_flag = !kNASolicitedFlag1;
  EXPECT_TRUE(na_message_.GetSolicitedFlag(&solicited_flag));
  EXPECT_EQ(solicited_flag, kNASolicitedFlag1);
  // Override flag.
  bool override_flag = !kNAOverrideFlag1;
  EXPECT_TRUE(na_message_.GetOverrideFlag(&override_flag));
  EXPECT_EQ(override_flag, kNAOverrideFlag1);
  // Target address.
  IPAddress target_address;
  EXPECT_TRUE(na_message_.GetTargetAddress(&target_address));
  EXPECT_TRUE(kNATargetAddress1.Equals(target_address));
}

TEST_F(ParsedNeighborAdvertTest, TargetLinkLayerOptionCorrect) {
  // Target link-layer option.
  ASSERT_TRUE(na_message_.HasTargetLinkLayerAddress());

  EXPECT_EQ(na_message_.OptionCount(
                NeighborDiscoveryMessage::kOptionTypeTargetLinkLayerAddress),
            1);
  LLAddress target_ll_address;
  EXPECT_TRUE(na_message_.GetTargetLinkLayerAddress(0, &target_ll_address));
  EXPECT_TRUE(kTargetLL1.Equals(target_ll_address));
}

// Redirect test.

class ParsedRedirectTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Start with the base message.
    ByteString rd_packet(kRMessage1, sizeof(kRMessage1));

    // Add a target link-layer option.
    const ByteString target_ll_option(kTargetLinkLayerOptionRaw2,
                                      sizeof(kTargetLinkLayerOptionRaw2));
    rd_packet.Append(target_ll_option);

    // Add a redirected header option.
    const ByteString redirected_header_option(kRedirectedHeader1,
                                              sizeof(kRedirectedHeader1));
    rd_packet.Append(redirected_header_option);

    rd_message_ = NeighborDiscoveryMessage(rd_packet);
  }

  NeighborDiscoveryMessage rd_message_;
};

TEST_F(ParsedRedirectTest, HeaderCorrect) {
  ASSERT_TRUE(rd_message_.IsValid()) << "RD Message is invalid.";

  // Verify the header was parsed corretly.
  EXPECT_EQ(rd_message_.type(), NeighborDiscoveryMessage::kTypeRedirect);
  // Verify checksum.
  uint16_t checksum;
  EXPECT_TRUE(rd_message_.GetChecksum(&checksum));
  EXPECT_EQ(checksum, htons(kRChecksum1));
  // Target address.
  IPAddress target_address;
  EXPECT_TRUE(rd_message_.GetTargetAddress(&target_address));
  EXPECT_TRUE(kRTargetAddress1.Equals(target_address));
  // Destination address.
  IPAddress destination_address;
  EXPECT_TRUE(rd_message_.GetDestinationAddress(&destination_address));
  EXPECT_TRUE(kRDestinationAddress1.Equals(destination_address));
}

TEST_F(ParsedRedirectTest, TargetLinkLayerOptionCorrect) {
  // Target link-layer option.
  ASSERT_TRUE(rd_message_.HasTargetLinkLayerAddress());

  EXPECT_EQ(rd_message_.OptionCount(
                NeighborDiscoveryMessage::kOptionTypeTargetLinkLayerAddress),
            1);

  LLAddress target_ll_address;
  EXPECT_TRUE(rd_message_.GetTargetLinkLayerAddress(0, &target_ll_address));
  EXPECT_TRUE(kTargetLL2.Equals(target_ll_address));
}

TEST_F(ParsedRedirectTest, RedirectedHeaderOptionCorrect) {
  // Redirected header option.
  ASSERT_TRUE(rd_message_.HasRedirectedHeader());

  EXPECT_EQ(rd_message_.OptionCount(
                NeighborDiscoveryMessage::kOptionTypeRedirectHeader),
            1);
  ByteString ip_header_and_data;
  EXPECT_TRUE(rd_message_.GetIpHeaderAndData(0, &ip_header_and_data));
  const ByteString expected_ip_header_and_data(kIPHeaderAndData1,
                                               sizeof(kIPHeaderAndData1));
  EXPECT_TRUE(expected_ip_header_and_data.Equals(ip_header_and_data));
}

}  // namespace portier
