// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/system/wilco_charge_controller_helper.h"

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/logging.h>
#include <base/strings/stringprintf.h>

namespace power_manager {
namespace system {

namespace {

constexpr const char kEcDriverSysfsDirectory[] =
    "/sys/bus/platform/devices/GOOG000C:00/";

// Relative path to |kEcDriverSysfsDirectory|.
constexpr const char kChargeScheduleDirectory[] = "wilco-charge-schedule/";

// kPeakShift* are relative paths to |kChargeScheduleDirectory|.
constexpr const char kPeakShiftEnablePath[] = "peak_shift_enable";
constexpr const char kPeakShiftThresholdPath[] = "peak_shift_battery_threshold";
constexpr const char kPeakShiftSchedulePath[] = "peak_shift_";

// kAdvancedCharging* are relative paths to |kChargeScheduleDirectory|.
constexpr const char kAdvancedChargingEnablePath[] = "advanced_charging_enable";
constexpr const char kAdvancedChargingSchedulePath[] = "advanced_charging_";

// Relative path to |kEcDriverSysfsDirectory|.
constexpr const char kBootOnAcEnablePath[] = "boot_on_ac";

// Relative path to |kEcDriverSysfsDirectory|.
constexpr const char kUsbPowerShareEnablePath[] = "usb_power_share";

constexpr const char kPowerSupplyDirectory[] =
    "/sys/class/power_supply/wilco-charger/";

// Relative path to |kPowerSupplyDirectory|.
constexpr const char kBatteryChargeModePath[] = "charge_type";

// Relative path to |kPowerSupplyDirectory|.
constexpr const char kBatteryChargeCustomChargeStartPath[] =
    "charge_control_start_threshold";

// Relative path to |kPowerSupplyDirectory|.
constexpr const char kBatteryChargeCustomChargeStopPath[] =
    "charge_control_end_threshold";

// Strings returned by this function are dictated by the kernel driver and
// can't be changed.
bool WeekDayToString(PowerManagementPolicy::WeekDay week_day,
                     std::string* week_day_out) {
  DCHECK(week_day_out);
  switch (week_day) {
    case PowerManagementPolicy::MONDAY:
      *week_day_out = "monday";
      return true;
    case PowerManagementPolicy::TUESDAY:
      *week_day_out = "tuesday";
      return true;
    case PowerManagementPolicy::WEDNESDAY:
      *week_day_out = "wednesday";
      return true;
    case PowerManagementPolicy::THURSDAY:
      *week_day_out = "thursday";
      return true;
    case PowerManagementPolicy::FRIDAY:
      *week_day_out = "friday";
      return true;
    case PowerManagementPolicy::SATURDAY:
      *week_day_out = "saturday";
      return true;
    case PowerManagementPolicy::SUNDAY:
      *week_day_out = "sunday";
      return true;
  }
  LOG(WARNING) << "Unexpected week day value " << static_cast<int>(week_day);
  return false;
}

bool WriteDataToFile(const base::FilePath& filename, const std::string& data) {
  if (base::WriteFile(filename, data.c_str(), data.length()) != data.length()) {
    PLOG(ERROR) << "Unable to write \"" << data << "\" to " << filename.value();
    return false;
  }
  return true;
}

}  // namespace

WilcoChargeControllerHelper::WilcoChargeControllerHelper() = default;

WilcoChargeControllerHelper::~WilcoChargeControllerHelper() = default;

bool WilcoChargeControllerHelper::SetPeakShiftEnabled(bool enable) {
  return WriteDataToFile(base::FilePath(kEcDriverSysfsDirectory)
                             .Append(kChargeScheduleDirectory)
                             .Append(kPeakShiftEnablePath),
                         enable ? "1" : "0");
}

bool WilcoChargeControllerHelper::SetPeakShiftBatteryPercentThreshold(
    int threshold) {
  return WriteDataToFile(base::FilePath(kEcDriverSysfsDirectory)
                             .Append(kChargeScheduleDirectory)
                             .Append(kPeakShiftThresholdPath),
                         base::StringPrintf("%d", threshold));
}

bool WilcoChargeControllerHelper::SetPeakShiftDayConfig(
    PowerManagementPolicy::WeekDay week_day, const std::string& config) {
  std::string week_day_str;
  return WeekDayToString(week_day, &week_day_str) &&
         WriteDataToFile(
             base::FilePath(kEcDriverSysfsDirectory)
                 .Append(kChargeScheduleDirectory)
                 .Append(std::string(kPeakShiftSchedulePath) + week_day_str),
             config);
}

bool WilcoChargeControllerHelper::SetBootOnAcEnabled(bool enable) {
  return WriteDataToFile(
      base::FilePath(kEcDriverSysfsDirectory).Append(kBootOnAcEnablePath),
      enable ? "1" : "0");
}

bool WilcoChargeControllerHelper::SetUsbPowerShareEnabled(bool enable) {
  return WriteDataToFile(
      base::FilePath(kEcDriverSysfsDirectory).Append(kUsbPowerShareEnablePath),
      enable ? "1" : "0");
}

bool WilcoChargeControllerHelper::SetAdvancedBatteryChargeModeEnabled(
    bool enable) {
  return WriteDataToFile(base::FilePath(kEcDriverSysfsDirectory)
                             .Append(kChargeScheduleDirectory)
                             .Append(kAdvancedChargingEnablePath),
                         enable ? "1" : "0");
  return false;
}

bool WilcoChargeControllerHelper::SetAdvancedBatteryChargeModeDayConfig(
    PowerManagementPolicy::WeekDay week_day, const std::string& config) {
  std::string week_day_str;
  return WeekDayToString(week_day, &week_day_str) &&
         WriteDataToFile(
             base::FilePath(kEcDriverSysfsDirectory)
                 .Append(kChargeScheduleDirectory)
                 .Append(std::string(kAdvancedChargingSchedulePath) +
                         week_day_str),
             config);
}

bool WilcoChargeControllerHelper::SetBatteryChargeMode(
    PowerManagementPolicy::BatteryChargeMode::Mode mode) {
  std::string charge_type;
  switch (mode) {
    case PowerManagementPolicy::BatteryChargeMode::STANDARD:
      charge_type = "Standard";
      break;
    case PowerManagementPolicy::BatteryChargeMode::EXPRESS_CHARGE:
      charge_type = "Fast";
      break;
    case PowerManagementPolicy::BatteryChargeMode::PRIMARILY_AC_USE:
      charge_type = "Trickle";
      break;
    case PowerManagementPolicy::BatteryChargeMode::ADAPTIVE:
      charge_type = "Adaptive";
      break;
    case PowerManagementPolicy::BatteryChargeMode::CUSTOM:
      charge_type = "Custom";
      break;
  }
  if (charge_type.empty()) {
    LOG(WARNING) << "Invalid battery charge mode " << mode;
    return false;
  }
  return WriteDataToFile(
      base::FilePath(kPowerSupplyDirectory).Append(kBatteryChargeModePath),
      charge_type);
}

bool WilcoChargeControllerHelper::SetBatteryChargeCustomThresholds(
    int custom_charge_start, int custom_charge_stop) {
  return WriteDataToFile(base::FilePath(kPowerSupplyDirectory)
                             .Append(kBatteryChargeCustomChargeStartPath),
                         base::StringPrintf("%d", custom_charge_start)) &&
         WriteDataToFile(base::FilePath(kPowerSupplyDirectory)
                             .Append(kBatteryChargeCustomChargeStopPath),
                         base::StringPrintf("%d", custom_charge_stop));
}

}  // namespace system
}  // namespace power_manager
