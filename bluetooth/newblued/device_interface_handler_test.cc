// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bluetooth/newblued/device_interface_handler.h"

#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "bluetooth/newblued/uuid.h"

using ::testing::Pair;
using ::testing::UnorderedElementsAre;

namespace bluetooth {

TEST(DeviceInterfaceHandlerTest, UpdateEirNormal) {
  Device device;
  uint8_t eir[] = {
      // Flag
      3, static_cast<uint8_t>(EirType::FLAGS), 0xAA, 0xBB,
      // UUID16_COMPLETE - Battery Service
      3, static_cast<uint8_t>(EirType::UUID16_COMPLETE), 0x0F, 0x18,
      // UUID32_INCOMPLETE - Blood Pressure
      5, static_cast<uint8_t>(EirType::UUID32_INCOMPLETE), 0x10, 0x18, 0x00,
      0x00,
      // UUID128_COMPLETE
      17, static_cast<uint8_t>(EirType::UUID128_COMPLETE), 0x0F, 0x0E, 0x0D,
      0x0C, 0x0B, 0x0A, 0x09, 0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01,
      0x00,
      // Name
      4, static_cast<uint8_t>(EirType::NAME_SHORT), 'f', 'o', 'o',
      // TX Power
      2, static_cast<uint8_t>(EirType::TX_POWER), 0xC7,
      // Class
      4, static_cast<uint8_t>(EirType::CLASS_OF_DEV), 0x01, 0x02, 0x03,
      // Service data associated with 16-bit Battery Service UUID
      5, static_cast<uint8_t>(EirType::SVC_DATA16), 0x0F, 0x18, 0x22, 0x11,
      // Service data associate with 32-bit Bond Management Service UUID
      7, static_cast<uint8_t>(EirType::SVC_DATA32), 0x1E, 0x18, 0x00, 0x00,
      0x44, 0x33,
      // Appearance
      3, static_cast<uint8_t>(EirType::GAP_APPEARANCE), 0x01, 0x02,
      // Manufacturer data
      5, static_cast<uint8_t>(EirType::MANUFACTURER_DATA), 0x0E, 0x00, 0x55,
      0x66};
  Uuid battery_service_uuid16(std::vector<uint8_t>({0x18, 0x0F}));
  Uuid blood_pressure_uuid32(std::vector<uint8_t>({0x00, 0x00, 0x18, 0x10}));
  Uuid bond_management_service_uuid32(
      std::vector<uint8_t>({0x00, 0x00, 0x18, 0x1E}));
  Uuid uuid128(
      std::vector<uint8_t>({0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                            0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F}));

  DeviceInterfaceHandler::UpdateEir(
      &device, std::vector<uint8_t>(eir, eir + arraysize(eir)));

  EXPECT_EQ(std::vector<uint8_t>({0xAA}), device.flags.value());
  EXPECT_THAT(device.service_uuids.value(),
              UnorderedElementsAre(battery_service_uuid16,
                                   blood_pressure_uuid32, uuid128));
  EXPECT_EQ("foo", device.name.value());
  EXPECT_EQ(-57, device.tx_power.value());
  EXPECT_EQ(0x00030201, device.eir_class.value());
  EXPECT_THAT(device.service_data.value(),
              UnorderedElementsAre(Pair(battery_service_uuid16,
                                        std::vector<uint8_t>({0x11, 0x22})),
                                   Pair(bond_management_service_uuid32,
                                        std::vector<uint8_t>({0x33, 0x44}))));
  EXPECT_EQ(0x0201, device.appearance.value());
  EXPECT_THAT(
      device.manufacturer.value(),
      UnorderedElementsAre(Pair(0x000E, std::vector<uint8_t>({0x55, 0x66}))));

  uint8_t eir2[] = {
      // Flag with zero octet
      1, static_cast<uint8_t>(EirType::FLAGS),
      // UUID32_INCOMPLETE - Bond Management Service
      5, static_cast<uint8_t>(EirType::UUID32_INCOMPLETE), 0x1E, 0x18, 0x00,
      0x00,
      // Service data associate with 32-bit Bond Management Service UUID
      7, static_cast<uint8_t>(EirType::SVC_DATA32), 0x1E, 0x18, 0x00, 0x00,
      0x66, 0x55};

  DeviceInterfaceHandler::UpdateEir(
      &device, std::vector<uint8_t>(eir2, eir2 + arraysize(eir2)));

  EXPECT_FALSE(device.flags.value().empty());
  EXPECT_THAT(device.service_uuids.value(),
              UnorderedElementsAre(bond_management_service_uuid32));
  EXPECT_EQ("foo", device.name.value());
  EXPECT_EQ(-57, device.tx_power.value());
  EXPECT_EQ(0x00030201, device.eir_class.value());
  EXPECT_THAT(device.service_data.value(),
              UnorderedElementsAre(Pair(bond_management_service_uuid32,
                                        std::vector<uint8_t>({0x55, 0x66}))));
  EXPECT_EQ(0x0201, device.appearance.value());
  EXPECT_THAT(
      device.manufacturer.value(),
      UnorderedElementsAre(Pair(0x000E, std::vector<uint8_t>({0x55, 0x66}))));
}

TEST(DeviceInterfaceHandlerTest, UpdateEirAbnormal) {
  Device device;
  uint8_t eir[] = {
      // Even if there are more than one instance of a UUID size of either
      // COMPLETE or INCOMPLETE type, the later one will still be honored
      3, static_cast<uint8_t>(EirType::UUID16_COMPLETE), 0x0F, 0x18,  //
      3, static_cast<uint8_t>(EirType::UUID16_INCOMPLETE), 0x10, 0x18,
      // Invalid UUID will be dropped.
      2, static_cast<uint8_t>(EirType::UUID32_INCOMPLETE), 0x10,
      // Contains non-ascii character
      5, static_cast<uint8_t>(EirType::NAME_SHORT), 0x80, 0x81, 'a', '\0',
      // TX Power with more than one octet will be dropped
      3, static_cast<uint8_t>(EirType::TX_POWER), 0xC7, 0x00,
      // Class with a wrong field length (2, should be 3)
      3, static_cast<uint8_t>(EirType::CLASS_OF_DEV), 0x01, 0x02,
      // Service data with an invalid service UUID will be dropped
      3, static_cast<uint8_t>(EirType::SVC_DATA16), 0x0F, 0x18,
      // Service data with zero length associated with 16-bit Battery Service
      // will be dropped
      3, static_cast<uint8_t>(EirType::SVC_DATA16), 0x0F, 0x18,
      // Wrong field length (4, should be 3)
      4, static_cast<uint8_t>(EirType::GAP_APPEARANCE), 0x01, 0x02, 0x03};
  Uuid battery_service_uuid16(std::vector<uint8_t>({0x18, 0x0F}));
  Uuid blood_pressure_uuid16(std::vector<uint8_t>({0x18, 0x10}));

  DeviceInterfaceHandler::UpdateEir(
      &device, std::vector<uint8_t>(eir, eir + arraysize(eir)));

  // Non-ascii characters are replaced with spaces.
  EXPECT_FALSE(device.flags.value().empty());
  EXPECT_THAT(
      device.service_uuids.value(),
      UnorderedElementsAre(battery_service_uuid16, blood_pressure_uuid16));
  EXPECT_EQ("  a", device.name.value());
  EXPECT_EQ(-128, device.tx_power.value());
  EXPECT_EQ(0x1F00, device.eir_class.value());
  EXPECT_TRUE(device.service_data.value().empty());
  EXPECT_EQ(0x0000, device.appearance.value());
  EXPECT_THAT(device.manufacturer.value(),
              UnorderedElementsAre(Pair(0xFFFF, std::vector<uint8_t>())));
}

}  // namespace bluetooth
