// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "diagnostics/cros_healthd/utils/battery_utils.h"

#include <memory>
#include <utility>
#include <vector>

#include <chromeos/dbus/service_constants.h>
#include <dbus/bus.h>
#include <dbus/message.h>
#include <dbus/object_proxy.h>

#include "power_manager/proto_bindings/power_supply_properties.pb.h"

namespace diagnostics {

using BatteryInfoPtr = ::chromeos::cros_healthd::mojom::BatteryInfoPtr;
using BatteryInfo = ::chromeos::cros_healthd::mojom::BatteryInfo;

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

  if (power_supply_proto.has_battery_cycle_count()) {
    info.cycle_count = power_supply_proto.battery_cycle_count();
  }
  if (power_supply_proto.has_battery_vendor()) {
    info.vendor = power_supply_proto.battery_vendor();
  }
  if (power_supply_proto.has_battery_voltage()) {
    info.voltage_now = power_supply_proto.battery_voltage();
  }
  if (power_supply_proto.has_battery_charge_full()) {
    info.charge_full = power_supply_proto.battery_charge_full();
  }
  if (power_supply_proto.has_battery_charge_full_design()) {
    info.charge_full_design = power_supply_proto.battery_charge_full_design();
  }
  if (power_supply_proto.has_battery_serial_number()) {
    info.serial_number = power_supply_proto.battery_serial_number();
  }
  if (power_supply_proto.has_battery_voltage_min_design()) {
    info.voltage_min_design = power_supply_proto.battery_voltage_min_design();
  }

  *output_info = info.Clone();

  return true;
}

namespace {

// Make a D-bus call to get the PowerSupplyProperties proto, which contains the
// battery metrics.
bool FetchBatteryMetrics(BatteryInfoPtr* output_info) {
  dbus::Bus::Options options;
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
