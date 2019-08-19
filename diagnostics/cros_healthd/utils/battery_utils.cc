// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "diagnostics/cros_healthd/utils/battery_utils.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <base/process/launch.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/string_util.h>
#include <chromeos/dbus/service_constants.h>
#include <dbus/bus.h>
#include <dbus/message.h>
#include <dbus/object_proxy.h>
#include <re2/re2.h>

#include "power_manager/proto_bindings/power_supply_properties.pb.h"

namespace diagnostics {

using BatteryInfoPtr = ::chromeos::cros_healthd::mojom::BatteryInfoPtr;
using BatteryInfo = ::chromeos::cros_healthd::mojom::BatteryInfo;

// This is a temporary hack enabling the battery_prober to provide the
// manufacture_date_smart property on Sona devices. The proper way to do this
// will take some planning. Details will be tracked here:
// https://crbug.com/978615.
bool FetchManufactureDateSmart(int64_t* manufacture_date_smart) {
  const char mosys_command[] = "mosys";
  const char mosys_subcommand[] = "platform";
  const char platform_subcommand[] = "model";
  std::string model_name;
  if (!base::GetAppOutput(
          {mosys_command, mosys_subcommand, platform_subcommand},
          &model_name)) {
    return false;
  }
  // CollapseWhitespaceASCII() is used to remove the newline from model_name.
  // This string is collected as output from the terminal.
  if (base::CollapseWhitespaceASCII(base::ToLowerASCII(model_name), true) !=
      "sona") {
    return false;
  }
  std::string ectool_output;
  // The command follows the format:
  // ectool i2cread <8 | 16> <port> <addr8> <offset>
  constexpr char num_bits[] = "16";
  constexpr char port[] = "2";
  constexpr char addr[] = "0x16";
  constexpr char offset[] = "0x1b";
  if (!base::GetAppOutput({"ectool", "i2cread", num_bits, port, addr, offset},
                          &ectool_output)) {
    return false;
  }
  constexpr auto kRegexPattern =
      R"(^Read from I2C port [\d]+ at .* offset .* = (.+)$)";
  std::string reg_value;
  // CollapseWhitespaceASCII is used to remove the newline from ectool_output.
  // This string is collected as output from the terminal.
  if (!RE2::PartialMatch(base::CollapseWhitespaceASCII(ectool_output, true),
                         kRegexPattern, &reg_value))
    return false;
  return base::HexStringToInt64(reg_value, manufacture_date_smart);
}

// Extract the battery metrics from the PowerSupplyProperties protobuf.
// Return true if the metrics could be successfully extracted from |response|
// and put into |output_info|.
bool ExtractBatteryMetrics(dbus::Response* response,
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
  int64_t manufacture_date_smart;
  info.manufacture_date_smart =
      FetchManufactureDateSmart(&manufacture_date_smart)
          ? manufacture_date_smart
          : 0;

  *output_info = info.Clone();

  return true;
}

namespace {

// Make a D-bus call to get the PowerSupplyProperties proto, which contains the
// battery metrics.
bool FetchBatteryMetrics(BatteryInfoPtr* output_info) {
  dbus::Bus::Options options;
  options.bus_type = dbus::Bus::SYSTEM;
  scoped_refptr<dbus::Bus> bus(new dbus::Bus(options));
  if (!bus->Connect()) {
    LOG(ERROR) << "Failed to connect to the system bus";
    return false;
  }
  dbus::ObjectProxy* bus_proxy = bus->GetObjectProxy(
      power_manager::kPowerManagerServiceName,
      dbus::ObjectPath(power_manager::kPowerManagerServicePath));
  constexpr base::TimeDelta kPowerManagerDBusTimeout =
      base::TimeDelta::FromSeconds(3);
  dbus::MethodCall method_call(power_manager::kPowerManagerInterface,
                               power_manager::kGetPowerSupplyPropertiesMethod);
  auto response = bus_proxy->CallMethodAndBlock(
      &method_call, kPowerManagerDBusTimeout.InMilliseconds());
  return ExtractBatteryMetrics(response.get(), output_info);
}

}  // namespace

std::vector<BatteryInfoPtr> FetchBatteryInfo() {
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
