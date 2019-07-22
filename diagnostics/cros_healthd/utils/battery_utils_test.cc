// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "diagnostics/cros_healthd/utils/battery_utils.h"

#include <map>
#include <memory>
#include <utility>

#include <base/message_loop/message_loop.h>
#include <base/time/time.h>
#include <dbus/message.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "power_manager/proto_bindings/power_supply_properties.pb.h"

namespace diagnostics {

using ::chromeos::cros_healthd::mojom::BatteryInfoPtr;
using ::testing::_;

bool ExtractBatteryMetrics(dbus::Response* response,
                           BatteryInfoPtr* output_info);

namespace {

const char kBatteryVendor[] = "TEST_MFR";
const double kBatteryVoltage = 127.45;
const int kBatteryCycleCount = 2;
const char kBatterySerialNumber[] = "1000";
const double kBatteryVoltageMinDesign = 114.00;
const double kBatteryChargeFull = 4.3;
const double kBatteryChargeFullDesign = 3.92;
const int kBatteryManufactureDateSmart = 0;

// Test the expected path of extracting battery metrics from a D-bus response.
TEST(BatteryUtils, TestExtractingBatteryMetrics) {
  // Create PowerSupplyProperties response protobuf.
  std::unique_ptr<dbus::Response> response = dbus::Response::CreateEmpty();
  dbus::MessageWriter writer(response.get());
  power_manager::PowerSupplyProperties power_supply_proto;
  power_supply_proto.set_battery_vendor(kBatteryVendor);
  power_supply_proto.set_battery_voltage(kBatteryVoltage);
  power_supply_proto.set_battery_cycle_count(kBatteryCycleCount);
  power_supply_proto.set_battery_charge_full(kBatteryChargeFull);
  power_supply_proto.set_battery_charge_full_design(kBatteryChargeFullDesign);
  power_supply_proto.set_battery_serial_number(kBatterySerialNumber);
  power_supply_proto.set_battery_voltage_min_design(kBatteryVoltageMinDesign);
  writer.AppendProtoAsArrayOfBytes(power_supply_proto);

  BatteryInfoPtr info;

  ExtractBatteryMetrics(response.get(), &info);
  ASSERT_EQ(kBatteryCycleCount, info->cycle_count);
  ASSERT_EQ(kBatteryVendor, info->vendor);
  ASSERT_EQ(kBatteryVoltage, info->voltage_now);
  ASSERT_EQ(kBatteryChargeFull, info->charge_full);
  ASSERT_EQ(kBatteryChargeFullDesign, info->charge_full_design);
  ASSERT_EQ(kBatterySerialNumber, info->serial_number);
  ASSERT_EQ(kBatteryVoltageMinDesign, info->voltage_min_design);
  ASSERT_EQ(kBatteryManufactureDateSmart, info->manufacture_date_smart);
}

// Test that ExtractBatteryMetrics exits safely (returning false) when it
// receves a bad D-bus response.
TEST(BatteryUtils, TestBadDbusResponse) {
  std::unique_ptr<dbus::Response> response = dbus::Response::CreateEmpty();
  dbus::MessageWriter writer(response.get());
  writer.AppendVariantOfBool(false);

  dbus::MessageReader reader_response(response.get());
  power_manager::PowerSupplyProperties power_supply_properties_proto;
  ASSERT_FALSE(
      reader_response.PopArrayOfBytesAsProto(&power_supply_properties_proto));

  BatteryInfoPtr info;

  ASSERT_FALSE(ExtractBatteryMetrics(response.get(), &info));
}

}  // namespace

}  // namespace diagnostics
