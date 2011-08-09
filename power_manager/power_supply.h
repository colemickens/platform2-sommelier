// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWER_SUPPLY_H_
#define POWER_MANAGER_POWER_SUPPLY_H_

#include <gtest/gtest_prod.h>  // for FRIEND_TEST

#include <string>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/time.h"
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
  friend class PowerSupplyTest;
  FRIEND_TEST(PowerSupplyTest, TestDischargingWithHysteresis);

  // Find sysfs directories to read from.
  void GetPowerSupplyPaths();

  // Computes time remaining based on energy drain rate.
  double GetLinearTimeToEmpty();

  // Determine remaining time when charging or discharging.
  void CalculateRemainingTime(chromeos::PowerStatus *status);

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
  double nominal_voltage_;

  // Indicates whether battery is present.
  bool battery_is_present_;
  // Indicates whether line power is on.
  bool line_power_on_;

  std::string serial_number_;
  std::string technology_;
  std::string type_;

  // These are used for using hysteresis to avoid large swings in calculated
  // remaining battery time.
  double acceptable_variance_;
  base::TimeDelta hysteresis_time_;
  bool found_acceptable_time_range_;
  double acceptable_time_;
  base::TimeDelta time_outside_of_acceptable_range_;
  base::Time last_acceptable_range_time_;
  base::Time last_poll_time_;
  base::Time discharge_start_time_;
  // Use a function pointer to get the current time.  This way base::Time::Now()
  // can be mocked out by inserting an alternate function.
  base::Time (*time_now_func)();

  DISALLOW_COPY_AND_ASSIGN(PowerSupply);
};

}  // namespace power_manager

#endif  // POWER_MANAGER_POWER_SUPPLY_H_
