// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "diagnostics/cros_healthd/utils/battery_utils.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <base/bind.h>
#include <base/process/launch.h>
#include <base/files/file_util.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/string_util.h>
#include <base/time/time.h>
#include <base/values.h>
#include <chromeos/dbus/service_constants.h>
#include <dbus/bus.h>
#include <dbus/message.h>
#include <dbus/object_proxy.h>
#include <re2/re2.h>

#include "power_manager/proto_bindings/power_supply_properties.pb.h"

namespace diagnostics {

BatteryFetcher::BatteryFetcher(
    org::chromium::debugdProxyInterface* debugd_proxy,
    dbus::ObjectProxy* power_manager_proxy)
    : debugd_proxy_(debugd_proxy), power_manager_proxy_(power_manager_proxy) {
  DCHECK(debugd_proxy_);
  DCHECK(power_manager_proxy_);
}

BatteryFetcher::~BatteryFetcher() = default;

namespace {
constexpr char kManufactureDateSmart[] = "manufacture_date_smart";
constexpr char kTemperatureSmart[] = "temperature_smart";
}  // namespace

template <typename T>
bool BatteryFetcher::FetchSmartBatteryMetric(
    const std::string& metric_name,
    T* smart_metric,
    base::OnceCallback<bool(const base::StringPiece& input, T* output)>
        convert_string_to_num) {
  brillo::ErrorPtr error;
  constexpr int kTimeoutMs = 10 * 1000;
  std::string debugd_result;
  if (!debugd_proxy_->CollectSmartBatteryMetric(metric_name, &debugd_result,
                                                &error, kTimeoutMs)) {
    LOG(ERROR) << "Failed retrieving " << metric_name
               << " from debugd: " << error->GetCode() << " "
               << error->GetMessage();
    return false;
  }

  std::move(convert_string_to_num).Run(debugd_result, smart_metric);
  return true;
}

// Extract the battery metrics from the PowerSupplyProperties protobuf.
// Return true if the metrics could be successfully extracted from |response|
// and put into |output_info|.
bool BatteryFetcher::ExtractBatteryMetrics(dbus::Response* response,
                                           BatteryInfoPtr* output_info) {
  DCHECK(response);
  DCHECK(output_info);

  BatteryInfo info;
  power_manager::PowerSupplyProperties power_supply_proto;
  dbus::MessageReader reader_response(response);
  if (!reader_response.PopArrayOfBytesAsProto(&power_supply_proto)) {
    LOG(ERROR) << "Could not successfully read power supply protobuf";
    return false;
  }
  info.cycle_count = power_supply_proto.has_battery_cycle_count()
                         ? power_supply_proto.battery_cycle_count()
                         : 0;
  info.vendor = power_supply_proto.has_battery_vendor()
                    ? power_supply_proto.battery_vendor()
                    : "";
  info.voltage_now = power_supply_proto.has_battery_voltage()
                         ? power_supply_proto.battery_voltage()
                         : 0.0;
  info.charge_full = power_supply_proto.has_battery_charge_full()
                         ? power_supply_proto.battery_charge_full()
                         : 0.0;
  info.charge_full_design =
      power_supply_proto.has_battery_charge_full_design()
          ? power_supply_proto.battery_charge_full_design()
          : 0.0;
  info.serial_number = power_supply_proto.has_battery_serial_number()
                           ? power_supply_proto.battery_serial_number()
                           : "";
  info.voltage_min_design =
      power_supply_proto.has_battery_voltage_min_design()
          ? power_supply_proto.battery_voltage_min_design()
          : 0.0;
  info.model_name = power_supply_proto.has_battery_model_name()
                        ? power_supply_proto.battery_model_name()
                        : "";
  info.charge_now = power_supply_proto.has_battery_charge()
                        ? power_supply_proto.battery_charge()
                        : 0;
  int64_t manufacture_date_smart;
  base::OnceCallback<bool(const base::StringPiece& input, int64_t* output)>
      convert_string_to_int =
          base::BindOnce([](const base::StringPiece& input, int64_t* output) {
            return base::StringToInt64(input, output);
          });
  info.manufacture_date_smart =
      FetchSmartBatteryMetric<int64_t>(kManufactureDateSmart,
                                       &manufacture_date_smart,
                                       std::move(convert_string_to_int))
          ? manufacture_date_smart
          : 0;
  uint64_t temperature_smart;
  base::OnceCallback<bool(const base::StringPiece& input, uint64_t* output)>
      convert_string_to_uint =
          base::BindOnce([](const base::StringPiece& input, uint64_t* output) {
            return base::StringToUint64(input, output);
          });
  info.temperature_smart =
      FetchSmartBatteryMetric<uint64_t>(kTemperatureSmart, &temperature_smart,
                                        std::move(convert_string_to_uint))
          ? temperature_smart
          : 0;

  *output_info = info.Clone();

  return true;
}

// Make a D-Bus call to get the PowerSupplyProperties proto, which contains the
// battery metrics.
bool BatteryFetcher::FetchBatteryMetrics(BatteryInfoPtr* output_info) {
  constexpr base::TimeDelta kPowerManagerDBusTimeout =
      base::TimeDelta::FromSeconds(3);
  dbus::MethodCall method_call(power_manager::kPowerManagerInterface,
                               power_manager::kGetPowerSupplyPropertiesMethod);
  auto response = power_manager_proxy_->CallMethodAndBlock(
      &method_call, kPowerManagerDBusTimeout.InMilliseconds());
  return ExtractBatteryMetrics(response.get(), output_info);
}

std::vector<BatteryFetcher::BatteryInfoPtr> BatteryFetcher::FetchBatteryInfo() {
  // Since Chromebooks currently only support a single battery (main battery),
  // the vector should have a size of one. In the future, if Chromebooks
  // contain more batteries, they can easily be supported by the vector.
  std::vector<BatteryInfoPtr> batteries{};

  BatteryInfoPtr info;
  if (FetchBatteryMetrics(&info)) {
    batteries.push_back(std::move(info));
  }

  return batteries;
}

}  // namespace diagnostics
