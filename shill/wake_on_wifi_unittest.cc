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

bool IPAddressesMatch(const IPAddressStore &addrs0,
                      const std::vector<IPAddress> &addrs1) {
  if (addrs0.Count() != addrs1.size()) {
    return false;
  }
  bool mismatch_found = false;
  int num_mismatch = addrs0.Count();
  for (const IPAddress &addr : addrs1) {
    if (addrs0.Contains(addr)) {
      --num_mismatch;
    } else {
      mismatch_found = true;
      break;
    }
  }
  if (mismatch_found || num_mismatch != 0) {
    return false;
  } else {
    return true;
  }
}

TEST(WakeOnWifiTest, CreateIPAddressPatternAndMask) {
  ByteString pattern;
  ByteString mask;
  ByteString expected_pattern;

  WakeOnWifi::CreateIPAddressPatternAndMask(IPAddress(kIPV4Address0), &pattern,
                                            &mask);
  expected_pattern =
      CreatePattern(kIPV4PatternPrefix, sizeof(kIPV4PatternPrefix),
                    kIPV4Address0Bytes, sizeof(kIPV4Address0Bytes));
  EXPECT_TRUE(pattern.Equals(expected_pattern));
  EXPECT_TRUE(mask.Equals(ByteString(kIPV4MaskBytes, sizeof(kIPV4MaskBytes))));

  pattern.Clear();
  expected_pattern.Clear();
  mask.Clear();
  WakeOnWifi::CreateIPAddressPatternAndMask(IPAddress(kIPV4Address1), &pattern,
                                            &mask);
  expected_pattern =
      CreatePattern(kIPV4PatternPrefix, sizeof(kIPV4PatternPrefix),
                    kIPV4Address1Bytes, sizeof(kIPV4Address1Bytes));
  EXPECT_TRUE(pattern.Equals(expected_pattern));
  EXPECT_TRUE(mask.Equals(ByteString(kIPV4MaskBytes, sizeof(kIPV4MaskBytes))));

  pattern.Clear();
  expected_pattern.Clear();
  mask.Clear();
  WakeOnWifi::CreateIPAddressPatternAndMask(IPAddress(kIPV6Address0), &pattern,
                                            &mask);
  expected_pattern =
      CreatePattern(kIPV6PatternPrefix, sizeof(kIPV6PatternPrefix),
                    kIPV6Address0Bytes, sizeof(kIPV6Address0Bytes));
  EXPECT_TRUE(pattern.Equals(expected_pattern));
  EXPECT_TRUE(mask.Equals(ByteString(kIPV6MaskBytes, sizeof(kIPV6MaskBytes))));

  pattern.Clear();
  expected_pattern.Clear();
  mask.Clear();
  WakeOnWifi::CreateIPAddressPatternAndMask(IPAddress(kIPV6Address1), &pattern,
                                            &mask);
  expected_pattern =
      CreatePattern(kIPV6PatternPrefix, sizeof(kIPV6PatternPrefix),
                    kIPV6Address1Bytes, sizeof(kIPV6Address1Bytes));
  EXPECT_TRUE(pattern.Equals(expected_pattern));
  EXPECT_TRUE(mask.Equals(ByteString(kIPV6MaskBytes, sizeof(kIPV6MaskBytes))));

  pattern.Clear();
  expected_pattern.Clear();
  mask.Clear();
  WakeOnWifi::CreateIPAddressPatternAndMask(IPAddress(kIPV6Address2), &pattern,
                                            &mask);
  expected_pattern =
      CreatePattern(kIPV6PatternPrefix, sizeof(kIPV6PatternPrefix),
                    kIPV6Address2Bytes, sizeof(kIPV6Address2Bytes));
  EXPECT_TRUE(pattern.Equals(expected_pattern));
  EXPECT_TRUE(mask.Equals(ByteString(kIPV6MaskBytes, sizeof(kIPV6MaskBytes))));

  pattern.Clear();
  expected_pattern.Clear();
  mask.Clear();
  WakeOnWifi::CreateIPAddressPatternAndMask(IPAddress(kIPV6Address3), &pattern,
                                            &mask);
  expected_pattern =
      CreatePattern(kIPV6PatternPrefix, sizeof(kIPV6PatternPrefix),
                    kIPV6Address3Bytes, sizeof(kIPV6Address3Bytes));
  EXPECT_TRUE(pattern.Equals(expected_pattern));
  EXPECT_TRUE(mask.Equals(ByteString(kIPV6MaskBytes, sizeof(kIPV6MaskBytes))));

  pattern.Clear();
  expected_pattern.Clear();
  mask.Clear();
  WakeOnWifi::CreateIPAddressPatternAndMask(IPAddress(kIPV6Address4), &pattern,
                                            &mask);
  expected_pattern =
      CreatePattern(kIPV6PatternPrefix, sizeof(kIPV6PatternPrefix),
                    kIPV6Address4Bytes, sizeof(kIPV6Address4Bytes));
  EXPECT_TRUE(pattern.Equals(expected_pattern));
  EXPECT_TRUE(mask.Equals(ByteString(kIPV6MaskBytes, sizeof(kIPV6MaskBytes))));

  pattern.Clear();
  expected_pattern.Clear();
  mask.Clear();
  WakeOnWifi::CreateIPAddressPatternAndMask(IPAddress(kIPV6Address5), &pattern,
                                            &mask);
  expected_pattern =
      CreatePattern(kIPV6PatternPrefix, sizeof(kIPV6PatternPrefix),
                    kIPV6Address5Bytes, sizeof(kIPV6Address5Bytes));
  EXPECT_TRUE(pattern.Equals(expected_pattern));
  EXPECT_TRUE(mask.Equals(ByteString(kIPV6MaskBytes, sizeof(kIPV6MaskBytes))));

  pattern.Clear();
  expected_pattern.Clear();
  mask.Clear();
  WakeOnWifi::CreateIPAddressPatternAndMask(IPAddress(kIPV6Address6), &pattern,
                                            &mask);
  expected_pattern =
      CreatePattern(kIPV6PatternPrefix, sizeof(kIPV6PatternPrefix),
                    kIPV6Address6Bytes, sizeof(kIPV6Address6Bytes));
  EXPECT_TRUE(pattern.Equals(expected_pattern));
  EXPECT_TRUE(mask.Equals(ByteString(kIPV6MaskBytes, sizeof(kIPV6MaskBytes))));

  pattern.Clear();
  expected_pattern.Clear();
  mask.Clear();
  WakeOnWifi::CreateIPAddressPatternAndMask(IPAddress(kIPV6Address7), &pattern,
                                            &mask);
  expected_pattern =
      CreatePattern(kIPV6PatternPrefix, sizeof(kIPV6PatternPrefix),
                    kIPV6Address7Bytes, sizeof(kIPV6Address7Bytes));
  EXPECT_TRUE(pattern.Equals(expected_pattern));
  EXPECT_TRUE(mask.Equals(ByteString(kIPV6MaskBytes, sizeof(kIPV6MaskBytes))));
}

TEST(WakeOnWifiTest, ConfigureWiphyIndex) {
  SetWakeOnPacketConnMessage msg;
  uint32_t value;
  EXPECT_FALSE(
      msg.attributes()->GetU32AttributeValue(NL80211_ATTR_WIPHY, &value));

  WakeOnWifi::ConfigureWiphyIndex(&msg, 137);
  EXPECT_TRUE(
      msg.attributes()->GetU32AttributeValue(NL80211_ATTR_WIPHY, &value));
  EXPECT_EQ(value, 137);
}

TEST(WakeOnWifiTest, ConfigureDisableWakeOnPacketMsg) {
  SetWakeOnPacketConnMessage msg;
  Error e;
  uint32_t value;
  EXPECT_FALSE(
      msg.attributes()->GetU32AttributeValue(NL80211_ATTR_WIPHY, &value));

  WakeOnWifi::ConfigureDisableWakeOnPacketMsg(&msg, 57, &e);
  EXPECT_EQ(e.type(), Error::Type::kSuccess);
  EXPECT_TRUE(
      msg.attributes()->GetU32AttributeValue(NL80211_ATTR_WIPHY, &value));
  EXPECT_EQ(value, 57);
}

TEST(WakeOnWifiTest, WakeOnPacketSettingsMatch) {
  IPAddressStore all_addresses;
  scoped_ptr<uint8_t[]> message_memory_0(
      new uint8_t[sizeof(kResponseNoIPAddresses)]);
  memcpy(message_memory_0.get(), kResponseNoIPAddresses,
         sizeof(kResponseNoIPAddresses));
  nlmsghdr *hdr0 = reinterpret_cast<nlmsghdr *>(message_memory_0.get());
  GetWakeOnPacketConnMessage msg0;
  msg0.InitFromNlmsg(hdr0);
  EXPECT_TRUE(WakeOnWifi::WakeOnPacketSettingsMatch(msg0, all_addresses));

  all_addresses.AddUnique(
      IPAddress(string(kIPV4Address0, sizeof(kIPV4Address0))));
  scoped_ptr<uint8_t[]> message_memory_1(new uint8_t[sizeof(kResponseIPV40)]);
  memcpy(message_memory_1.get(), kResponseIPV40, sizeof(kResponseIPV40));
  nlmsghdr *hdr1 = reinterpret_cast<nlmsghdr *>(message_memory_1.get());
  GetWakeOnPacketConnMessage msg1;
  msg1.InitFromNlmsg(hdr1);
  EXPECT_TRUE(WakeOnWifi::WakeOnPacketSettingsMatch(msg1, all_addresses));
  EXPECT_FALSE(WakeOnWifi::WakeOnPacketSettingsMatch(msg0, all_addresses));

  all_addresses.AddUnique(
      IPAddress(string(kIPV4Address1, sizeof(kIPV4Address1))));
  scoped_ptr<uint8_t[]> message_memory_2(new uint8_t[sizeof(kResponseIPV401)]);
  memcpy(message_memory_2.get(), kResponseIPV401, sizeof(kResponseIPV401));
  nlmsghdr *hdr2 = reinterpret_cast<nlmsghdr *>(message_memory_2.get());
  GetWakeOnPacketConnMessage msg2;
  msg2.InitFromNlmsg(hdr2);
  EXPECT_TRUE(WakeOnWifi::WakeOnPacketSettingsMatch(msg2, all_addresses));
  EXPECT_FALSE(WakeOnWifi::WakeOnPacketSettingsMatch(msg1, all_addresses));
  EXPECT_FALSE(WakeOnWifi::WakeOnPacketSettingsMatch(msg0, all_addresses));

  all_addresses.AddUnique(
      IPAddress(string(kIPV6Address0, sizeof(kIPV6Address0))));
  scoped_ptr<uint8_t[]> message_memory_3(
      new uint8_t[sizeof(kResponseIPV401IPV60)]);
  memcpy(message_memory_3.get(), kResponseIPV401IPV60,
         sizeof(kResponseIPV401IPV60));
  nlmsghdr *hdr3 = reinterpret_cast<nlmsghdr *>(message_memory_3.get());
  GetWakeOnPacketConnMessage msg3;
  msg3.InitFromNlmsg(hdr3);
  EXPECT_TRUE(WakeOnWifi::WakeOnPacketSettingsMatch(msg3, all_addresses));
  EXPECT_FALSE(WakeOnWifi::WakeOnPacketSettingsMatch(msg2, all_addresses));
  EXPECT_FALSE(WakeOnWifi::WakeOnPacketSettingsMatch(msg1, all_addresses));
  EXPECT_FALSE(WakeOnWifi::WakeOnPacketSettingsMatch(msg0, all_addresses));

  all_addresses.AddUnique(
      IPAddress(string(kIPV6Address1, sizeof(kIPV6Address1))));
  scoped_ptr<uint8_t[]> message_memory_4(
      new uint8_t[sizeof(kResponseIPV401IPV601)]);
  memcpy(message_memory_4.get(), kResponseIPV401IPV601,
         sizeof(kResponseIPV401IPV601));
  nlmsghdr *hdr4 = reinterpret_cast<nlmsghdr *>(message_memory_4.get());
  GetWakeOnPacketConnMessage msg4;
  msg4.InitFromNlmsg(hdr4);
  EXPECT_TRUE(WakeOnWifi::WakeOnPacketSettingsMatch(msg4, all_addresses));
  EXPECT_FALSE(WakeOnWifi::WakeOnPacketSettingsMatch(msg3, all_addresses));
  EXPECT_FALSE(WakeOnWifi::WakeOnPacketSettingsMatch(msg2, all_addresses));
  EXPECT_FALSE(WakeOnWifi::WakeOnPacketSettingsMatch(msg1, all_addresses));
  EXPECT_FALSE(WakeOnWifi::WakeOnPacketSettingsMatch(msg0, all_addresses));
}

TEST(WakeOnWifiTest, ConfigureAddWakeOnPacketMsg) {
  IPAddressStore all_addresses;
  int index = 1;  // wiphy device number
  SetWakeOnPacketConnMessage msg0;
  Error e;
  all_addresses.AddUnique(
      IPAddress(string(kIPV4Address0, sizeof(kIPV4Address0))));
  ByteString expected_mask = ByteString(kIPV4MaskBytes, sizeof(kIPV4MaskBytes));
  ByteString expected_pattern =
      CreatePattern(kIPV4PatternPrefix, sizeof(kIPV4PatternPrefix),
                    kIPV4Address0Bytes, sizeof(kIPV4Address0Bytes));
  WakeOnWifi::ConfigureAddWakeOnPacketMsg(&msg0, all_addresses, index, &e);
  EXPECT_TRUE(WakeOnWifi::WakeOnPacketSettingsMatch(msg0, all_addresses));

  SetWakeOnPacketConnMessage msg1;
  all_addresses.AddUnique(
      IPAddress(string(kIPV4Address1, sizeof(kIPV4Address1))));
  expected_pattern =
      CreatePattern(kIPV4PatternPrefix, sizeof(kIPV4PatternPrefix),
                    kIPV4Address1Bytes, sizeof(kIPV4Address1Bytes));
  WakeOnWifi::ConfigureAddWakeOnPacketMsg(&msg1, all_addresses, index, &e);
  EXPECT_TRUE(WakeOnWifi::WakeOnPacketSettingsMatch(msg1, all_addresses));

  SetWakeOnPacketConnMessage msg2;
  all_addresses.AddUnique(
      IPAddress(string(kIPV6Address0, sizeof(kIPV6Address0))));
  expected_mask = ByteString(kIPV6MaskBytes, sizeof(kIPV6MaskBytes));
  expected_pattern =
      CreatePattern(kIPV6PatternPrefix, sizeof(kIPV6PatternPrefix),
                    kIPV6Address0Bytes, sizeof(kIPV6Address0Bytes));
  WakeOnWifi::ConfigureAddWakeOnPacketMsg(&msg2, all_addresses, index, &e);
  EXPECT_TRUE(WakeOnWifi::WakeOnPacketSettingsMatch(msg2, all_addresses));

  SetWakeOnPacketConnMessage msg3;
  all_addresses.AddUnique(
      IPAddress(string(kIPV6Address1, sizeof(kIPV6Address1))));
  expected_pattern =
      CreatePattern(kIPV6PatternPrefix, sizeof(kIPV6PatternPrefix),
                    kIPV6Address1Bytes, sizeof(kIPV6Address1Bytes));
  WakeOnWifi::ConfigureAddWakeOnPacketMsg(&msg3, all_addresses, index, &e);
  EXPECT_TRUE(WakeOnWifi::WakeOnPacketSettingsMatch(msg3, all_addresses));

  SetWakeOnPacketConnMessage msg4;
  all_addresses.AddUnique(
      IPAddress(string(kIPV6Address2, sizeof(kIPV6Address2))));
  expected_pattern =
      CreatePattern(kIPV6PatternPrefix, sizeof(kIPV6PatternPrefix),
                    kIPV6Address2Bytes, sizeof(kIPV6Address2Bytes));
  WakeOnWifi::ConfigureAddWakeOnPacketMsg(&msg4, all_addresses, index, &e);
  EXPECT_TRUE(WakeOnWifi::WakeOnPacketSettingsMatch(msg4, all_addresses));

  SetWakeOnPacketConnMessage msg5;
  all_addresses.AddUnique(
      IPAddress(string(kIPV6Address3, sizeof(kIPV6Address3))));
  expected_pattern =
      CreatePattern(kIPV6PatternPrefix, sizeof(kIPV6PatternPrefix),
                    kIPV6Address3Bytes, sizeof(kIPV6Address3Bytes));
  WakeOnWifi::ConfigureAddWakeOnPacketMsg(&msg5, all_addresses, index, &e);
  EXPECT_TRUE(WakeOnWifi::WakeOnPacketSettingsMatch(msg5, all_addresses));

  SetWakeOnPacketConnMessage msg6;
  all_addresses.AddUnique(
      IPAddress(string(kIPV6Address4, sizeof(kIPV6Address4))));
  expected_pattern =
      CreatePattern(kIPV6PatternPrefix, sizeof(kIPV6PatternPrefix),
                    kIPV6Address4Bytes, sizeof(kIPV6Address4Bytes));
  WakeOnWifi::ConfigureAddWakeOnPacketMsg(&msg6, all_addresses, index, &e);
  EXPECT_TRUE(WakeOnWifi::WakeOnPacketSettingsMatch(msg6, all_addresses));

  SetWakeOnPacketConnMessage msg7;
  all_addresses.AddUnique(
      IPAddress(string(kIPV6Address5, sizeof(kIPV6Address5))));
  expected_pattern =
      CreatePattern(kIPV6PatternPrefix, sizeof(kIPV6PatternPrefix),
                    kIPV6Address5Bytes, sizeof(kIPV6Address5Bytes));
  WakeOnWifi::ConfigureAddWakeOnPacketMsg(&msg7, all_addresses, index, &e);
  EXPECT_TRUE(WakeOnWifi::WakeOnPacketSettingsMatch(msg7, all_addresses));

  SetWakeOnPacketConnMessage msg8;
  all_addresses.AddUnique(
      IPAddress(string(kIPV6Address6, sizeof(kIPV6Address6))));
  expected_pattern =
      CreatePattern(kIPV6PatternPrefix, sizeof(kIPV6PatternPrefix),
                    kIPV6Address6Bytes, sizeof(kIPV6Address6Bytes));
  WakeOnWifi::ConfigureAddWakeOnPacketMsg(&msg8, all_addresses, index, &e);
  EXPECT_TRUE(WakeOnWifi::WakeOnPacketSettingsMatch(msg8, all_addresses));

  SetWakeOnPacketConnMessage msg9;
  all_addresses.AddUnique(
      IPAddress(string(kIPV6Address7, sizeof(kIPV6Address7))));
  expected_pattern =
      CreatePattern(kIPV6PatternPrefix, sizeof(kIPV6PatternPrefix),
                    kIPV6Address7Bytes, sizeof(kIPV6Address7Bytes));
  WakeOnWifi::ConfigureAddWakeOnPacketMsg(&msg9, all_addresses, index, &e);
  EXPECT_TRUE(WakeOnWifi::WakeOnPacketSettingsMatch(msg9, all_addresses));
}

}  // namespace shill
