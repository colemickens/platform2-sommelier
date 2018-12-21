/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <string>
#include <utility>
#include <vector>

#include <base/files/file_enumerator.h>
#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/values.h>

#include "runtime_probe/functions/generic_battery.h"
#include "runtime_probe/utils/file_utils.h"

namespace runtime_probe {

GenericBattery::DataType GenericBattery::Eval() const {
  constexpr char kSysfsBatteryPath[] = "/sys/class/power_supply/BAT*";
  constexpr char kSysfsExpectedType[] = "Battery";
  const std::vector<std::string> keys{"manufacturer", "model_name",
                                      "technology", "type"};
  const std::vector<std::string> optional_keys{
      "capacity",           "capacity_level",
      "charge_full",        "charge_full_design",
      "charge_now",         "current_now",
      "cycle_count",        "present",
      "serial_number",      "status",
      "voltage_min_design", "voltage_now"};

  DataType result{};

  const base::FilePath glob_path{kSysfsBatteryPath};
  const auto glob_root = glob_path.DirName();
  const auto glob_pattern = glob_path.BaseName();

  base::FileEnumerator battery_it(glob_root, false,
                                  base::FileEnumerator::FileType::DIRECTORIES,
                                  glob_pattern.value());
  while (true) {
    // TODO(itspeter): Extra take care if there are multiple batteries.
    auto battery_path = battery_it.Next();
    if (battery_path.empty())
      break;

    auto dict_value = MapFilesToDict(battery_path, keys, optional_keys);
    if (!dict_value.empty()) {
      std::string power_supply_type;
      if (dict_value.GetString("type", &power_supply_type) &&
          power_supply_type != kSysfsExpectedType) {
        LOG(ERROR) << "power_supply_type [" << power_supply_type << "] is not ["
                   << kSysfsExpectedType << "] for " << battery_path.value();
        continue;
      }
      result.push_back(std::move(dict_value));
    }
  }

  if (result.size() > 1) {
    LOG(ERROR) << "Multiple batteries is not supported yet.";
    return {};
  }
  return result;
}

}  // namespace runtime_probe
