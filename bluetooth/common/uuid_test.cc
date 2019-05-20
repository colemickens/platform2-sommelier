// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bluetooth/common/uuid.h"

#include <stdint.h>

#include <algorithm>
#include <array>
#include <vector>

#include <gtest/gtest.h>

namespace bluetooth {

TEST(UUIDTest, UuidConstruction) {
  std::vector<uint8_t> value_invalid = {0x01, 0x02, 0x03};

  std::vector<uint8_t> value16 = {0x01, 0x02};
  std::array<uint8_t, 16> uuid16_value = {
      0x00, 0x00, 0x01, 0x02, 0x00, 0x00, 0x10, 0x00,
      0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb,
  };

  std::vector<uint8_t> value32 = {0x01, 0x02, 0x03, 0x04};
  std::array<uint8_t, 16> uuid32_value = {
      0x01, 0x02, 0x03, 0x04, 0x00, 0x00, 0x10, 0x00,
      0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb,
  };

  std::vector<uint8_t> value128 = {
      0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
      0x08, 0x09, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15,
  };
  std::array<uint8_t, 16> uuid128_value;

  std::copy(value128.begin(), value128.end(), uuid128_value.begin());

  Uuid uuid16(value16);
  Uuid uuid32(value32);
  Uuid uuid128(value128);
  Uuid uuid_invalid(value_invalid);

  EXPECT_EQ(UuidFormat::UUID16, uuid16.format());
  EXPECT_EQ(UuidFormat::UUID32, uuid32.format());
  EXPECT_EQ(UuidFormat::UUID128, uuid128.format());
  EXPECT_EQ(UuidFormat::UUID_INVALID, uuid_invalid.format());

  EXPECT_EQ(uuid16_value, uuid16.value());
  EXPECT_EQ(uuid32_value, uuid32.value());
  EXPECT_EQ(uuid128_value, uuid128.value());

  Uuid uuid_16("0102");
  Uuid uuid_16_invalid("102");
  EXPECT_EQ(UuidFormat::UUID16, uuid_16.format());
  EXPECT_EQ(uuid16_value, uuid_16.value());
  EXPECT_EQ(UuidFormat::UUID_INVALID, uuid_16_invalid.format());

  Uuid uuid_32("01020304");
  Uuid uuid_32_invalid("0102030405");
  EXPECT_EQ(UuidFormat::UUID32, uuid_32.format());
  EXPECT_EQ(uuid32_value, uuid_32.value());
  EXPECT_EQ(UuidFormat::UUID_INVALID, uuid_32_invalid.format());

  Uuid uuid_128("00010203-0405-0607-0809-101112131415");
  Uuid uuid_too_long("00010203-0405-0607-0809-1011-1213-1415");
  Uuid uuid_too_short("00010203-0405-0607-0809101112131415");
  Uuid uuid_non_digit("00010203-0405-0607-0809-1011-2131415");
  Uuid uuid_misplaced("0001-002030405-0607-0809-101112131415");
  Uuid uuid_at("00010203-0405-0607@0809-101112131415");
  EXPECT_EQ(UuidFormat::UUID128, uuid_128.format());
  EXPECT_EQ(uuid128_value, uuid_128.value());
  EXPECT_EQ(UuidFormat::UUID_INVALID, uuid_too_long.format());
  EXPECT_EQ(UuidFormat::UUID_INVALID, uuid_too_short.format());
  EXPECT_EQ(UuidFormat::UUID_INVALID, uuid_non_digit.format());
  EXPECT_EQ(UuidFormat::UUID_INVALID, uuid_misplaced.format());
  EXPECT_EQ(UuidFormat::UUID_INVALID, uuid_at.format());
}

TEST(UUIDTest, UuidOperators) {
  std::vector<uint8_t> value16 = {0x01, 0x02};
  std::vector<uint8_t> value16_in_128 = {
      0x00, 0x00, 0x01, 0x02, 0x00, 0x00, 0x10, 0x00,
      0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb,
  };

  std::vector<uint8_t> value32 = {0x01, 0x02, 0x03, 0x04};
  std::vector<uint8_t> value_invalid1 = {0x01};
  std::vector<uint8_t> value_invalid2 = {0x01, 0x02, 0x03};

  Uuid uuid16(value16);
  Uuid uuid16_in_128(value16_in_128);
  Uuid uuid32(value32);
  Uuid uuid_invalid1(value_invalid1);
  Uuid uuid_invalid2(value_invalid2);

  EXPECT_EQ(uuid16, uuid16_in_128);
  EXPECT_NE(uuid16, uuid_invalid1);
  EXPECT_NE(uuid16, uuid32);

  EXPECT_FALSE(uuid_invalid1 < uuid_invalid2);
  EXPECT_LT(uuid_invalid1, uuid16);
  EXPECT_LT(uuid16, uuid32);
}

}  // namespace bluetooth
