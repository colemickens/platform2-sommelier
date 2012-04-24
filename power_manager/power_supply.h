// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
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

namespace power_manager {

enum BatteryState {
  BATTERY_STATE_UNKNOWN,
  BATTERY_STATE_CHARGING,
  BATTERY_STATE_DISCHARGING,
  BATTERY_STATE_EMPTY,
  BATTERY_STATE_FULLY_CHARGED
};

// Structures used for passing power supply info.
struct PowerStatus {
  bool line_power_on;

  // Amount of energy, measured in Wh, in the battery.
  double battery_energy;

  // Amount of energy being drained from the battery, measured in W. If
  // positive, the source is being discharged, if negative it's being charged.
  double battery_energy_rate;

  double battery_voltage;

  // Set to true when we have just transitioned states and we might have both a
  // segment of charging and discharging in the calculation. This is done to
  // signal that the time value maybe inaccurate.
  bool is_calculating_battery_time;

  // Time in seconds until the battery is considered empty, 0 for unknown.
  int64 battery_time_to_empty;
  // Time in seconds until the battery is considered full. 0 for unknown.
  int64 battery_time_to_full;

  // Averaged time in seconds until the battery is considered empty, 0 for
  // unknown.
  int64 averaged_battery_time_to_empty;
  // Average time in seconds until the battery is considered full. 0 for
  // unknown.
  int64 averaged_battery_time_to_full;

  double battery_percentage;
  bool battery_is_present;

  BatteryState battery_state;
};

struct PowerInformation {
  PowerStatus power_status;

  // Amount of energy, measured in Wh, in the battery when it's considered
  // empty.
  double battery_energy_empty;

  // Amount of energy, measured in Wh, in the battery when it's considered full.
  double battery_energy_full;

  // Amount of energy, measured in Wh, the battery is designed to hold when it's
  // considered full.
  double battery_energy_full_design;

  bool battery_is_rechargeable;  // [needed?]
  double battery_capacity;

  std::string battery_technology;  // [needed?]

  std::string battery_vendor;
  std::string battery_model;
  std::string battery_serial;

  std::string line_power_vendor;
  std::string line_power_model;
  std::string line_power_serial;

  std::string battery_state_string;
};

// Used to read power supply status from sysfs, e.g. whether on AC or battery,
// charge and voltage level, current, etc.
class PowerSupply {
 public:
  explicit PowerSupply(const FilePath& power_supply_path);
  ~PowerSupply();

  void Init();

  // Read data from power supply sysfs and fill out all fields of the
  // PowerStatus structure if possible.
  bool GetPowerStatus(PowerStatus* status, bool is_calculating);

  // Read data from power supply sysfs for PowerInformation structure.
  bool GetPowerInformation(PowerInformation* info);

  const FilePath& line_power_path() const { return line_power_path_; }
  const FilePath& battery_path() const { return battery_path_; }

  void SetSuspendState(bool state);

 private:
  friend class PowerSupplyTest;
  FRIEND_TEST(PowerSupplyTest, TestDischargingWithHysteresis);
  FRIEND_TEST(PowerSupplyTest, TestDischargingWithSuspendResume);

  // Use the PowerPrefs functions to read info from sysfs.
  // Make this public for unit testing purposes, but keep objects private.
  class PowerInfoReader : public PowerPrefs {
   public:
    explicit PowerInfoReader(const FilePath& path) : PowerPrefs(path, path) {}

    double ReadScaledDouble(const char* name);
    bool ReadString(const char* name, std::string* str);
  };

  // Find sysfs directories to read from.
  void GetPowerSupplyPaths();

  // Computes time remaining based on energy drain rate.
  double GetLinearTimeToEmpty();

  // Determine remaining time when charging or discharging.
  void CalculateRemainingTime(PowerStatus *status);

  // Offsets the timestamps used in hysteresis calculations.  This is used when
  // suspending and resuming -- the time while suspended should not count toward
  // the hysteresis times.
  void AdjustHysteresisTimes(const base::TimeDelta& offset);

  // Used for reading line power and battery status from sysfs.
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

  base::Time suspend_time_;
  bool is_suspended_;

  DISALLOW_COPY_AND_ASSIGN(PowerSupply);
};

}  // namespace power_manager

#endif  // POWER_MANAGER_POWER_SUPPLY_H_
