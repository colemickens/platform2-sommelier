// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bluetooth/newblued/gatt_attributes.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>

namespace bluetooth {

class GattTest : public ::testing::Test {};

TEST_F(GattTest, AttributesInit) {
  std::string empty_address("");
  std::string address("01:02:03:0A:0B:0C");
  Uuid service_uuid(std::vector<uint8_t>({0x12, 0x34}));
  Uuid characteristic_uuid(std::vector<uint8_t>({0x56, 0x78}));
  Uuid descriptor_uuid(std::vector<uint8_t>({0x56, 0x78}));
  Uuid included_service_uuid(std::vector<uint8_t>({0x9A, 0xBC}));

  // GattService fails to init.
  EXPECT_DEATH(GattService(empty_address, 0x0001, 0x0003, true, service_uuid),
               "");
  EXPECT_DEATH(GattService(address, 0x0003, 0x0001, true, service_uuid), "");

  // GattService inits successfully.
  GattService s(address, 0x0001, 0x0003, true, service_uuid);
  EXPECT_EQ(address, s.device_address());
  EXPECT_EQ(0x0001, s.first_handle());
  EXPECT_EQ(0x0003, s.last_handle());
  EXPECT_EQ(true, s.primary());
  EXPECT_EQ(service_uuid, s.uuid());

  // GattIncludedService fails to init.
  EXPECT_DEATH(GattIncludedService(nullptr, 0x0002, 0x0001, 0x0003,
                                   included_service_uuid),
               "");
  EXPECT_DEATH(
      GattIncludedService(&s, 0x0002, 0x0003, 0x0001, included_service_uuid),
      "");

  // GattIncludedService inits successfully.
  GattIncludedService ins(&s, 0x0002, 0x0001, 0x0003, included_service_uuid);
  EXPECT_EQ(&s, ins.service());
  EXPECT_EQ(0x0002, ins.included_handle());
  EXPECT_EQ(0x0001, ins.first_handle());
  EXPECT_EQ(0x0003, ins.last_handle());
  EXPECT_EQ(included_service_uuid, ins.uuid());

  // Gatt Characteristic fails to init.
  EXPECT_DEATH(GattCharacteristic(nullptr, 0x0005, 0x0004, 0x0006, 0xAB,
                                  characteristic_uuid),
               "");
  EXPECT_DEATH(
      GattCharacteristic(&s, 0x0005, 0x0006, 0x0004, 0xAB, characteristic_uuid),
      "");

  // GattCharacteristic inits successfully.
  GattCharacteristic c(&s, 0x0005, 0x0004, 0x0006, 0xAB, characteristic_uuid);
  EXPECT_EQ(&s, c.service());
  EXPECT_EQ(0x0005, c.value_handle());
  EXPECT_EQ(0x0004, c.first_handle());
  EXPECT_EQ(0x0006, c.last_handle());
  EXPECT_EQ(0xAB, c.properties());
  EXPECT_EQ(characteristic_uuid, c.uuid());
  EXPECT_TRUE(c.value().empty());
  EXPECT_EQ(GattCharacteristic::NotifySetting::NONE, c.notify_setting());

  // GattDescriptor fails to init.
  EXPECT_DEATH(GattDescriptor(nullptr, 0x0006, descriptor_uuid), "");

  // GattDescriptor inits successfully.
  GattDescriptor d(&c, 0x0006, descriptor_uuid);
  EXPECT_EQ(&c, d.characteristic());
  EXPECT_EQ(0x0006, d.handle());
  EXPECT_EQ(descriptor_uuid, d.uuid());
}

}  // namespace bluetooth
