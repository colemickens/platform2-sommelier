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
constexpr const char kPeakShiftPropertyDirectory[] = "properties/peakshift/";

// Next kPeakShift* are relative paths to |kPeakShiftPropertyDirectory|.
constexpr const char kPeakShiftEnablePath[] = "enable";
constexpr const char kPeakShiftThresholdPath[] = "peakshift_battery_threshold";

constexpr const char kPeakShiftMondayPath[] = "peakshift_monday";
constexpr const char kPeakShiftTuesdayPath[] = "peakshift_tuesday";
constexpr const char kPeakShiftWednesdayPath[] = "peakshift_wednesday";
constexpr const char kPeakShiftThursdayPath[] = "peakshift_thursday";
constexpr const char kPeakShiftFridayPath[] = "peakshift_friday";
constexpr const char kPeakShiftSaturdayPath[] = "peakshift_saturday";
constexpr const char kPeakShiftSundayPath[] = "peakshift_sunday";

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
                             .Append(kPeakShiftPropertyDirectory)
                             .Append(kPeakShiftEnablePath),
                         enable ? "1" : "0");
}

bool WilcoChargeControllerHelper::SetPeakShiftBatteryPercentThreshold(
    int threshold) {
  return WriteDataToFile(base::FilePath(kEcDriverSysfsDirectory)
                             .Append(kPeakShiftPropertyDirectory)
                             .Append(kPeakShiftThresholdPath),
                         base::StringPrintf("%03d", threshold));
}

bool WilcoChargeControllerHelper::SetPeakShiftDayConfig(
    PowerManagementPolicy::WeekDay week_day, const std::string& config) {
  const char* day_file = nullptr;
  switch (week_day) {
    case PowerManagementPolicy::MONDAY:
      day_file = kPeakShiftMondayPath;
      break;
    case PowerManagementPolicy::TUESDAY:
      day_file = kPeakShiftTuesdayPath;
      break;
    case PowerManagementPolicy::WEDNESDAY:
      day_file = kPeakShiftWednesdayPath;
      break;
    case PowerManagementPolicy::THURSDAY:
      day_file = kPeakShiftThursdayPath;
      break;
    case PowerManagementPolicy::FRIDAY:
      day_file = kPeakShiftFridayPath;
      break;
    case PowerManagementPolicy::SATURDAY:
      day_file = kPeakShiftSaturdayPath;
      break;
    case PowerManagementPolicy::SUNDAY:
      day_file = kPeakShiftSundayPath;
      break;
  }
  if (!day_file) {
    PLOG(WARNING) << "Unexpected week day value " << static_cast<int>(week_day);
    return false;
  }
  return WriteDataToFile(base::FilePath(kEcDriverSysfsDirectory)
                             .Append(kPeakShiftPropertyDirectory)
                             .Append(day_file),
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
  NOTIMPLEMENTED();
  return false;
}

bool WilcoChargeControllerHelper::SetAdvancedBatteryChargeModeDayConfig(
    PowerManagementPolicy::WeekDay week_day, const std::string& config) {
  NOTIMPLEMENTED();
  return false;
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
