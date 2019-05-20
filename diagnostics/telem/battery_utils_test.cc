// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "diagnostics/telem/battery_utils.h"

#include <map>
#include <string>
#include <utility>

#include <base/message_loop/message_loop.h>
#include <base/time/time.h>
#include <dbus/message.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "diagnostics/telem/cache_writer_impl.h"
#include "diagnostics/telem/telemetry.h"
#include "diagnostics/telem/telemetry_item_enum.h"
#include "power_manager/proto_bindings/power_supply_properties.pb.h"

using ::testing::_;

namespace diagnostics {

namespace {

const char kBatteryVendor[] = "TEST_MFR";
const double kBatteryVoltage = 127.45;
const int kBatteryCycleCount = 2;
const char kBatteryCycleCountStr[] = "2";

// Test the expected path of extracting battery metrics from a dbus response.
TEST(BatteryUtils, TestExtractingBatteryMetrics) {
  // Create PowerSupplyProperties response protobuf.
  std::unique_ptr<dbus::Response> response = dbus::Response::CreateEmpty();
  dbus::MessageWriter writer(response.get());
  power_manager::PowerSupplyProperties power_supply_proto;
  // Since base::Value() does not provide an int64 constructor, we must add it
  // to the cache as a string.
  power_supply_proto.set_battery_vendor(kBatteryVendor);
  power_supply_proto.set_battery_voltage(kBatteryVoltage);
  power_supply_proto.set_battery_cycle_count(kBatteryCycleCount);
  writer.AppendProtoAsArrayOfBytes(power_supply_proto);

  CacheWriterImpl test_cache;
  base::Optional<base::Value> battery_cycle_count = {};
  base::Optional<base::Value> battery_vendor = {};
  base::Optional<base::Value> battery_voltage = {};

  ExtractBatteryMetrics(response.get(), &battery_cycle_count, &battery_vendor,
                        &battery_voltage);
  ASSERT_EQ(battery_cycle_count,
            base::Optional<base::Value>(base::Value(kBatteryCycleCountStr)));
  ASSERT_EQ(battery_vendor,
            base::Optional<base::Value>(base::Value(kBatteryVendor)));
  ASSERT_EQ(battery_voltage,
            base::Optional<base::Value>(base::Value(kBatteryVoltage)));
}

// Test that receiving a bad response from dbus does not abruptly fail.
TEST(BatteryUtils, TestBadDbusResponse) {
  std::unique_ptr<dbus::Response> response = dbus::Response::CreateEmpty();
  dbus::MessageWriter writer(response.get());
  writer.AppendVariantOfBool(false);

  CacheWriterImpl test_cache;

  dbus::MessageReader reader_response(response.get());
  power_manager::PowerSupplyProperties power_supply_properties_proto;
  ASSERT_FALSE(
      reader_response.PopArrayOfBytesAsProto(&power_supply_properties_proto));

  base::Optional<base::Value> battery_cycle_count = {};
  base::Optional<base::Value> battery_vendor = {};
  base::Optional<base::Value> battery_voltage = {};

  ExtractBatteryMetrics(response.get(), &battery_cycle_count, &battery_vendor,
                        &battery_voltage);
  test_cache.CheckParsedDataIsNull(TelemetryItemEnum::kBatteryVoltage);
}

}  // namespace

}  // namespace diagnostics
