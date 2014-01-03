// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gflags/gflags.h>

#include <iomanip>
#include <iostream>
#include <string>

#include "base/at_exit.h"
#include "base/basictypes.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "power_manager/common/power_constants.h"
#include "power_manager/common/prefs.h"
#include "power_manager/powerd/system/power_supply.h"
#include "power_manager/powerd/system/udev_stub.h"

// Displays info about battery and line power.

using base::TimeDelta;

DEFINE_string(prefs_dir, "/var/lib/power_manager",
              "Directory containing prefs that can be changed at runtime.");
DEFINE_string(default_prefs_dir, "/usr/share/power_manager",
              "Directory containing default prefs.");

namespace {

// Path to power supply info.
const char kPowerStatusPath[] = "/sys/class/power_supply";

// Number of columns that should be used to display field names.
const int kFieldNameColumns = 22;

std::string BoolToString(bool value) {
  return value ? "yes" : "no";
}

template <class T>
std::string ValueToString(T value) {
  std::stringstream stream;
  stream << value;
  return stream.str();
}

class InfoDisplay {
 public:
  InfoDisplay() : name_indent_(0), value_indent_(0) {}

  void SetIndent(int name_indent, int value_indent) {
    name_indent_ = name_indent;
    value_indent_ = value_indent;
  }

  void PrintStringValue(const std::string& name_field,
                        const std::string& value_field) {
    std::cout << std::setw(name_indent_)
              << ""
              << std::setw(value_indent_ - name_indent_)
              << std::setiosflags(std::ios::left)
              << std::resetiosflags(std::ios::right)
              << name_field + ":"
              << value_field
              << std::endl;
  }

  template <class T>
  void PrintValue(const std::string& name_field, T value) {
    PrintStringValue(name_field, ValueToString(value));
  }

  void PrintString(const std::string& string) {
    std::cout << std::setw(name_indent_)
              << ""
              << string
              << std::endl;
  }

 private:
  int name_indent_;
  int value_indent_;

  DISALLOW_COPY_AND_ASSIGN(InfoDisplay);
};

}  // namespace

int main(int argc, char** argv) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  base::AtExitManager at_exit_manager;
  base::MessageLoopForIO message_loop;

  std::vector<base::FilePath> pref_paths;
  pref_paths.push_back(base::FilePath(FLAGS_prefs_dir));
  pref_paths.push_back(base::FilePath(FLAGS_default_prefs_dir));
  power_manager::Prefs prefs;
  CHECK(prefs.Init(pref_paths));

  power_manager::system::UdevStub udev;
  base::FilePath path(kPowerStatusPath);
  power_manager::system::PowerSupply power_supply;
  power_supply.Init(path, &prefs, &udev);

  CHECK(power_supply.RefreshImmediately());
  const power_manager::system::PowerStatus& status =
      power_supply.power_status();

  // NOTE, autotests (see autotest/files/client/cros/power_status.py) rely on
  // parsing this information below.
  // DO NOT CHANGE formatting without also fixing there as well.
  InfoDisplay display;
  display.SetIndent(0, 0);
  display.PrintString("Device: Line Power");
  display.SetIndent(2, kFieldNameColumns);
  display.PrintValue("path", status.line_power_path);
  display.PrintStringValue("online", BoolToString(status.line_power_on));
  display.PrintStringValue("type", status.line_power_type);
  switch (status.external_power) {
    case power_manager::PowerSupplyProperties_ExternalPower_AC:
      display.PrintStringValue("enum type", "AC");
      break;
    case power_manager::PowerSupplyProperties_ExternalPower_USB:
      display.PrintStringValue("enum type", "USB");
      break;
    case power_manager::PowerSupplyProperties_ExternalPower_DISCONNECTED:
      display.PrintStringValue("enum type", "Disconnected");
      break;
    default:
      display.PrintStringValue("enum type", "Unknown");
  }
  display.PrintStringValue("model name", status.line_power_model_name);
  display.PrintValue("voltage (V)", status.line_power_voltage);
  display.PrintValue("current (A)", status.line_power_current);

  if (status.battery_is_present) {
    display.SetIndent(0, 0);
    display.PrintString("Device: Battery");
    display.SetIndent(2, kFieldNameColumns);
    display.PrintValue("path", status.battery_path);
    display.PrintStringValue("vendor", status.battery_vendor);
    display.PrintStringValue("model name", status.battery_model_name);
    display.PrintStringValue("serial number", status.battery_serial);

    switch (status.battery_state) {
      case power_manager::PowerSupplyProperties_BatteryState_FULL:
        display.PrintStringValue("state", "Fully charged");
        break;
      case power_manager::PowerSupplyProperties_BatteryState_CHARGING:
        display.PrintStringValue("state", "Charging");
        break;
      case power_manager::PowerSupplyProperties_BatteryState_DISCHARGING:
        display.PrintStringValue("state", "Discharging");
        break;
      case power_manager::PowerSupplyProperties_BatteryState_NOT_PRESENT:
        display.PrintStringValue("state", "Not present");
        break;
      default:
        display.PrintStringValue("state", "Unknown");
    }

    display.PrintValue("voltage (V)", status.battery_voltage);
    display.PrintValue("energy (Wh)", status.battery_energy);
    display.PrintValue("energy rate (W)", status.battery_energy_rate);
    display.PrintValue("current (A)", status.battery_current);
    display.PrintValue("charge (Ah)", status.battery_charge);
    display.PrintValue("full charge (Ah)", status.battery_charge_full);
    display.PrintValue("percentage", status.battery_percentage);
    display.PrintValue("display percentage",
        status.display_battery_percentage);
    display.PrintStringValue("technology", status.battery_technology);

    // Don't print the battery time estimates -- they're wildly inaccurate since
    // this program only takes a single reading of the current.
  }
  return 0;
}
