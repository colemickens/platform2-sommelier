// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bluetooth/common/util.h"

#include <vector>

#include <bits/stdint-uintn.h>
#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <newblue/uuid.h>

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

TEST(UtilTest, TrimAdapterFromObjectPath) {
  std::string path("org/bluez");
  std::string path2("/org/bluez/hcix");
  std::string path3("/org/bluez/hci0/dev_12_34_56_78_9A_BC");
  std::string path4("/org/bluez/hci10");

  EXPECT_FALSE(TrimAdapterFromObjectPath(&path));
  EXPECT_EQ("org/bluez", path);
  EXPECT_FALSE(TrimAdapterFromObjectPath(&path2));
  EXPECT_EQ("/org/bluez/hcix", path2);
  EXPECT_FALSE(TrimAdapterFromObjectPath(&path3));
  EXPECT_EQ("/org/bluez/hci0/dev_12_34_56_78_9A_BC", path3);
  EXPECT_TRUE(TrimAdapterFromObjectPath(&path4));
  EXPECT_TRUE(path4.empty());
}

TEST(UtilTest, TrimDeviceFromObjectPath) {
  std::string path("dev_12_34_56_78_9A_BC");
  std::string path2("/dev_12_34_56");
  std::string path3("/dev_12_34_56_78_9A_BC");
  std::string path4("/org/bluez/hci0/dev_12_34_56_78_9a_bc");

  EXPECT_EQ("", TrimDeviceFromObjectPath(&path));
  EXPECT_EQ("dev_12_34_56_78_9A_BC", path);
  EXPECT_EQ("", TrimDeviceFromObjectPath(&path2));
  EXPECT_EQ("/dev_12_34_56", path2);
  EXPECT_EQ("12:34:56:78:9A:BC", TrimDeviceFromObjectPath(&path3));
  EXPECT_EQ("", path3);
  EXPECT_EQ("12:34:56:78:9a:bc", TrimDeviceFromObjectPath(&path4));
  EXPECT_EQ("/org/bluez/hci0", path4);
}

TEST(UtilTest, TrimServiceFromObjectPath) {
  std::string path("service01");
  std::string path2("/service1FF");
  std::string path3("/service001F");
  std::string path4("/dev_12_34_56_78_9A_BC/service001F");

  EXPECT_EQ(kInvalidServiceHandle, TrimServiceFromObjectPath(&path));
  EXPECT_EQ("service01", path);
  EXPECT_EQ(kInvalidServiceHandle, TrimServiceFromObjectPath(&path2));
  EXPECT_EQ("/service1FF", path2);
  EXPECT_EQ(0x001F, TrimServiceFromObjectPath(&path3));
  EXPECT_TRUE(path3.empty());
  EXPECT_EQ(0x001F, TrimServiceFromObjectPath(&path4));
  EXPECT_EQ("/dev_12_34_56_78_9A_BC", path4);
}

TEST(UtilTest, TrimCharacteristicFromObjectPath) {
  std::string path("char0123");
  std::string path2("/charxxxx");
  std::string path3("/char01FFF");
  std::string path4("/char01ff");
  std::string path5("/service01FF/char01FF");

  EXPECT_EQ(kInvalidCharacteristicHandle,
            TrimCharacteristicFromObjectPath(&path));
  EXPECT_EQ("char0123", path);
  EXPECT_EQ(kInvalidCharacteristicHandle,
            TrimCharacteristicFromObjectPath(&path2));
  EXPECT_EQ("/charxxxx", path2);
  EXPECT_EQ(kInvalidCharacteristicHandle,
            TrimCharacteristicFromObjectPath(&path3));
  EXPECT_EQ("/char01FFF", path3);
  EXPECT_EQ(0x01FF, TrimCharacteristicFromObjectPath(&path4));
  EXPECT_TRUE(path4.empty());
  EXPECT_EQ(0x01FF, TrimCharacteristicFromObjectPath(&path5));
  EXPECT_EQ("/service01FF", path5);
}

TEST(UtilTest, TrimDescriptorFromObjectPath) {
  std::string path("descriptor01F");
  std::string path2("/descriptor01F");
  std::string path3("/descriptor01ff");
  std::string path4("/char0123/descriptor01FF");

  EXPECT_EQ(kInvalidDescriptorHandle, TrimDescriptorFromObjectPath(&path));
  EXPECT_EQ("descriptor01F", path);
  EXPECT_EQ(kInvalidDescriptorHandle, TrimDescriptorFromObjectPath(&path2));
  EXPECT_EQ("/descriptor01F", path2);
  EXPECT_EQ(0x01FF, TrimDescriptorFromObjectPath(&path3));
  EXPECT_TRUE(path3.empty());
  EXPECT_EQ(0x01FF, TrimDescriptorFromObjectPath(&path4));
  EXPECT_EQ("/char0123", path4);
}

TEST(UtilTest, ConvertToObjectPath) {
  std::string address("11:22:33:44:55:66");
  std::string dev_p = std::string(device_object_prefix) + "11_22_33_44_55_66";
  uint16_t sh = 0x01FF;
  std::string sp("/service01FF");
  uint16_t ch = 0x01FF;
  std::string cp("/char01FF");
  uint16_t dh = 0x01FF;
  std::string dp("/descriptor01FF");

  // device
  EXPECT_TRUE(ConvertDeviceAddressToObjectPath("").empty());
  EXPECT_EQ(dev_p, ConvertDeviceAddressToObjectPath(address));

  // service
  EXPECT_EQ(dev_p + sp, ConvertServiceHandleToObjectPath(address, sh));

  // characteristic
  EXPECT_EQ(dev_p + sp + cp,
            ConvertCharacteristicHandleToObjectPath(address, sh, ch));

  // descriptor
  EXPECT_EQ(dev_p + sp + cp + dp,
            ConvertDescriptorHandleToObjectPath(address, sh, ch, dh));
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

TEST(UtilTest, ConvertServiceObjectPathToHandle) {
  std::string path("/org/bluez");
  std::string path2(
      "/org/bluez/hci0/dev_00_01_02_03_04_05/service001F/char0123");
  std::string path3("/org/bluez/hci0/dev_00_01_02_03_04_05/service001F");

  std::string address("");
  uint16_t h = 0x0000;

  EXPECT_FALSE(ConvertServiceObjectPathToHandle(&address, &h, path));
  EXPECT_TRUE(address.empty());
  EXPECT_EQ(0x0000, h);

  EXPECT_FALSE(ConvertServiceObjectPathToHandle(&address, &h, path2));
  EXPECT_TRUE(address.empty());
  EXPECT_EQ(0x0000, h);

  EXPECT_TRUE(ConvertServiceObjectPathToHandle(&address, &h, path3));
  EXPECT_EQ("00:01:02:03:04:05", address);
  EXPECT_EQ(0x001F, h);
}

TEST(UtilTest, ConvertCharacteristicObjectPathToHandles) {
  std::string path("/org/bluez");
  std::string path2(
      "/org/bluez/hci0/dev_00_01_02_03_04_05/service001F/char0123/"
      "descriptor01FF");
  std::string path3(
      "/org/bluez/hci0/dev_00_01_02_03_04_05/service001F/char0123");

  std::string address("");
  uint16_t sh = 0x0000;
  uint16_t ch = 0x0000;

  EXPECT_FALSE(
      ConvertCharacteristicObjectPathToHandles(&address, &sh, &ch, path));
  EXPECT_TRUE(address.empty());
  EXPECT_EQ(0x0000, sh);
  EXPECT_EQ(0x0000, ch);

  EXPECT_FALSE(
      ConvertCharacteristicObjectPathToHandles(&address, &sh, &ch, path2));
  EXPECT_TRUE(address.empty());
  EXPECT_EQ(0x0000, sh);
  EXPECT_EQ(0x0000, ch);

  EXPECT_TRUE(
      ConvertCharacteristicObjectPathToHandles(&address, &sh, &ch, path3));
  EXPECT_EQ("00:01:02:03:04:05", address);
  EXPECT_EQ(0x001F, sh);
  EXPECT_EQ(0x0123, ch);
}

TEST(UtilTest, ConvertDescriptorObjectPathToHandles) {
  std::string path("/org/bluez");
  std::string path2(
      "/org/bluez/hci0/dev_00_01_02_03_04_05/service001F/char0123/"
      "descriptor001F");

  std::string address("");
  uint16_t sh = 0x0000;
  uint16_t ch = 0x0000;
  uint16_t dh = 0x0000;

  EXPECT_FALSE(
      ConvertDescriptorObjectPathToHandles(&address, &sh, &ch, &dh, path));
  EXPECT_TRUE(address.empty());
  EXPECT_EQ(0x0000, sh);
  EXPECT_EQ(0x0000, ch);
  EXPECT_EQ(0x0000, dh);

  EXPECT_TRUE(
      ConvertDescriptorObjectPathToHandles(&address, &sh, &ch, &dh, path2));
  EXPECT_EQ("00:01:02:03:04:05", address);
  EXPECT_EQ(0x001F, sh);
  EXPECT_EQ(0x0123, ch);
  EXPECT_EQ(0x001F, dh);
}

TEST(UtilTest, ConvertToUuid) {
  struct uuid uuid;
  const std::array<uint8_t, 16> expected_value = {
      0x00, 0x00, 0x12, 0x34, 0x00, 0x00, 0x10, 0x00,
      0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb,
  };
  const std::string expected_canonical_value(
      "00001234-0000-1000-8000-00805f9b34fb");

  uuidFromUuid16(&uuid, 0x1234);

  Uuid result = ConvertToUuid(uuid);
  EXPECT_EQ(UuidFormat::UUID128, result.format());
  EXPECT_EQ(expected_value, result.value());
  EXPECT_EQ(expected_canonical_value, result.canonical_value());
}

TEST(UtilTest, ConvertToRawUuid) {
  Uuid uuid_invalid;
  Uuid uuid_16(std::vector<uint8_t>({0x1a, 0x2b}));
  Uuid uuid_32(std::vector<uint8_t>({0x00, 0x00, 0x1a, 0x2b}));
  Uuid uuid_128(
      std::vector<uint8_t>({0x00, 0x00, 0x1a, 0x2b, 0x00, 0x00, 0x10, 0x00,
                            0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb}));

  struct uuid u_invalid = ConvertToRawUuid(uuid_invalid);
  struct uuid u_16 = ConvertToRawUuid(uuid_16);
  struct uuid u_32 = ConvertToRawUuid(uuid_32);
  struct uuid u_128 = ConvertToRawUuid(uuid_128);

  EXPECT_TRUE(uuidIsZero(&u_invalid));
  EXPECT_EQ(0x00001a2b00001000, u_16.hi);
  EXPECT_EQ(0x800000805f9b34fb, u_16.lo);
  EXPECT_TRUE(uuidCmp(&u_16, &u_32));
  EXPECT_TRUE(uuidCmp(&u_16, &u_128));
}

}  // namespace
}  // namespace bluetooth
