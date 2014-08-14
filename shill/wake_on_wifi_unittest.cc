// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include <gtest/gtest.h>

#include "shill/byte_string.h"
#include "shill/error.h"
#include "shill/ip_address.h"
#include "shill/nl80211_message.h"
#include "shill/testing.h"
#include "shill/wake_on_wifi.h"

namespace shill {

namespace {
// Zero-byte pattern prefixes to match the offsetting bytes in the ethernet
// frame that lie before the source ip address field
static const uint8_t kIPV4PatternPrefix[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static const uint8_t kIPV6PatternPrefix[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

// These masks have bits set to 1 to match bytes in an IP address pattern that
// represent the source IP address of the frame. They are padded with zero
// bits in front to ignore the frame offset and at the end to byte-align the
// mask itself
static const uint8_t kIPV4MaskBytes[] = {0x00, 0x00, 0x00, 0x3c};
static const uint8_t kIPV6MaskBytes[] = {0x00, 0x00, 0xc0, 0xff, 0x3f};

static const char kEmptyIPAddress[] = "";
static const char kIPV4Address0[] = "192.168.10.20";
static const uint8_t kIPV4Address0Bytes[] = {0xc0, 0xa8, 0x0a, 0x14};
static const char kIPV4Address1[] = "1.2.3.4";
static const uint8_t kIPV4Address1Bytes[] = {0x01, 0x02, 0x03, 0x04};

static const char kIPV6Address0[] = "FEDC:BA98:7654:3210:FEDC:BA98:7654:3210";
static const uint8_t kIPV6Address0Bytes[] = {0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54,
                                             0x32, 0x10, 0xfe, 0xdc, 0xba, 0x98,
                                             0x76, 0x54, 0x32, 0x10};
static const char kIPV6Address1[] = "1080:0:0:0:8:800:200C:417A";
static const uint8_t kIPV6Address1Bytes[] = {0x10, 0x80, 0x00, 0x00, 0x00, 0x00,
                                             0x00, 0x00, 0x00, 0x08, 0x08, 0x00,
                                             0x20, 0x0c, 0x41, 0x7a};
static const char kIPV6Address2[] = "1080::8:800:200C:417A";
static const uint8_t kIPV6Address2Bytes[] = {0x10, 0x80, 0x00, 0x00, 0x00, 0x00,
                                             0x00, 0x00, 0x00, 0x08, 0x08, 0x00,
                                             0x20, 0x0c, 0x41, 0x7a};
static const char kIPV6Address3[] = "FF01::101";
static const uint8_t kIPV6Address3Bytes[] = {0xff, 0x01, 0x00, 0x00, 0x00, 0x00,
                                             0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                             0x00, 0x00, 0x01, 0x01};
static const char kIPV6Address4[] = "::1";
static const uint8_t kIPV6Address4Bytes[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                             0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                             0x00, 0x00, 0x00, 0x01};
static const char kIPV6Address5[] = "::";
static const uint8_t kIPV6Address5Bytes[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                             0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                             0x00, 0x00, 0x00, 0x00};
static const char kIPV6Address6[] = "0:0:0:0:0:FFFF:129.144.52.38";
static const uint8_t kIPV6Address6Bytes[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                             0x00, 0x00, 0x00, 0x00, 0xff, 0xff,
                                             0x81, 0x90, 0x34, 0x26};
static const char kIPV6Address7[] = "::DEDE:190.144.52.38";
static const uint8_t kIPV6Address7Bytes[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                             0x00, 0x00, 0x00, 0x00, 0xde, 0xde,
                                             0xbe, 0x90, 0x34, 0x26};
}  // namespace

ByteString CreatePattern(const unsigned char *prefix, size_t prefix_len,
                         const unsigned char *addr, size_t addr_len) {
  ByteString result(prefix, prefix_len);
  result.Append(ByteString(addr, addr_len));
  return result;
}

IPAddress MakeIPV4(const char *addr) {
  std::string ip_string(addr, strlen(addr));
  IPAddress ip_addr(IPAddress::kFamilyIPv4);
  ip_addr.SetAddressFromString(ip_string);
  return ip_addr;
}

IPAddress MakeIPV6(const char *addr) {
  std::string ip_string(addr, strlen(addr));
  IPAddress ip_addr(IPAddress::kFamilyIPv6);
  ip_addr.SetAddressFromString(ip_string);
  return ip_addr;
}

TEST(WifiUtilTest, CreateIPAddressPatternAndMask) {
  ByteString pattern;
  ByteString mask;
  ByteString expected_pattern;

  WakeOnWifi::CreateIPAddressPatternAndMask(MakeIPV4(kIPV4Address0), &pattern,
                                            &mask);
  expected_pattern =
      CreatePattern(kIPV4PatternPrefix, sizeof(kIPV4PatternPrefix),
                    kIPV4Address0Bytes, sizeof(kIPV4Address0Bytes));
  EXPECT_TRUE(pattern.Equals(expected_pattern));
  EXPECT_TRUE(mask.Equals(ByteString(kIPV4MaskBytes, sizeof(kIPV4MaskBytes))));

  pattern.Clear();
  expected_pattern.Clear();
  mask.Clear();
  WakeOnWifi::CreateIPAddressPatternAndMask(MakeIPV4(kIPV4Address1), &pattern,
                                            &mask);
  expected_pattern =
      CreatePattern(kIPV4PatternPrefix, sizeof(kIPV4PatternPrefix),
                    kIPV4Address1Bytes, sizeof(kIPV4Address1Bytes));
  EXPECT_TRUE(pattern.Equals(expected_pattern));
  EXPECT_TRUE(mask.Equals(ByteString(kIPV4MaskBytes, sizeof(kIPV4MaskBytes))));

  pattern.Clear();
  expected_pattern.Clear();
  mask.Clear();
  WakeOnWifi::CreateIPAddressPatternAndMask(MakeIPV6(kIPV6Address0), &pattern,
                                            &mask);
  expected_pattern =
      CreatePattern(kIPV6PatternPrefix, sizeof(kIPV6PatternPrefix),
                    kIPV6Address0Bytes, sizeof(kIPV6Address0Bytes));
  EXPECT_TRUE(pattern.Equals(expected_pattern));
  EXPECT_TRUE(mask.Equals(ByteString(kIPV6MaskBytes, sizeof(kIPV6MaskBytes))));

  pattern.Clear();
  expected_pattern.Clear();
  mask.Clear();
  WakeOnWifi::CreateIPAddressPatternAndMask(MakeIPV6(kIPV6Address1), &pattern,
                                            &mask);
  expected_pattern =
      CreatePattern(kIPV6PatternPrefix, sizeof(kIPV6PatternPrefix),
                    kIPV6Address1Bytes, sizeof(kIPV6Address1Bytes));
  EXPECT_TRUE(pattern.Equals(expected_pattern));
  EXPECT_TRUE(mask.Equals(ByteString(kIPV6MaskBytes, sizeof(kIPV6MaskBytes))));

  pattern.Clear();
  expected_pattern.Clear();
  mask.Clear();
  WakeOnWifi::CreateIPAddressPatternAndMask(MakeIPV6(kIPV6Address2), &pattern,
                                            &mask);
  expected_pattern =
      CreatePattern(kIPV6PatternPrefix, sizeof(kIPV6PatternPrefix),
                    kIPV6Address2Bytes, sizeof(kIPV6Address2Bytes));
  EXPECT_TRUE(pattern.Equals(expected_pattern));
  EXPECT_TRUE(mask.Equals(ByteString(kIPV6MaskBytes, sizeof(kIPV6MaskBytes))));

  pattern.Clear();
  expected_pattern.Clear();
  mask.Clear();
  WakeOnWifi::CreateIPAddressPatternAndMask(MakeIPV6(kIPV6Address3), &pattern,
                                            &mask);
  expected_pattern =
      CreatePattern(kIPV6PatternPrefix, sizeof(kIPV6PatternPrefix),
                    kIPV6Address3Bytes, sizeof(kIPV6Address3Bytes));
  EXPECT_TRUE(pattern.Equals(expected_pattern));
  EXPECT_TRUE(mask.Equals(ByteString(kIPV6MaskBytes, sizeof(kIPV6MaskBytes))));

  pattern.Clear();
  expected_pattern.Clear();
  mask.Clear();
  WakeOnWifi::CreateIPAddressPatternAndMask(MakeIPV6(kIPV6Address4), &pattern,
                                            &mask);
  expected_pattern =
      CreatePattern(kIPV6PatternPrefix, sizeof(kIPV6PatternPrefix),
                    kIPV6Address4Bytes, sizeof(kIPV6Address4Bytes));
  EXPECT_TRUE(pattern.Equals(expected_pattern));
  EXPECT_TRUE(mask.Equals(ByteString(kIPV6MaskBytes, sizeof(kIPV6MaskBytes))));

  pattern.Clear();
  expected_pattern.Clear();
  mask.Clear();
  WakeOnWifi::CreateIPAddressPatternAndMask(MakeIPV6(kIPV6Address5), &pattern,
                                            &mask);
  expected_pattern =
      CreatePattern(kIPV6PatternPrefix, sizeof(kIPV6PatternPrefix),
                    kIPV6Address5Bytes, sizeof(kIPV6Address5Bytes));
  EXPECT_TRUE(pattern.Equals(expected_pattern));
  EXPECT_TRUE(mask.Equals(ByteString(kIPV6MaskBytes, sizeof(kIPV6MaskBytes))));

  pattern.Clear();
  expected_pattern.Clear();
  mask.Clear();
  WakeOnWifi::CreateIPAddressPatternAndMask(MakeIPV6(kIPV6Address6), &pattern,
                                            &mask);
  expected_pattern =
      CreatePattern(kIPV6PatternPrefix, sizeof(kIPV6PatternPrefix),
                    kIPV6Address6Bytes, sizeof(kIPV6Address6Bytes));
  EXPECT_TRUE(pattern.Equals(expected_pattern));
  EXPECT_TRUE(mask.Equals(ByteString(kIPV6MaskBytes, sizeof(kIPV6MaskBytes))));

  pattern.Clear();
  expected_pattern.Clear();
  mask.Clear();
  WakeOnWifi::CreateIPAddressPatternAndMask(MakeIPV6(kIPV6Address7), &pattern,
                                            &mask);
  expected_pattern =
      CreatePattern(kIPV6PatternPrefix, sizeof(kIPV6PatternPrefix),
                    kIPV6Address7Bytes, sizeof(kIPV6Address7Bytes));
  EXPECT_TRUE(pattern.Equals(expected_pattern));
  EXPECT_TRUE(mask.Equals(ByteString(kIPV6MaskBytes, sizeof(kIPV6MaskBytes))));
}

TEST(WifiUtilTest, ConfigureWiphyIndex) {
  SetWakeOnPacketConnMessage msg;
  uint32_t value;
  EXPECT_FALSE(
      msg.attributes()->GetU32AttributeValue(NL80211_ATTR_WIPHY, &value));

  WakeOnWifi::ConfigureWiphyIndex(&msg, 137);
  EXPECT_TRUE(
      msg.attributes()->GetU32AttributeValue(NL80211_ATTR_WIPHY, &value));
  EXPECT_EQ(value, 137);
}

TEST(WifiUtilTest, ConfigureRemoveAllWakeOnPacketMsg) {
  SetWakeOnPacketConnMessage msg;
  Error e;
  uint32_t value;
  EXPECT_FALSE(
      msg.attributes()->GetU32AttributeValue(NL80211_ATTR_WIPHY, &value));

  WakeOnWifi::ConfigureRemoveAllWakeOnPacketMsg(&msg, 57, &e);
  EXPECT_EQ(e.type(), Error::Type::kSuccess);
  EXPECT_TRUE(
      msg.attributes()->GetU32AttributeValue(NL80211_ATTR_WIPHY, &value));
  EXPECT_EQ(value, 57);
}

bool AddWakeOnPacketMsgAttributesSet(SetWakeOnPacketConnMessage *msg) {
  AttributeListRefPtr triggers;
  AttributeListRefPtr patterns;
  AttributeListRefPtr pattern_info;
  ByteString mask;
  ByteString pattern;
  uint32_t value;
  uint32_t offset;
  uint8_t patnum = 1;
  if (!msg->attributes()->GetU32AttributeValue(NL80211_ATTR_WIPHY, &value)) {
    return false;
  }
  if (!msg->attributes()->GetNestedAttributeList(NL80211_ATTR_WOWLAN_TRIGGERS,
                                                 &triggers)) {
    return false;
  }
  if (!triggers->GetNestedAttributeList(NL80211_WOWLAN_TRIG_PKT_PATTERN,
                                        &patterns)) {
    return false;
  }
  if (!patterns->GetNestedAttributeList(patnum, &pattern_info)) {
    return false;
  }

  if (!pattern_info->GetRawAttributeValue(NL80211_PKTPAT_MASK, &mask)) {
    return false;
  }
  if (!pattern_info->GetRawAttributeValue(NL80211_PKTPAT_PATTERN, &pattern)) {
    return false;
  }
  if (!pattern_info->GetU32AttributeValue(NL80211_PKTPAT_OFFSET, &offset)) {
    return false;
  }
  if (offset != 0) {
    return false;
  }
  return true;
}

bool AddWakeOnPacketMsgPatternAndMaskMatch(SetWakeOnPacketConnMessage *msg,
                                           ByteString expected_pattern,
                                           ByteString expected_mask) {
  AttributeListRefPtr triggers;
  AttributeListRefPtr patterns;
  AttributeListRefPtr pattern_info;
  ByteString msg_mask;
  ByteString msg_pattern;

  uint8_t patnum = 1;
  if (!msg->attributes()->GetNestedAttributeList(NL80211_ATTR_WOWLAN_TRIGGERS,
                                                 &triggers)) {
    return false;
  }
  if (!triggers->GetNestedAttributeList(NL80211_WOWLAN_TRIG_PKT_PATTERN,
                                        &patterns)) {
    return false;
  }
  if (!patterns->GetNestedAttributeList(patnum, &pattern_info)) {
    return false;
  }
  if (!pattern_info->GetRawAttributeValue(NL80211_PKTPAT_PATTERN,
                                          &msg_pattern)) {
    return false;
  }
  if (!pattern_info->GetRawAttributeValue(NL80211_PKTPAT_MASK, &msg_mask)) {
    return false;
  }
  if (!expected_pattern.Equals(msg_pattern)) {
    LOG(INFO) << "Expected pattern: " << expected_pattern.HexEncode();
    LOG(INFO) << "Actual pattern: " << msg_pattern.HexEncode();
    return false;
  }
  if (!expected_mask.Equals(msg_mask)) {
    LOG(INFO) << "Expected mask: " << expected_mask.HexEncode();
    LOG(INFO) << "Actual mask: " << msg_mask.HexEncode();
    return false;
  }
  return true;
}

TEST(WifiUtilTest, ConfigureAddWakeOnPacketMsg) {
  SetWakeOnPacketConnMessage msg0;
  int index = 1;
  Error e;
  ByteString expected_mask = ByteString(kIPV4MaskBytes, sizeof(kIPV4MaskBytes));
  ByteString expected_pattern =
      CreatePattern(kIPV4PatternPrefix, sizeof(kIPV4PatternPrefix),
                    kIPV4Address0Bytes, sizeof(kIPV4Address0Bytes));
  WakeOnWifi::ConfigureAddWakeOnPacketMsg(&msg0, MakeIPV4(kIPV4Address0), index,
                                          &e);
  EXPECT_TRUE(AddWakeOnPacketMsgAttributesSet(&msg0));
  EXPECT_TRUE(AddWakeOnPacketMsgPatternAndMaskMatch(&msg0, expected_pattern,
                                                    expected_mask));

  SetWakeOnPacketConnMessage msg1;
  expected_pattern =
      CreatePattern(kIPV4PatternPrefix, sizeof(kIPV4PatternPrefix),
                    kIPV4Address1Bytes, sizeof(kIPV4Address1Bytes));
  WakeOnWifi::ConfigureAddWakeOnPacketMsg(&msg1, MakeIPV4(kIPV4Address1), index,
                                          &e);
  EXPECT_TRUE(AddWakeOnPacketMsgAttributesSet(&msg1));
  EXPECT_TRUE(AddWakeOnPacketMsgPatternAndMaskMatch(&msg1, expected_pattern,
                                                    expected_mask));

  SetWakeOnPacketConnMessage msg2;
  expected_mask = ByteString(kIPV6MaskBytes, sizeof(kIPV6MaskBytes));
  expected_pattern =
      CreatePattern(kIPV6PatternPrefix, sizeof(kIPV6PatternPrefix),
                    kIPV6Address0Bytes, sizeof(kIPV6Address0Bytes));
  WakeOnWifi::ConfigureAddWakeOnPacketMsg(&msg2, MakeIPV6(kIPV6Address0), index,
                                          &e);
  EXPECT_TRUE(AddWakeOnPacketMsgAttributesSet(&msg2));
  EXPECT_TRUE(AddWakeOnPacketMsgPatternAndMaskMatch(&msg2, expected_pattern,
                                                    expected_mask));

  SetWakeOnPacketConnMessage msg3;
  expected_pattern =
      CreatePattern(kIPV6PatternPrefix, sizeof(kIPV6PatternPrefix),
                    kIPV6Address1Bytes, sizeof(kIPV6Address1Bytes));
  WakeOnWifi::ConfigureAddWakeOnPacketMsg(&msg3, MakeIPV6(kIPV6Address1), index,
                                          &e);
  EXPECT_TRUE(AddWakeOnPacketMsgAttributesSet(&msg3));
  EXPECT_TRUE(AddWakeOnPacketMsgPatternAndMaskMatch(&msg3, expected_pattern,
                                                    expected_mask));

  SetWakeOnPacketConnMessage msg4;
  expected_pattern =
      CreatePattern(kIPV6PatternPrefix, sizeof(kIPV6PatternPrefix),
                    kIPV6Address2Bytes, sizeof(kIPV6Address2Bytes));
  WakeOnWifi::ConfigureAddWakeOnPacketMsg(&msg4, MakeIPV6(kIPV6Address2), index,
                                          &e);
  EXPECT_TRUE(AddWakeOnPacketMsgAttributesSet(&msg4));
  EXPECT_TRUE(AddWakeOnPacketMsgPatternAndMaskMatch(&msg4, expected_pattern,
                                                    expected_mask));

  SetWakeOnPacketConnMessage msg5;
  expected_pattern =
      CreatePattern(kIPV6PatternPrefix, sizeof(kIPV6PatternPrefix),
                    kIPV6Address3Bytes, sizeof(kIPV6Address3Bytes));
  WakeOnWifi::ConfigureAddWakeOnPacketMsg(&msg5, MakeIPV6(kIPV6Address3), index,
                                          &e);
  EXPECT_TRUE(AddWakeOnPacketMsgAttributesSet(&msg5));
  EXPECT_TRUE(AddWakeOnPacketMsgPatternAndMaskMatch(&msg5, expected_pattern,
                                                    expected_mask));

  SetWakeOnPacketConnMessage msg6;
  expected_pattern =
      CreatePattern(kIPV6PatternPrefix, sizeof(kIPV6PatternPrefix),
                    kIPV6Address4Bytes, sizeof(kIPV6Address4Bytes));
  WakeOnWifi::ConfigureAddWakeOnPacketMsg(&msg6, MakeIPV6(kIPV6Address4), index,
                                          &e);
  EXPECT_TRUE(AddWakeOnPacketMsgAttributesSet(&msg6));
  EXPECT_TRUE(AddWakeOnPacketMsgPatternAndMaskMatch(&msg6, expected_pattern,
                                                    expected_mask));

  SetWakeOnPacketConnMessage msg7;
  expected_pattern =
      CreatePattern(kIPV6PatternPrefix, sizeof(kIPV6PatternPrefix),
                    kIPV6Address5Bytes, sizeof(kIPV6Address5Bytes));
  WakeOnWifi::ConfigureAddWakeOnPacketMsg(&msg7, MakeIPV6(kIPV6Address5), index,
                                          &e);
  EXPECT_TRUE(AddWakeOnPacketMsgAttributesSet(&msg7));
  EXPECT_TRUE(AddWakeOnPacketMsgPatternAndMaskMatch(&msg7, expected_pattern,
                                                    expected_mask));

  SetWakeOnPacketConnMessage msg8;
  expected_pattern =
      CreatePattern(kIPV6PatternPrefix, sizeof(kIPV6PatternPrefix),
                    kIPV6Address6Bytes, sizeof(kIPV6Address6Bytes));
  WakeOnWifi::ConfigureAddWakeOnPacketMsg(&msg8, MakeIPV6(kIPV6Address6), index,
                                          &e);
  EXPECT_TRUE(AddWakeOnPacketMsgAttributesSet(&msg8));
  EXPECT_TRUE(AddWakeOnPacketMsgPatternAndMaskMatch(&msg8, expected_pattern,
                                                    expected_mask));

  SetWakeOnPacketConnMessage msg9;
  expected_pattern =
      CreatePattern(kIPV6PatternPrefix, sizeof(kIPV6PatternPrefix),
                    kIPV6Address7Bytes, sizeof(kIPV6Address7Bytes));
  WakeOnWifi::ConfigureAddWakeOnPacketMsg(&msg9, MakeIPV6(kIPV6Address7), index,
                                          &e);
  EXPECT_TRUE(AddWakeOnPacketMsgAttributesSet(&msg9));
  EXPECT_TRUE(AddWakeOnPacketMsgPatternAndMaskMatch(&msg9, expected_pattern,
                                                    expected_mask));
}

}  // namespace shill
