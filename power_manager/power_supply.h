// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWER_SUPPLY_H_
#define POWER_MANAGER_POWER_SUPPLY_H_

#include <string>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "power_manager/power_prefs.h"

namespace chromeos {
struct PowerInformation;
struct PowerStatus;
} // namespace chromeos

namespace power_manager {

// Used to read power supply status from sysfs, e.g. whether on AC or battery,
// charge and voltage level, current, etc.
class PowerSupply {
 public:
  PowerSupply(const FilePath& power_supply_path);
  ~PowerSupply();

  void Init();

  // Read data from power supply sysfs and fill out all fields of the
  // PowerStatus structure if possible.
  bool GetPowerStatus(chromeos::PowerStatus* status);

  // Read data from power supply sysfs for PowerInformation structure.
  bool GetPowerInformation(chromeos::PowerInformation* info);

  const FilePath& line_power_path() const { return line_power_path_; }
  const FilePath& battery_path() const { return battery_path_; }

 private:
  // Find sysfs directories to read from.
  void GetPowerSupplyPaths();

  // Use the PowerPrefs functions to read info from sysfs.
  class PowerInfoReader : public PowerPrefs {
   public:
    PowerInfoReader(const FilePath& path) : PowerPrefs(path, path) {}

    double ReadScaledDouble(const char* name);
    bool ReadString(const char* name, std::string* str);
  };
  PowerInfoReader* line_power_info_;
  PowerInfoReader* battery_info_;

  // Paths to power supply base sysfs directory and battery and line power
  // subdirectories.
  FilePath power_supply_path_;
  FilePath line_power_path_;
  FilePath battery_path_;

  // Various fields that are read from power supply sysfs.
  double charge_full_;
  double charge_full_design_;
  double charge_now_;
  double current_now_;
  double cycle_count_;
  double voltage_now_;

  // Indicates whether battery is present.
  bool present_;
  // Indicates whether line power is on.
  bool online_;

  std::string serial_number_;
  std::string status_;
  std::string technology_;
  std::string type_;

  DISALLOW_COPY_AND_ASSIGN(PowerSupply);
};

}  // namespace power_manager

#endif  // POWER_MANAGER_POWER_SUPPLY_H_
