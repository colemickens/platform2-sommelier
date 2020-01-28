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
#include <base/strings/stringprintf.h>
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

namespace {

using ::chromeos::cros_healthd::mojom::SmartBatteryInfo;

// The path used to check a device's master configuration hardware properties.
constexpr char kHardwarePropertiesPath[] = "/hardware-properties";
// The master configuration property that specifies a device's PSU type.
constexpr char kPsuTypeProperty[] = "psu-type";

// The path used to check a device's master configuration cros_healthd battery
// properties.
constexpr char kBatteryPropertiesPath[] = "/cros-healthd/battery";
// The master configuration property that indicates whether a device has Smart
// Battery info.
constexpr char kHasSmartBatteryInfoProperty[] = "has-smart-battery-info";

constexpr char kManufactureDateSmart[] = "manufacture_date_smart";
constexpr char kTemperatureSmart[] = "temperature_smart";

// Converts a Smart Battery manufacture date from the ((year - 1980) * 512 +
// month * 32 + day) format to yyyy-mm-dd format.
std::string ConvertSmartBatteryManufactureDate(int64_t manufacture_date) {
  int remainder = manufacture_date;
  int day = remainder % 32;
  remainder /= 32;
  int month = remainder % 16;
  remainder /= 16;
  int year = remainder + 1980;
  return base::StringPrintf("%04d-%02d-%02d", year, month, day);
}

}  // namespace

BatteryFetcher::BatteryFetcher(
    org::chromium::debugdProxyInterface* debugd_proxy,
    dbus::ObjectProxy* power_manager_proxy,
    brillo::CrosConfigInterface* cros_config)
    : debugd_proxy_(debugd_proxy),
      power_manager_proxy_(power_manager_proxy),
      cros_config_(cros_config) {
  DCHECK(debugd_proxy_);
  DCHECK(power_manager_proxy_);
  DCHECK(cros_config_);
}

BatteryFetcher::~BatteryFetcher() = default;

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

  // Only obtain Smart Battery metrics for devices that support them (i.e.
  // devices with a Smart Battery).
  std::string has_smart_battery_info;
  cros_config_->GetString(kBatteryPropertiesPath, kHasSmartBatteryInfoProperty,
                          &has_smart_battery_info);
  if (has_smart_battery_info == "true") {
    SmartBatteryInfo smart_info;
    int64_t manufacture_date;
    base::OnceCallback<bool(const base::StringPiece& input, int64_t* output)>
        convert_string_to_int =
            base::BindOnce([](const base::StringPiece& input, int64_t* output) {
              return base::StringToInt64(input, output);
            });
    smart_info.manufacture_date =
        FetchSmartBatteryMetric<int64_t>(kManufactureDateSmart,
                                         &manufacture_date,
                                         std::move(convert_string_to_int))
            ? ConvertSmartBatteryManufactureDate(manufacture_date)
            : "0000-00-00";
    uint64_t temperature;
    base::OnceCallback<bool(const base::StringPiece& input, uint64_t* output)>
        convert_string_to_uint = base::BindOnce(
            [](const base::StringPiece& input, uint64_t* output) {
              return base::StringToUint64(input, output);
            });
    smart_info.temperature =
        FetchSmartBatteryMetric<uint64_t>(kTemperatureSmart, &temperature,
                                          std::move(convert_string_to_uint))
            ? temperature
            : 0;
    info.smart_battery_info = smart_info.Clone();
  }

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

  std::string psu_type;
  cros_config_->GetString(kHardwarePropertiesPath, kPsuTypeProperty, &psu_type);
  if (psu_type != "AC_only") {
    BatteryInfoPtr info;
    if (FetchBatteryMetrics(&info)) {
      batteries.push_back(std::move(info));
    }
  }

  return batteries;
}

}  // namespace diagnostics
