// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/power_supply.h"

#include <cmath>

#include "base/file_util.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "cros/chromeos_power.h"

namespace {

// For passing string pointers when no data is available, and we don't want to
// pass a NULL pointer.
const char kUnknownString[] = "Unknown";

// sysfs reports only integer values.  For non-integral values, it scales them
// up by 10^6.  This factor scales them back down accordingly.
const double kDoubleScaleFactor = 0.000001;

// Converts time from hours to seconds, and rounds to nearest integer.
int64 HoursToSeconds(double num_hours) {
  return lround(num_hours * 3600);
}

} // namespace

namespace power_manager {

PowerSupply::PowerSupply(const FilePath& power_supply_path)
    : line_power_info_(NULL),
      battery_info_(NULL),
      power_supply_path_(power_supply_path) {}

PowerSupply::~PowerSupply() {
  // Clean up allocated objects.
  if (line_power_info_)
    delete line_power_info_;
  if (battery_info_)
    delete battery_info_;
}

void PowerSupply::Init() {
  GetPowerSupplyPaths();
}

bool PowerSupply::GetPowerStatus(chromeos::PowerStatus* status) {
  CHECK(status);
  // Look for battery path if none has been found yet.
  if (battery_info_ || line_power_info_)
    GetPowerSupplyPaths();
  // The line power path should have been found during initialization, so there
  // is no need to look for it again.  However, check just to make sure the path
  // is still valid.  Better safe than sorry.
  if (!line_power_info_ || !file_util::PathExists(line_power_path_))
    return false;

  int64 value;
  line_power_info_->GetInt64("online", &value);
  online_ = (bool) value;

  // Return the line power status.
  status->line_power_on = online_;

  // If no battery was found, or if the previously found path doesn't exist
  // anymore, return true.  This is still an acceptable case since the battery
  // could be physically removed.
  status->battery_is_present = present_ = false;
  if (!battery_info_ || !file_util::PathExists(battery_path_))
    return true;

  battery_info_->GetInt64("present", &value);
  present_ = (bool) value;
  status->battery_is_present = present_;
  // If there is no battery present, we can skip the rest of the readings.
  if (!present_)
    return true;

  // Read all battery values from sysfs.
  charge_full_ = battery_info_->ReadScaledDouble("charge_full");
  charge_full_design_ = battery_info_->ReadScaledDouble("charge_full_design");
  charge_now_ = battery_info_->ReadScaledDouble("charge_now");
  // Sometimes current could be negative.  Ignore it and use |line_power_on| to
  // determine whether it's charging or discharging.
  current_now_ = fabs(battery_info_->ReadScaledDouble("current_now"));
  cycle_count_ = battery_info_->ReadScaledDouble("cycle_count");
  voltage_now_ = battery_info_->ReadScaledDouble("voltage_now");

  serial_number_.clear();
  status_.clear();
  technology_.clear();
  type_.clear();
  battery_info_->ReadString("serial_number", &serial_number_);
  battery_info_->ReadString("status", &status_);
  battery_info_->ReadString("technology", &technology_);
  battery_info_->ReadString("type", &type_);

  // Perform calculations / interpretations of the data read from sysfs.
  status->battery_energy = charge_now_ * voltage_now_;
  status->battery_energy_rate = current_now_ * voltage_now_;
  status->battery_voltage = voltage_now_;
  // Check to make sure there isn't a division by zero.
  if (current_now_ > 0) {
    status->battery_time_to_empty = HoursToSeconds(charge_now_ / current_now_);
    status->battery_time_to_full = HoursToSeconds((charge_full_ - charge_now_) /
                                                      current_now_);
  } else {
    status->battery_time_to_empty = 0;
    status->battery_time_to_full = 0;
  }
  if (charge_full_ > 0)
    status->battery_percentage = 100. * charge_now_ / charge_full_;
  else
    status->battery_percentage = 0;

  // TODO(sque): make this a STL map container?
  if (status_ == "Charging")
    status->battery_state = chromeos::BATTERY_STATE_CHARGING;
  else if (status_ == "Discharging")
    status->battery_state = chromeos::BATTERY_STATE_DISCHARGING;
  else if (status_ == "Empty")
    status->battery_state = chromeos::BATTERY_STATE_EMPTY;
  else if (status_ == "Full")
    status->battery_state = chromeos::BATTERY_STATE_FULLY_CHARGED;
  else
    status->battery_state = chromeos::BATTERY_STATE_UNKNOWN;
  return true;
}

void PowerSupply::GetPowerSupplyPaths() {
  // First check if both line power and batter paths have been found and still
  // exist.  If so, there is no need to do anything else.
  if (battery_info_ && file_util::PathExists(battery_path_) &&
      line_power_info_ && file_util::PathExists(line_power_path_))
    return;
  // Use a FileEnumerator to browse through all files/subdirectories in the
  // power supply sysfs directory.
  file_util::FileEnumerator file_enum(power_supply_path_, false,
      file_util::FileEnumerator::DIRECTORIES);
  // Read type info from all power sources, and try to identify battery and line
  // power sources.  Their paths are to be stored locally.
  for (FilePath path = file_enum.Next();
       !path.empty();
       path = file_enum.Next()) {
    std::string buf;
    if (file_util::ReadFileToString(path.Append("type"), &buf)) {
      TrimWhitespaceASCII(buf, TRIM_TRAILING, &buf);
      // Only look for battery / line power paths if they haven't been found
      // already.  This makes the assumption that they don't change (but battery
      // path can disappear if removed).  So this code should only be run once
      // for each power source.
      if (buf == "Battery" && !battery_info_) {
        LOG(INFO) << "Battery path found: " << path.value();
        battery_path_ = path;
        battery_info_ = new PowerInfoReader(path);
      } else if (buf == "Mains" && !line_power_info_) {
        LOG(INFO) << "Line power path found: " << path.value();
        line_power_path_ = path;
        line_power_info_ = new PowerInfoReader(path);
      }
    }
  }
}

double PowerSupply::PowerInfoReader::ReadScaledDouble(const char* name) {
  int64 value;
  if (GetInt64(name, &value))
    return kDoubleScaleFactor * value;
  return -1.;
}

bool PowerSupply::PowerInfoReader::ReadString(const char* name,
                                              std::string* str) {
  bool result = file_util::ReadFileToString(pref_path().Append(name), str);
  TrimWhitespaceASCII(*str, TRIM_TRAILING, str);
  return result;
}

}  // namespace power_manager
