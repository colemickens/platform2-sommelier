// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdlib>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <utility>

#include <base/at_exit.h>
#include <base/logging.h>
#include <base/message_loop/message_loop.h>
#include <base/optional.h>
#include <base/strings/stringprintf.h>
#include <brillo/flag_helper.h>
#include <brillo/syslog_logging.h>

#include "diagnostics/cros_healthd_mojo_adapter/cros_healthd_mojo_adapter.h"
#include "mojo/cros_healthd_probe.mojom.h"

namespace {

constexpr std::pair<const char*,
                    chromeos::cros_healthd::mojom::ProbeCategoryEnum>
    kCategorySwitches[] = {
        {"battery", chromeos::cros_healthd::mojom::ProbeCategoryEnum::kBattery},
        {"storage", chromeos::cros_healthd::mojom::ProbeCategoryEnum::
                        kNonRemovableBlockDevices},
        {"cached_vpd",
         chromeos::cros_healthd::mojom::ProbeCategoryEnum::kCachedVpdData},
};

void DisplayBatteryInfo(
    const chromeos::cros_healthd::mojom::BatteryInfoPtr& battery) {
  DCHECK(!battery.is_null());

  printf(
      "charge_full,charge_full_design,cycle_count,serial_number,"
      "vendor(manufacturer),voltage_now,voltage_min_design,"
      "manufacture_date_smart,temperature_smart,model_name,charge_now\n");

  printf("%f,%f,%ld,%s,%s,%f,%f,%ld,%ld,%s,%f\n", battery->charge_full,
         battery->charge_full_design, battery->cycle_count,
         battery->serial_number.c_str(), battery->vendor.c_str(),
         battery->voltage_now, battery->voltage_min_design,
         battery->manufacture_date_smart, battery->temperature_smart,
         battery->model_name.c_str(), battery->charge_now);
}

void DisplayBlockDeviceInfo(
    const std::vector<
        chromeos::cros_healthd::mojom::NonRemovableBlockDeviceInfoPtr>&
        block_devices) {
  printf("path,size,type,manfid,name,serial\n");

  for (const auto& device : block_devices) {
    printf("%s,%ld,%s,0x%x,%s,0x%x\n", device->path.c_str(), device->size,
           device->type.c_str(), static_cast<int>(device->manufacturer_id),
           device->name.c_str(), device->serial);
  }
}

void DisplayCachedVpdInfo(
    const chromeos::cros_healthd::mojom::CachedVpdInfoPtr& vpd) {
  DCHECK(!vpd.is_null());

  printf("sku_number\n");

  printf("%s\n", vpd->sku_number.c_str());
}

// Displays the retrieved telemetry information to the console.
void DisplayTelemetryInfo(
    const chromeos::cros_healthd::mojom::TelemetryInfoPtr& info) {
  const auto& battery = info->battery_info;
  if (!battery.is_null())
    DisplayBatteryInfo(battery);

  const auto& block_devices = info->block_device_info;
  if (block_devices)
    DisplayBlockDeviceInfo(block_devices.value());

  const auto& vpd = info->vpd_info;
  if (!vpd.is_null())
    DisplayCachedVpdInfo(vpd);
}

// Create a stringified list of the category names for use in help.
std::string GetCategoryHelp() {
  std::stringstream ss;
  ss << "Category to probe: [";
  const char* sep = "";
  for (auto pair : kCategorySwitches) {
    ss << sep << pair.first;
    sep = ", ";
  }
  ss << "]";
  return ss.str();
}

}  // namespace

// 'telem' command-line tool:
//
// Test driver for cros_healthd's telemetry collection. Supports requesting a
// single category at a time.
int main(int argc, char** argv) {
  std::string category_help = GetCategoryHelp();
  DEFINE_string(category, "", category_help.c_str());
  brillo::FlagHelper::Init(argc, argv, "telem - Device telemetry tool.");
  brillo::InitLog(brillo::kLogToSyslog | brillo::kLogToStderrIfTty);

  base::AtExitManager at_exit_manager;

  std::map<std::string, chromeos::cros_healthd::mojom::ProbeCategoryEnum>
      switch_to_category(std::begin(kCategorySwitches),
                         std::end(kCategorySwitches));

  logging::InitLogging(logging::LoggingSettings());

  base::MessageLoopForIO message_loop;

  // Make sure at least one category is specified.
  if (FLAGS_category == "") {
    LOG(ERROR) << "No category specified.";
    return EXIT_FAILURE;
  }
  // Validate the category flag.
  auto iterator = switch_to_category.find(FLAGS_category);
  if (iterator == switch_to_category.end()) {
    LOG(ERROR) << "Invalid category: " << FLAGS_category;
    return EXIT_FAILURE;
  }

  // Probe and display the category.
  diagnostics::CrosHealthdMojoAdapter adapter;
  const std::vector<chromeos::cros_healthd::mojom::ProbeCategoryEnum>
      categories_to_probe = {iterator->second};
  DisplayTelemetryInfo(adapter.GetTelemetryInfo(categories_to_probe));

  return EXIT_SUCCESS;
}
