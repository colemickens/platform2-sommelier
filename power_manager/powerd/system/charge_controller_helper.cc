// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/system/charge_controller_helper.h"

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/logging.h>
#include <base/strings/stringprintf.h>

namespace power_manager {
namespace system {

namespace {

constexpr const char kPeakShiftPropertyPath[] =
    "/sys/bus/platform/devices/GOOG000C:00/properties/peakshift";

constexpr const char kPeakShiftEnablePath[] = "enable";
constexpr const char kPeakShiftThresholdPath[] = "peakshift_battery_threshold";

constexpr const char kPeakShiftMondayPath[] = "peakshift_monday";
constexpr const char kPeakShiftTuesdayPath[] = "peakshift_tuesday";
constexpr const char kPeakShiftWednesdayPath[] = "peakshift_wednesday";
constexpr const char kPeakShiftThursdayPath[] = "peakshift_thursday";
constexpr const char kPeakShiftFridayPath[] = "peakshift_friday";
constexpr const char kPeakShiftSaturdayPath[] = "peakshift_saturday";
constexpr const char kPeakShiftSundayPath[] = "peakshift_sunday";

constexpr const char kBootOnAcEnablePath[] =
    "/sys/bus/platform/devices/GOOG00C:00/boot_on_ac";

bool WriteDataToFile(const base::FilePath& filename, const std::string& data) {
  if (base::WriteFile(filename, data.c_str(), data.length()) != data.length()) {
    PLOG(ERROR) << "Unable to write \"" << data << "\" to " << filename.value();
    return false;
  }
  return true;
}

}  // namespace

ChargeControllerHelper::ChargeControllerHelper() = default;

ChargeControllerHelper::~ChargeControllerHelper() = default;

bool ChargeControllerHelper::SetPeakShiftEnabled(bool enable) {
  return WriteDataToFile(
      base::FilePath(kPeakShiftPropertyPath).Append(kPeakShiftEnablePath),
      enable ? "1" : "0");
}

bool ChargeControllerHelper::SetPeakShiftBatteryPercentThreshold(
    int threshold) {
  return WriteDataToFile(
      base::FilePath(kPeakShiftPropertyPath).Append(kPeakShiftThresholdPath),
      base::StringPrintf("%03d", threshold));
}

bool ChargeControllerHelper::SetPeakShiftDayConfig(WeekDay week_day,
                                                   const std::string& config) {
  base::FilePath filename(kPeakShiftPropertyPath);
  const char* day_file = nullptr;
  switch (week_day) {
    case WeekDay::MONDAY:
      day_file = kPeakShiftMondayPath;
      break;
    case WeekDay::TUESDAY:
      day_file = kPeakShiftTuesdayPath;
      break;
    case WeekDay::WEDNESDAY:
      day_file = kPeakShiftWednesdayPath;
      break;
    case WeekDay::THURSDAY:
      day_file = kPeakShiftThursdayPath;
      break;
    case WeekDay::FRIDAY:
      day_file = kPeakShiftFridayPath;
      break;
    case WeekDay::SATURDAY:
      day_file = kPeakShiftSaturdayPath;
      break;
    case WeekDay::SUNDAY:
      day_file = kPeakShiftSundayPath;
      break;
  }
  if (!day_file) {
    PLOG(WARNING) << "Unexpected week day value " << static_cast<int>(week_day);
    return false;
  }
  return WriteDataToFile(
      base::FilePath(kPeakShiftPropertyPath).Append(day_file), config);
}

bool ChargeControllerHelper::SetBootOnAcEnabled(bool enable) {
  return WriteDataToFile(base::FilePath(kBootOnAcEnablePath),
                         enable ? "1" : "0");
}

}  // namespace system
}  // namespace power_manager
