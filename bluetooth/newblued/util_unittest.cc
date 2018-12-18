// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <bluetooth/common/util.h>

#include <vector>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

using testing::ElementsAre;

namespace bluetooth {
namespace {

constexpr char device_object_prefix[] = "/org/bluez/hci0/dev_";

const char* const kInvalidAddresses[] = {
    "",
    "11",
    "11:1:11:11:11:11",
    "11:11:11:11:11:11:",
    "11:11:11:1G:11:11",
    "11:11:11:11:11:11:11",
};

const char* const kInvalidDeviceObjectPathes[] = {
    "",
    "11",
    "11_1_11_11_11_11",
    "11_11_11_11_11_11_",
    "11_11_11_1G_11_11",
    "11_11_11_11_11_11_11",
};

TEST(UtilTest, GetFromLE) {
  uint8_t le16[] = {0x11, 0x22};
  uint8_t le24[] = {0x33, 0x44, 0x55};
  uint8_t le_bytes[] = {0x11, 0x22, 0x33, 0x44, 0x55};
  std::vector<uint8_t> expected_bytes({0x55, 0x44, 0x33, 0x22, 0x11});

  EXPECT_EQ(0x2211, GetNumFromLE16(le16));
  EXPECT_EQ(0x554433, GetNumFromLE24(le24));
  EXPECT_EQ(expected_bytes, GetBytesFromLE(le_bytes, sizeof(le_bytes)));

  EXPECT_TRUE(GetBytesFromLE(le_bytes, 0).empty());
}

TEST(UtilTest, GetNextId) {
  UniqueId id1 = GetNextId();
  UniqueId id2 = GetNextId();

  EXPECT_NE(kInvalidUniqueId, id1);
  EXPECT_NE(kInvalidUniqueId, id2);
  EXPECT_NE(id1, id2);
  EXPECT_LT(id1, id2);
}

TEST(UtilTest, ConvertToBtAddr) {
  for (const char* address : kInvalidAddresses) {
    struct bt_addr result {};
    EXPECT_FALSE(ConvertToBtAddr(false, address, &result));
    EXPECT_EQ(result.type, 0);
    EXPECT_THAT(result.addr, ElementsAre(0x00, 0x00, 0x00, 0x00, 0x00, 0x00));
  }
  {
    struct bt_addr result {};
    EXPECT_TRUE(ConvertToBtAddr(false, "12:34:56:78:9A:BC", &result));
    EXPECT_EQ(result.type, BT_ADDR_TYPE_LE_PUBLIC);
    EXPECT_THAT(result.addr, ElementsAre(0xBC, 0x9A, 0x78, 0x56, 0x34, 0x12));
  }
  {
    struct bt_addr result {};
    EXPECT_TRUE(ConvertToBtAddr(true, "CB:A9:87:65:43:21", &result));
    EXPECT_EQ(result.type, BT_ADDR_TYPE_LE_RANDOM);
    EXPECT_THAT(result.addr, ElementsAre(0x21, 0x43, 0x65, 0x87, 0xA9, 0xCB));
  }
}

TEST(UtilTest, ConvertDeviceObjectPathToAddress) {
  const std::string prefix = device_object_prefix;
  for (const char* address : kInvalidDeviceObjectPathes) {
    EXPECT_EQ("", ConvertDeviceObjectPathToAddress(address));
    EXPECT_EQ("", ConvertDeviceObjectPathToAddress(prefix + address));
  }
  EXPECT_EQ("", ConvertDeviceObjectPathToAddress("12_34_56_78_9A_BC"));
  EXPECT_EQ("12:34:56:78:9A:BC",
            ConvertDeviceObjectPathToAddress(prefix + "12_34_56_78_9A_BC"));
  EXPECT_EQ("12:34:56:78:9a:bc",
            ConvertDeviceObjectPathToAddress(prefix + "12_34_56_78_9a_bc"));
}

TEST(UtilTest, ConvertDeviceAddressToObjectPath) {
  EXPECT_EQ(std::string(device_object_prefix) + "11_22_33_44_55_66",
            ConvertDeviceAddressToObjectPath("11:22:33:44:55:66"));
}

}  // namespace
}  // namespace bluetooth
