// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWERD_SYSTEM_CHARGE_CONTROLLER_HELPER_INTERFACE_H_
#define POWER_MANAGER_POWERD_SYSTEM_CHARGE_CONTROLLER_HELPER_INTERFACE_H_

#include <string>

namespace power_manager {
namespace system {

// Important note: It is not final version of interface,
// blocked by b:125011171.
//
// Interface for classes to perform the actions requested by
// |policy::ChargeController|.
//
// All methods return true on success and false on failure.
class ChargeControllerHelperInterface {
 public:
  enum class WeekDay {
    MONDAY = 0,
    TUESDAY,
    WEDNESDAY,
    THURSDAY,
    FRIDAY,
    SATURDAY,
    SUNDAY,
  };

  virtual ~ChargeControllerHelperInterface() = default;

  // Enables or disables peak shift.
  virtual bool SetPeakShiftEnabled(bool enable) = 0;

  // Sets the lower bound of the battery charge (as a percent in [0, 100])
  // for using peak shift.
  virtual bool SetPeakShiftBatteryPercentThreshold(int threshold) = 0;

  // Configures when peak shift will be enabled on |week_day|.
  // |config| contains space separated zero-leading hour and minute of
  // start time, end time and charge start time,
  // i.e. "00 30 09 45 20 00" means:
  //     - 00:30 is start time,
  //     - 09:45 is end time,
  //     - 20:00 is charge start time.
  virtual bool SetPeakShiftDayConfig(WeekDay week_day,
                                     const std::string& config) = 0;

  // Enables or disables boot on AC.
  virtual bool SetBootOnAcEnabled(bool enable) = 0;
};

}  // namespace system
}  // namespace power_manager

#endif  // POWER_MANAGER_POWERD_SYSTEM_CHARGE_CONTROLLER_HELPER_INTERFACE_H_
