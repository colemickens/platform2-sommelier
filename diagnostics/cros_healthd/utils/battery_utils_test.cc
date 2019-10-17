// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "diagnostics/cros_healthd/utils/battery_utils.h"

#include <map>
#include <memory>
#include <utility>

#include <base/message_loop/message_loop.h>
#include <base/time/time.h>
#include <chromeos/dbus/service_constants.h>
#include <brillo/dbus/dbus_connection.h>
#include <dbus/message.h>
#include <dbus/mock_bus.h>
#include <dbus/mock_object_proxy.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "debugd/dbus-proxy-mocks.h"
#include "power_manager/proto_bindings/power_supply_properties.pb.h"

namespace diagnostics {

using ::chromeos::cros_healthd::mojom::BatteryInfoPtr;
using ::dbus::MockBus;
using ::dbus::MockObjectProxy;
using ::testing::_;
using ::testing::Return;
using ::testing::StrictMock;

namespace {

constexpr char kBatteryVendor[] = "TEST_MFR";
constexpr double kBatteryVoltage = 127.45;
constexpr int kBatteryCycleCount = 2;
constexpr char kBatterySerialNumber[] = "1000";
constexpr double kBatteryVoltageMinDesign = 114.00;
constexpr double kBatteryChargeFull = 4.3;
constexpr double kBatteryChargeFullDesign = 3.92;
constexpr char kBatteryModelName[] = "TEST_MODEL_NAME";
constexpr double kBatteryChargeNow = 5.17;
// Since the BatteryFetcher will not be able to speak to debugd via D-Bus in the
// unit test, we expect that the reported manufacture_date_smart and
// temperature_smart will be the default value of 0.
constexpr int kBatteryManufactureDateSmart = 0;
constexpr int kBatteryTemperatureSmart = 0;
constexpr base::TimeDelta kDebugdTimeOut =
    base::TimeDelta::FromMilliseconds(10 * 1000);

}  // namespace

class BatteryUtilsTest : public ::testing::Test {
 protected:
  BatteryUtilsTest() {
    mock_proxy_ = std::make_unique<org::chromium::debugdProxyMock>();
    battery_fetcher_ = std::make_unique<BatteryFetcher>(mock_proxy_.get());
    options_.bus_type = dbus::Bus::SYSTEM;
    mock_bus_ = new dbus::MockBus(options_);
    mock_object_proxy_ =
        new dbus::MockObjectProxy(mock_bus_.get(), debugd::kDebugdServiceName,
                                  dbus::ObjectPath(debugd::kDebugdServicePath));
  }

  bool ExtractBatteryMetrics(dbus::Response* response,
                             BatteryInfoPtr* output_info) {
    return battery_fetcher_.get()->ExtractBatteryMetrics(response, output_info);
  }

  org::chromium::debugdProxyMock* GetMockProxy() { return mock_proxy_.get(); }

  dbus::MockObjectProxy* GetMockObjectProxy() {
    return mock_object_proxy_.get();
  }

 private:
  std::unique_ptr<org::chromium::debugdProxyMock> mock_proxy_;
  std::unique_ptr<BatteryFetcher> battery_fetcher_;
  dbus::Bus::Options options_;
  scoped_refptr<dbus::MockBus> mock_bus_;
  scoped_refptr<dbus::MockObjectProxy> mock_object_proxy_;
};

// Test the expected path of extracting battery metrics from a D-Bus response.
TEST_F(BatteryUtilsTest, TestExtractingBatteryMetrics) {
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
  power_supply_proto.set_battery_model_name(kBatteryModelName);
  power_supply_proto.set_battery_charge(kBatteryChargeNow);

  writer.AppendProtoAsArrayOfBytes(power_supply_proto);

  BatteryInfoPtr info;

  EXPECT_CALL(*GetMockProxy(), GetObjectProxy())
      .Times(2)
      .WillRepeatedly(testing::Return(GetMockObjectProxy()));

  EXPECT_CALL(*GetMockObjectProxy(),
              MockCallMethodAndBlock(_, kDebugdTimeOut.InMilliseconds()))
      .Times(2)
      .WillRepeatedly(testing::Return(nullptr));

  ExtractBatteryMetrics(response.get(), &info);
  ASSERT_TRUE(info);
  EXPECT_EQ(kBatteryCycleCount, info->cycle_count);
  EXPECT_EQ(kBatteryVendor, info->vendor);
  EXPECT_EQ(kBatteryVoltage, info->voltage_now);
  EXPECT_EQ(kBatteryChargeFull, info->charge_full);
  EXPECT_EQ(kBatteryChargeFullDesign, info->charge_full_design);
  EXPECT_EQ(kBatterySerialNumber, info->serial_number);
  EXPECT_EQ(kBatteryVoltageMinDesign, info->voltage_min_design);
  EXPECT_EQ(kBatteryModelName, info->model_name);
  EXPECT_EQ(kBatteryChargeNow, info->charge_now);
  EXPECT_EQ(kBatteryManufactureDateSmart, info->manufacture_date_smart);
  EXPECT_EQ(kBatteryTemperatureSmart, info->temperature_smart);
}

// Test that ExtractBatteryMetrics exits safely (returning false) when it
// receives a bad D-Bus response.
TEST_F(BatteryUtilsTest, TestBadDbusResponse) {
  std::unique_ptr<dbus::Response> response = dbus::Response::CreateEmpty();
  dbus::MessageWriter writer(response.get());
  writer.AppendVariantOfBool(false);

  dbus::MessageReader reader_response(response.get());
  power_manager::PowerSupplyProperties power_supply_properties_proto;
  ASSERT_FALSE(
      reader_response.PopArrayOfBytesAsProto(&power_supply_properties_proto));

  BatteryInfoPtr info;

  EXPECT_FALSE(ExtractBatteryMetrics(response.get(), &info));
}

}  // namespace diagnostics
