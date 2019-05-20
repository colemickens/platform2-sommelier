// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "diagnostics/telem/battery_utils.h"

#include <memory>

#include <base/strings/string_number_conversions.h>
#include <chromeos/dbus/service_constants.h>
#include <dbus/bus.h>
#include <dbus/message.h>
#include <dbus/object_proxy.h>

#include "diagnostics/telem/telem_cache.h"
#include "diagnostics/telem/telemetry.h"
#include "power_manager/proto_bindings/power_supply_properties.pb.h"

namespace diagnostics {

namespace {

constexpr base::TimeDelta kPowerManagerDBusTimeout =
    base::TimeDelta::FromSeconds(3);

}  // namespace

void ExtractBatteryMetrics(dbus::Response* response,
                           base::Optional<base::Value>* battery_cycle_count,
                           base::Optional<base::Value>* battery_vendor,
                           base::Optional<base::Value>* battery_voltage) {
  if (response == nullptr) {
    return;
  }
  power_manager::PowerSupplyProperties power_supply_proto;
  dbus::MessageReader reader_response(response);
  if (reader_response.PopArrayOfBytesAsProto(&power_supply_proto)) {
    if (power_supply_proto.has_battery_cycle_count()) {
      *battery_cycle_count = base::Optional<base::Value>(base::Value(
          base::IntToString(power_supply_proto.battery_cycle_count())));
    }
    if (power_supply_proto.has_battery_vendor()) {
      *battery_vendor = base::Optional<base::Value>(
          base::Value(power_supply_proto.battery_vendor()));
    }
    if (power_supply_proto.has_battery_voltage()) {
      *battery_voltage = base::Optional<base::Value>(
          base::Value(power_supply_proto.battery_voltage()));
    }
  } else {
    LOG(ERROR) << "Could not successfully read power supply protobuf";
  }
}

void FetchBatteryMetrics(CacheWriter* cache) {
  dbus::Bus::Options options;
  scoped_refptr<dbus::Bus> bus(new dbus::Bus(options));
  base::Optional<base::Value> battery_cycle_count = {};
  base::Optional<base::Value> battery_vendor = {};
  base::Optional<base::Value> battery_voltage = {};
  if (!bus->Connect()) {
    LOG(ERROR) << "Failed to connect to system bus";
  } else {
    dbus::ObjectProxy* bus_proxy = bus->GetObjectProxy(
        power_manager::kPowerManagerServiceName,
        dbus::ObjectPath(power_manager::kPowerManagerServicePath));
    dbus::MethodCall method_call(
        power_manager::kPowerManagerInterface,
        power_manager::kGetPowerSupplyPropertiesMethod);
    auto response = bus_proxy->CallMethodAndBlock(
        &method_call, kPowerManagerDBusTimeout.InMilliseconds());
    ExtractBatteryMetrics(response.get(), &battery_cycle_count, &battery_vendor,
                          &battery_voltage);
    cache->SetParsedData(TelemetryItemEnum::kBatteryCycleCount,
                         battery_cycle_count);
    cache->SetParsedData(TelemetryItemEnum::kBatteryManufacturer,
                         battery_vendor);
    cache->SetParsedData(TelemetryItemEnum::kBatteryVoltage, battery_voltage);
  }
}

}  // namespace diagnostics
