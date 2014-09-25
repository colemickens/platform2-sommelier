// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>
#include <string>
#include <utility>

#include <gtest/gtest.h>

#include "shill/byte_string.h"
#include "shill/error.h"
#include "shill/ip_address.h"
#include "shill/ip_address_store.h"
#include "shill/nl80211_message.h"
#include "shill/testing.h"
#include "shill/wake_on_wifi.h"

using std::set;
using std::string;

namespace shill {

namespace {

// Zero-byte pattern prefixes to match the offsetting bytes in the Ethernet
// frame that lie before the source IP address field.
const uint8_t kIPV4PatternPrefix[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                      0x00, 0x00, 0x00, 0x00, 0x00};
const uint8_t kIPV6PatternPrefix[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

// These masks have bits set to 1 to match bytes in an IP address pattern that
// represent the source IP address of the frame. They are padded with zero
// bits in front to ignore the frame offset and at the end to byte-align the
// mask itself.
const uint8_t kIPV4MaskBytes[] = {0x00, 0x00, 0x00, 0x3c};
const uint8_t kIPV6MaskBytes[] = {0x00, 0x00, 0xc0, 0xff, 0x3f};

const char kIPV4Address0[] = "192.168.10.20";
const uint8_t kIPV4Address0Bytes[] = {0xc0, 0xa8, 0x0a, 0x14};
const char kIPV4Address1[] = "1.2.3.4";
const uint8_t kIPV4Address1Bytes[] = {0x01, 0x02, 0x03, 0x04};

const char kIPV6Address0[] = "FEDC:BA98:7654:3210:FEDC:BA98:7654:3210";
const uint8_t kIPV6Address0Bytes[] = {0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54,
                                      0x32, 0x10, 0xfe, 0xdc, 0xba, 0x98,
                                      0x76, 0x54, 0x32, 0x10};
const char kIPV6Address1[] = "1080:0:0:0:8:800:200C:417A";
const uint8_t kIPV6Address1Bytes[] = {0x10, 0x80, 0x00, 0x00, 0x00, 0x00,
                                      0x00, 0x00, 0x00, 0x08, 0x08, 0x00,
                                      0x20, 0x0c, 0x41, 0x7a};
const char kIPV6Address2[] = "1080::8:800:200C:417A";
const uint8_t kIPV6Address2Bytes[] = {0x10, 0x80, 0x00, 0x00, 0x00, 0x00,
                                      0x00, 0x00, 0x00, 0x08, 0x08, 0x00,
                                      0x20, 0x0c, 0x41, 0x7a};
const char kIPV6Address3[] = "FF01::101";
const uint8_t kIPV6Address3Bytes[] = {0xff, 0x01, 0x00, 0x00, 0x00, 0x00,
                                      0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                      0x00, 0x00, 0x01, 0x01};
const char kIPV6Address4[] = "::1";
const uint8_t kIPV6Address4Bytes[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                      0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                      0x00, 0x00, 0x00, 0x01};
const char kIPV6Address5[] = "::";
const uint8_t kIPV6Address5Bytes[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                      0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                      0x00, 0x00, 0x00, 0x00};
const char kIPV6Address6[] = "0:0:0:0:0:FFFF:129.144.52.38";
const uint8_t kIPV6Address6Bytes[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                      0x00, 0x00, 0x00, 0x00, 0xff, 0xff,
                                      0x81, 0x90, 0x34, 0x26};
const char kIPV6Address7[] = "::DEDE:190.144.52.38";
const uint8_t kIPV6Address7Bytes[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                      0x00, 0x00, 0x00, 0x00, 0xde, 0xde,
                                      0xbe, 0x90, 0x34, 0x26};

// These blobs represent NL80211 messages from the kernel reporting the NIC's
// wake-on-packet settings, sent in response to NL80211_CMD_GET_WOWLAN requests.
const uint8_t kResponseNoIPAddresses[] = {
    0x14, 0x00, 0x00, 0x00, 0x14, 0x00, 0x01, 0x00, 0x01, 0x00,
    0x00, 0x00, 0x57, 0x40, 0x00, 0x00, 0x49, 0x01, 0x00, 0x00};
const uint8_t kResponseIPV40[] = {
    0x4C, 0x00, 0x00, 0x00, 0x14, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00,
    0x00, 0x57, 0x40, 0x00, 0x00, 0x49, 0x01, 0x00, 0x00, 0x38, 0x00,
    0x75, 0x00, 0x34, 0x00, 0x04, 0x00, 0x30, 0x00, 0x01, 0x00, 0x08,
    0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x3C, 0x22, 0x00, 0x02, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0xC0, 0xA8, 0x0A, 0x14, 0x00, 0x00};
const uint8_t kResponseIPV40WakeOnDisconnect[] = {
    0x50, 0x00, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
    0x57, 0x40, 0x00, 0x00, 0x49, 0x01, 0x00, 0x00, 0x3C, 0x00, 0x75, 0x00,
    0x04, 0x00, 0x02, 0x00, 0x34, 0x00, 0x04, 0x00, 0x30, 0x00, 0x01, 0x00,
    0x08, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x3C, 0x22, 0x00, 0x02, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0xC0, 0xA8, 0x0A, 0x14, 0x00, 0x00};
const uint8_t kResponseIPV401[] = {
    0x7C, 0x00, 0x00, 0x00, 0x14, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00,
    0x57, 0x40, 0x00, 0x00, 0x49, 0x01, 0x00, 0x00, 0x68, 0x00, 0x75, 0x00,
    0x64, 0x00, 0x04, 0x00, 0x30, 0x00, 0x01, 0x00, 0x08, 0x00, 0x01, 0x00,
    0x00, 0x00, 0x00, 0x3C, 0x22, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x02,
    0x03, 0x04, 0x00, 0x00, 0x30, 0x00, 0x02, 0x00, 0x08, 0x00, 0x01, 0x00,
    0x00, 0x00, 0x00, 0x3C, 0x22, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xC0, 0xA8,
    0x0A, 0x14, 0x00, 0x00};
const uint8_t kResponseIPV401IPV60[] = {
    0xB8, 0x00, 0x00, 0x00, 0x14, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00,
    0x57, 0x40, 0x00, 0x00, 0x49, 0x01, 0x00, 0x00, 0xA4, 0x00, 0x75, 0x00,
    0xA0, 0x00, 0x04, 0x00, 0x30, 0x00, 0x01, 0x00, 0x08, 0x00, 0x01, 0x00,
    0x00, 0x00, 0x00, 0x3C, 0x22, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x02,
    0x03, 0x04, 0x00, 0x00, 0x30, 0x00, 0x02, 0x00, 0x08, 0x00, 0x01, 0x00,
    0x00, 0x00, 0x00, 0x3C, 0x22, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xC0, 0xA8,
    0x0A, 0x14, 0x00, 0x00, 0x3C, 0x00, 0x03, 0x00, 0x09, 0x00, 0x01, 0x00,
    0x00, 0x00, 0xC0, 0xFF, 0x3F, 0x00, 0x00, 0x00, 0x2A, 0x00, 0x02, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFE, 0xDC,
    0xBA, 0x98, 0x76, 0x54, 0x32, 0x10, 0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54,
    0x32, 0x10, 0x00, 0x00};
const uint8_t kResponseIPV401IPV601[] = {
    0xF4, 0x00, 0x00, 0x00, 0x14, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00,
    0x57, 0x40, 0x00, 0x00, 0x49, 0x01, 0x00, 0x00, 0xE0, 0x00, 0x75, 0x00,
    0xDC, 0x00, 0x04, 0x00, 0x30, 0x00, 0x01, 0x00, 0x08, 0x00, 0x01, 0x00,
    0x00, 0x00, 0x00, 0x3C, 0x22, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x02,
    0x03, 0x04, 0x00, 0x00, 0x3C, 0x00, 0x02, 0x00, 0x09, 0x00, 0x01, 0x00,
    0x00, 0x00, 0xC0, 0xFF, 0x3F, 0x00, 0x00, 0x00, 0x2A, 0x00, 0x02, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x80,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x08, 0x00, 0x20, 0x0C,
    0x41, 0x7A, 0x00, 0x00, 0x30, 0x00, 0x03, 0x00, 0x08, 0x00, 0x01, 0x00,
    0x00, 0x00, 0x00, 0x3C, 0x22, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xC0, 0xA8,
    0x0A, 0x14, 0x00, 0x00, 0x3C, 0x00, 0x04, 0x00, 0x09, 0x00, 0x01, 0x00,
    0x00, 0x00, 0xC0, 0xFF, 0x3F, 0x00, 0x00, 0x00, 0x2A, 0x00, 0x02, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFE, 0xDC,
    0xBA, 0x98, 0x76, 0x54, 0x32, 0x10, 0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54,
    0x32, 0x10, 0x00, 0x00};

}  // namespace

ByteString CreatePattern(const unsigned char *prefix, size_t prefix_len,
                         const unsigned char *addr, size_t addr_len) {
  ByteString result(prefix, prefix_len);
  result.Append(ByteString(addr, addr_len));
  return result;
}

TEST(WakeOnWiFiTest, CreateIPAddressPatternAndMask) {
  ByteString pattern;
  ByteString mask;
  ByteString expected_pattern;

  WakeOnWiFi::CreateIPAddressPatternAndMask(IPAddress(kIPV4Address0), &pattern,
                                            &mask);
  expected_pattern =
      CreatePattern(kIPV4PatternPrefix, sizeof(kIPV4PatternPrefix),
                    kIPV4Address0Bytes, sizeof(kIPV4Address0Bytes));
  EXPECT_TRUE(pattern.Equals(expected_pattern));
  EXPECT_TRUE(mask.Equals(ByteString(kIPV4MaskBytes, sizeof(kIPV4MaskBytes))));

  pattern.Clear();
  expected_pattern.Clear();
  mask.Clear();
  WakeOnWiFi::CreateIPAddressPatternAndMask(IPAddress(kIPV4Address1), &pattern,
                                            &mask);
  expected_pattern =
      CreatePattern(kIPV4PatternPrefix, sizeof(kIPV4PatternPrefix),
                    kIPV4Address1Bytes, sizeof(kIPV4Address1Bytes));
  EXPECT_TRUE(pattern.Equals(expected_pattern));
  EXPECT_TRUE(mask.Equals(ByteString(kIPV4MaskBytes, sizeof(kIPV4MaskBytes))));

  pattern.Clear();
  expected_pattern.Clear();
  mask.Clear();
  WakeOnWiFi::CreateIPAddressPatternAndMask(IPAddress(kIPV6Address0), &pattern,
                                            &mask);
  expected_pattern =
      CreatePattern(kIPV6PatternPrefix, sizeof(kIPV6PatternPrefix),
                    kIPV6Address0Bytes, sizeof(kIPV6Address0Bytes));
  EXPECT_TRUE(pattern.Equals(expected_pattern));
  EXPECT_TRUE(mask.Equals(ByteString(kIPV6MaskBytes, sizeof(kIPV6MaskBytes))));

  pattern.Clear();
  expected_pattern.Clear();
  mask.Clear();
  WakeOnWiFi::CreateIPAddressPatternAndMask(IPAddress(kIPV6Address1), &pattern,
                                            &mask);
  expected_pattern =
      CreatePattern(kIPV6PatternPrefix, sizeof(kIPV6PatternPrefix),
                    kIPV6Address1Bytes, sizeof(kIPV6Address1Bytes));
  EXPECT_TRUE(pattern.Equals(expected_pattern));
  EXPECT_TRUE(mask.Equals(ByteString(kIPV6MaskBytes, sizeof(kIPV6MaskBytes))));

  pattern.Clear();
  expected_pattern.Clear();
  mask.Clear();
  WakeOnWiFi::CreateIPAddressPatternAndMask(IPAddress(kIPV6Address2), &pattern,
                                            &mask);
  expected_pattern =
      CreatePattern(kIPV6PatternPrefix, sizeof(kIPV6PatternPrefix),
                    kIPV6Address2Bytes, sizeof(kIPV6Address2Bytes));
  EXPECT_TRUE(pattern.Equals(expected_pattern));
  EXPECT_TRUE(mask.Equals(ByteString(kIPV6MaskBytes, sizeof(kIPV6MaskBytes))));

  pattern.Clear();
  expected_pattern.Clear();
  mask.Clear();
  WakeOnWiFi::CreateIPAddressPatternAndMask(IPAddress(kIPV6Address3), &pattern,
                                            &mask);
  expected_pattern =
      CreatePattern(kIPV6PatternPrefix, sizeof(kIPV6PatternPrefix),
                    kIPV6Address3Bytes, sizeof(kIPV6Address3Bytes));
  EXPECT_TRUE(pattern.Equals(expected_pattern));
  EXPECT_TRUE(mask.Equals(ByteString(kIPV6MaskBytes, sizeof(kIPV6MaskBytes))));

  pattern.Clear();
  expected_pattern.Clear();
  mask.Clear();
  WakeOnWiFi::CreateIPAddressPatternAndMask(IPAddress(kIPV6Address4), &pattern,
                                            &mask);
  expected_pattern =
      CreatePattern(kIPV6PatternPrefix, sizeof(kIPV6PatternPrefix),
                    kIPV6Address4Bytes, sizeof(kIPV6Address4Bytes));
  EXPECT_TRUE(pattern.Equals(expected_pattern));
  EXPECT_TRUE(mask.Equals(ByteString(kIPV6MaskBytes, sizeof(kIPV6MaskBytes))));

  pattern.Clear();
  expected_pattern.Clear();
  mask.Clear();
  WakeOnWiFi::CreateIPAddressPatternAndMask(IPAddress(kIPV6Address5), &pattern,
                                            &mask);
  expected_pattern =
      CreatePattern(kIPV6PatternPrefix, sizeof(kIPV6PatternPrefix),
                    kIPV6Address5Bytes, sizeof(kIPV6Address5Bytes));
  EXPECT_TRUE(pattern.Equals(expected_pattern));
  EXPECT_TRUE(mask.Equals(ByteString(kIPV6MaskBytes, sizeof(kIPV6MaskBytes))));

  pattern.Clear();
  expected_pattern.Clear();
  mask.Clear();
  WakeOnWiFi::CreateIPAddressPatternAndMask(IPAddress(kIPV6Address6), &pattern,
                                            &mask);
  expected_pattern =
      CreatePattern(kIPV6PatternPrefix, sizeof(kIPV6PatternPrefix),
                    kIPV6Address6Bytes, sizeof(kIPV6Address6Bytes));
  EXPECT_TRUE(pattern.Equals(expected_pattern));
  EXPECT_TRUE(mask.Equals(ByteString(kIPV6MaskBytes, sizeof(kIPV6MaskBytes))));

  pattern.Clear();
  expected_pattern.Clear();
  mask.Clear();
  WakeOnWiFi::CreateIPAddressPatternAndMask(IPAddress(kIPV6Address7), &pattern,
                                            &mask);
  expected_pattern =
      CreatePattern(kIPV6PatternPrefix, sizeof(kIPV6PatternPrefix),
                    kIPV6Address7Bytes, sizeof(kIPV6Address7Bytes));
  EXPECT_TRUE(pattern.Equals(expected_pattern));
  EXPECT_TRUE(mask.Equals(ByteString(kIPV6MaskBytes, sizeof(kIPV6MaskBytes))));
}

TEST(WakeOnWiFiTest, ConfigureWiphyIndex) {
  SetWakeOnPacketConnMessage msg;
  uint32_t value;
  EXPECT_FALSE(
      msg.attributes()->GetU32AttributeValue(NL80211_ATTR_WIPHY, &value));

  WakeOnWiFi::ConfigureWiphyIndex(&msg, 137);
  EXPECT_TRUE(
      msg.attributes()->GetU32AttributeValue(NL80211_ATTR_WIPHY, &value));
  EXPECT_EQ(value, 137);
}

TEST(WakeOnWiFiTest, ConfigureDisableWakeOnWiFiMessage) {
  SetWakeOnPacketConnMessage msg;
  Error e;
  uint32_t value;
  EXPECT_FALSE(
      msg.attributes()->GetU32AttributeValue(NL80211_ATTR_WIPHY, &value));

  WakeOnWiFi::ConfigureDisableWakeOnWiFiMessage(&msg, 57, &e);
  EXPECT_EQ(e.type(), Error::Type::kSuccess);
  EXPECT_TRUE(
      msg.attributes()->GetU32AttributeValue(NL80211_ATTR_WIPHY, &value));
  EXPECT_EQ(value, 57);
}

TEST(WakeOnWiFiTest, WakeOnWiFiSettingsMatch) {
  IPAddressStore all_addresses;
  set<WakeOnWiFi::WakeOnWiFiTrigger> trigs;
  GetWakeOnPacketConnMessage msg0;
  msg0.InitFromNlmsg(
      reinterpret_cast<const nlmsghdr *>(kResponseNoIPAddresses));
  EXPECT_TRUE(WakeOnWiFi::WakeOnWiFiSettingsMatch(msg0, trigs, all_addresses));

  trigs.insert(WakeOnWiFi::kIPAddress);
  all_addresses.AddUnique(
      IPAddress(string(kIPV4Address0, sizeof(kIPV4Address0))));
  GetWakeOnPacketConnMessage msg1;
  msg1.InitFromNlmsg(reinterpret_cast<const nlmsghdr *>(kResponseIPV40));
  EXPECT_TRUE(WakeOnWiFi::WakeOnWiFiSettingsMatch(msg1, trigs, all_addresses));
  EXPECT_FALSE(WakeOnWiFi::WakeOnWiFiSettingsMatch(msg0, trigs, all_addresses));

  // Test matching of wake-on-disconnect flag.
  trigs.insert(WakeOnWiFi::kDisconnect);
  GetWakeOnPacketConnMessage msg2;
  msg2.InitFromNlmsg(
      reinterpret_cast<const nlmsghdr *>(kResponseIPV40WakeOnDisconnect));
  EXPECT_TRUE(WakeOnWiFi::WakeOnWiFiSettingsMatch(msg2, trigs, all_addresses));
  EXPECT_FALSE(WakeOnWiFi::WakeOnWiFiSettingsMatch(msg1, trigs, all_addresses));
  EXPECT_FALSE(WakeOnWiFi::WakeOnWiFiSettingsMatch(msg0, trigs, all_addresses));

  trigs.erase(WakeOnWiFi::kDisconnect);
  all_addresses.AddUnique(
      IPAddress(string(kIPV4Address1, sizeof(kIPV4Address1))));
  GetWakeOnPacketConnMessage msg3;
  msg3.InitFromNlmsg(reinterpret_cast<const nlmsghdr *>(kResponseIPV401));
  EXPECT_TRUE(WakeOnWiFi::WakeOnWiFiSettingsMatch(msg3, trigs, all_addresses));
  EXPECT_FALSE(WakeOnWiFi::WakeOnWiFiSettingsMatch(msg2, trigs, all_addresses));
  EXPECT_FALSE(WakeOnWiFi::WakeOnWiFiSettingsMatch(msg1, trigs, all_addresses));
  EXPECT_FALSE(WakeOnWiFi::WakeOnWiFiSettingsMatch(msg0, trigs, all_addresses));

  all_addresses.AddUnique(
      IPAddress(string(kIPV6Address0, sizeof(kIPV6Address0))));
  GetWakeOnPacketConnMessage msg4;
  msg4.InitFromNlmsg(
      reinterpret_cast<const nlmsghdr *>((kResponseIPV401IPV60)));
  EXPECT_TRUE(WakeOnWiFi::WakeOnWiFiSettingsMatch(msg4, trigs, all_addresses));
  EXPECT_FALSE(WakeOnWiFi::WakeOnWiFiSettingsMatch(msg3, trigs, all_addresses));
  EXPECT_FALSE(WakeOnWiFi::WakeOnWiFiSettingsMatch(msg2, trigs, all_addresses));
  EXPECT_FALSE(WakeOnWiFi::WakeOnWiFiSettingsMatch(msg1, trigs, all_addresses));
  EXPECT_FALSE(WakeOnWiFi::WakeOnWiFiSettingsMatch(msg0, trigs, all_addresses));

  all_addresses.AddUnique(
      IPAddress(string(kIPV6Address1, sizeof(kIPV6Address1))));
  GetWakeOnPacketConnMessage msg5;
  msg5.InitFromNlmsg(reinterpret_cast<const nlmsghdr *>(kResponseIPV401IPV601));
  EXPECT_TRUE(WakeOnWiFi::WakeOnWiFiSettingsMatch(msg5, trigs, all_addresses));
  EXPECT_FALSE(WakeOnWiFi::WakeOnWiFiSettingsMatch(msg4, trigs, all_addresses));
  EXPECT_FALSE(WakeOnWiFi::WakeOnWiFiSettingsMatch(msg3, trigs, all_addresses));
  EXPECT_FALSE(WakeOnWiFi::WakeOnWiFiSettingsMatch(msg2, trigs, all_addresses));
  EXPECT_FALSE(WakeOnWiFi::WakeOnWiFiSettingsMatch(msg1, trigs, all_addresses));
  EXPECT_FALSE(WakeOnWiFi::WakeOnWiFiSettingsMatch(msg0, trigs, all_addresses));
}

TEST(WakeOnWiFiTest, ConfigureSetWakeOnWiFiSettingsMessage) {
  IPAddressStore all_addresses;
  set<WakeOnWiFi::WakeOnWiFiTrigger> trigs;
  int index = 1;  // wiphy device number
  SetWakeOnPacketConnMessage msg0;
  Error e;
  trigs.insert(WakeOnWiFi::kIPAddress);
  all_addresses.AddUnique(
      IPAddress(string(kIPV4Address0, sizeof(kIPV4Address0))));
  ByteString expected_mask = ByteString(kIPV4MaskBytes, sizeof(kIPV4MaskBytes));
  ByteString expected_pattern =
      CreatePattern(kIPV4PatternPrefix, sizeof(kIPV4PatternPrefix),
                    kIPV4Address0Bytes, sizeof(kIPV4Address0Bytes));
  WakeOnWiFi::ConfigureSetWakeOnWiFiSettingsMessage(&msg0, trigs, all_addresses,
                                                 index, &e);
  EXPECT_TRUE(WakeOnWiFi::WakeOnWiFiSettingsMatch(msg0, trigs, all_addresses));

  SetWakeOnPacketConnMessage msg1;
  all_addresses.AddUnique(
      IPAddress(string(kIPV4Address1, sizeof(kIPV4Address1))));
  expected_pattern =
      CreatePattern(kIPV4PatternPrefix, sizeof(kIPV4PatternPrefix),
                    kIPV4Address1Bytes, sizeof(kIPV4Address1Bytes));
  WakeOnWiFi::ConfigureSetWakeOnWiFiSettingsMessage(&msg1, trigs, all_addresses,
                                                 index, &e);
  EXPECT_TRUE(WakeOnWiFi::WakeOnWiFiSettingsMatch(msg1, trigs, all_addresses));

  SetWakeOnPacketConnMessage msg2;
  all_addresses.AddUnique(
      IPAddress(string(kIPV6Address0, sizeof(kIPV6Address0))));
  expected_mask = ByteString(kIPV6MaskBytes, sizeof(kIPV6MaskBytes));
  expected_pattern =
      CreatePattern(kIPV6PatternPrefix, sizeof(kIPV6PatternPrefix),
                    kIPV6Address0Bytes, sizeof(kIPV6Address0Bytes));
  WakeOnWiFi::ConfigureSetWakeOnWiFiSettingsMessage(&msg2, trigs, all_addresses,
                                                 index, &e);
  EXPECT_TRUE(WakeOnWiFi::WakeOnWiFiSettingsMatch(msg2, trigs, all_addresses));

  SetWakeOnPacketConnMessage msg3;
  all_addresses.AddUnique(
      IPAddress(string(kIPV6Address1, sizeof(kIPV6Address1))));
  expected_pattern =
      CreatePattern(kIPV6PatternPrefix, sizeof(kIPV6PatternPrefix),
                    kIPV6Address1Bytes, sizeof(kIPV6Address1Bytes));
  WakeOnWiFi::ConfigureSetWakeOnWiFiSettingsMessage(&msg3, trigs, all_addresses,
                                                 index, &e);
  EXPECT_TRUE(WakeOnWiFi::WakeOnWiFiSettingsMatch(msg3, trigs, all_addresses));

  SetWakeOnPacketConnMessage msg4;
  all_addresses.AddUnique(
      IPAddress(string(kIPV6Address2, sizeof(kIPV6Address2))));
  expected_pattern =
      CreatePattern(kIPV6PatternPrefix, sizeof(kIPV6PatternPrefix),
                    kIPV6Address2Bytes, sizeof(kIPV6Address2Bytes));
  WakeOnWiFi::ConfigureSetWakeOnWiFiSettingsMessage(&msg4, trigs, all_addresses,
                                                 index, &e);
  EXPECT_TRUE(WakeOnWiFi::WakeOnWiFiSettingsMatch(msg4, trigs, all_addresses));

  SetWakeOnPacketConnMessage msg5;
  all_addresses.AddUnique(
      IPAddress(string(kIPV6Address3, sizeof(kIPV6Address3))));
  expected_pattern =
      CreatePattern(kIPV6PatternPrefix, sizeof(kIPV6PatternPrefix),
                    kIPV6Address3Bytes, sizeof(kIPV6Address3Bytes));
  WakeOnWiFi::ConfigureSetWakeOnWiFiSettingsMessage(&msg5, trigs, all_addresses,
                                                 index, &e);
  EXPECT_TRUE(WakeOnWiFi::WakeOnWiFiSettingsMatch(msg5, trigs, all_addresses));

  SetWakeOnPacketConnMessage msg6;
  all_addresses.AddUnique(
      IPAddress(string(kIPV6Address4, sizeof(kIPV6Address4))));
  expected_pattern =
      CreatePattern(kIPV6PatternPrefix, sizeof(kIPV6PatternPrefix),
                    kIPV6Address4Bytes, sizeof(kIPV6Address4Bytes));
  WakeOnWiFi::ConfigureSetWakeOnWiFiSettingsMessage(&msg6, trigs, all_addresses,
                                                 index, &e);
  EXPECT_TRUE(WakeOnWiFi::WakeOnWiFiSettingsMatch(msg6, trigs, all_addresses));

  SetWakeOnPacketConnMessage msg7;
  all_addresses.AddUnique(
      IPAddress(string(kIPV6Address5, sizeof(kIPV6Address5))));
  expected_pattern =
      CreatePattern(kIPV6PatternPrefix, sizeof(kIPV6PatternPrefix),
                    kIPV6Address5Bytes, sizeof(kIPV6Address5Bytes));
  WakeOnWiFi::ConfigureSetWakeOnWiFiSettingsMessage(&msg7, trigs, all_addresses,
                                                 index, &e);
  EXPECT_TRUE(WakeOnWiFi::WakeOnWiFiSettingsMatch(msg7, trigs, all_addresses));

  SetWakeOnPacketConnMessage msg8;
  all_addresses.AddUnique(
      IPAddress(string(kIPV6Address6, sizeof(kIPV6Address6))));
  expected_pattern =
      CreatePattern(kIPV6PatternPrefix, sizeof(kIPV6PatternPrefix),
                    kIPV6Address6Bytes, sizeof(kIPV6Address6Bytes));
  WakeOnWiFi::ConfigureSetWakeOnWiFiSettingsMessage(&msg8, trigs, all_addresses,
                                                 index, &e);
  EXPECT_TRUE(WakeOnWiFi::WakeOnWiFiSettingsMatch(msg8, trigs, all_addresses));

  SetWakeOnPacketConnMessage msg9;
  all_addresses.AddUnique(
      IPAddress(string(kIPV6Address7, sizeof(kIPV6Address7))));
  expected_pattern =
      CreatePattern(kIPV6PatternPrefix, sizeof(kIPV6PatternPrefix),
                    kIPV6Address7Bytes, sizeof(kIPV6Address7Bytes));
  WakeOnWiFi::ConfigureSetWakeOnWiFiSettingsMessage(&msg9, trigs, all_addresses,
                                                 index, &e);
  EXPECT_TRUE(WakeOnWiFi::WakeOnWiFiSettingsMatch(msg9, trigs, all_addresses));
}

}  // namespace shill
