// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/power_supply.h"

#include <cmath>

#include "base/file_util.h"
#include "base/logging.h"
#include "base/string_util.h"

namespace {

// For passing string pointers when no data is available, and we don't want to
// pass a NULL pointer.
const char kUnknownString[] = "Unknown";

// sysfs reports only integer values.  For non-integral values, it scales them
// up by 10^6.  This factor scales them back down accordingly.
const double kDoubleScaleFactor = 0.000001;

// How much the remaining time can vary, as a fraction of the baseline time.
const double kAcceptableVariance = 0.02;

// Initially, allow 10 seconds before deciding on an acceptable time.
const base::TimeDelta kHysteresisTimeFast = base::TimeDelta::FromSeconds(10);

// Allow three minutes before deciding on a new acceptable time.
const base::TimeDelta kHysteresisTime = base::TimeDelta::FromMinutes(3);

// Converts time from hours to seconds.
double HoursToSecondsDouble(double num_hours) {
  return num_hours * 3600.;
}
// Same as above, but rounds to nearest integer.
int64 HoursToSecondsInt(double num_hours) {
  return lround(HoursToSecondsDouble(num_hours));
}

} // namespace

namespace power_manager {

PowerSupply::PowerSupply(const FilePath& power_supply_path)
    : line_power_info_(NULL),
      battery_info_(NULL),
      power_supply_path_(power_supply_path),
      acceptable_variance_(kAcceptableVariance),
      hysteresis_time_(kHysteresisTimeFast),
      found_acceptable_time_range_(false),
      time_now_func(base::Time::Now),
      is_suspended_(false) {}

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

bool PowerSupply::GetPowerStatus(PowerStatus* status) {
  CHECK(status);
  // Look for battery path if none has been found yet.
  if (!battery_info_ || !line_power_info_)
    GetPowerSupplyPaths();
  // The line power path should have been found during initialization, so there
  // is no need to look for it again.  However, check just to make sure the path
  // is still valid.  Better safe than sorry.
  if ((!line_power_info_ || !file_util::PathExists(line_power_path_)) &&
      (!battery_info_ || !file_util::PathExists(battery_path_))) {
#ifndef IS_DESKTOP
    // A hack for situations like VMs where there is no power supply sysfs.
    LOG(INFO) << "No power supply sysfs path found, assuming line power on.";
#endif
    status->line_power_on = line_power_on_ = true;
    status->battery_is_present = battery_is_present_ = false;
    return true;
  }
  int64 value;
  if (line_power_info_ && file_util::PathExists(line_power_path_)) {
    line_power_info_->GetInt64("online", &value);
    // Return the line power status.
    status->line_power_on = line_power_on_ = (bool) value;
  }

  // If no battery was found, or if the previously found path doesn't exist
  // anymore, return true.  This is still an acceptable case since the battery
  // could be physically removed.
  if (!battery_info_ || !file_util::PathExists(battery_path_)) {
    status->battery_is_present = battery_is_present_ = false;
    return true;
  }

  battery_info_->GetInt64("present", &value);
  status->battery_is_present = battery_is_present_ = (bool) value;
  // If there is no battery present, we can skip the rest of the readings.
  if (!battery_is_present_)
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
  // Attempt to determine nominal voltage for time remaining calculations.
  if (file_util::PathExists(battery_path_.Append("voltage_min_design")))
    nominal_voltage_ = battery_info_->ReadScaledDouble("voltage_min_design");
  else if (file_util::PathExists(battery_path_.Append("voltage_max_design")))
    nominal_voltage_ = battery_info_->ReadScaledDouble("voltage_max_design");
  else
    nominal_voltage_ = voltage_now_;

  serial_number_.clear();
  technology_.clear();
  type_.clear();
  battery_info_->ReadString("serial_number", &serial_number_);
  battery_info_->ReadString("technology", &technology_);
  battery_info_->ReadString("type", &type_);

  // Perform calculations / interpretations of the data read from sysfs.
  status->battery_energy = charge_now_ * voltage_now_;
  status->battery_energy_rate = current_now_ * voltage_now_;
  status->battery_voltage = voltage_now_;

  CalculateRemainingTime(status);

  if (charge_full_ > 0 && charge_full_design_ > 0)
    status->battery_percentage = 100. * charge_now_ / charge_full_;
  else
    status->battery_percentage = -1;

  // Determine battery state from above readings.  Disregard the "status" field
  // in sysfs, as that can be inconsistent with the numerical readings.
  status->battery_state = BATTERY_STATE_UNKNOWN;
  if (line_power_on_) {
    if (charge_now_ >= charge_full_) {
      status->battery_state = BATTERY_STATE_FULLY_CHARGED;
    } else {
      if (current_now_ <= 0)
        LOG(WARNING) << "Line power is on and battery is not fully charged "
                     << "but battery current is " << current_now_ << " A.";
      status->battery_state = BATTERY_STATE_CHARGING;
    }
  } else {
    status->battery_state = BATTERY_STATE_DISCHARGING;
    if (charge_now_ == 0)
      status->battery_state = BATTERY_STATE_EMPTY;
  }
  return true;
}

bool PowerSupply::GetPowerInformation(PowerInformation* info) {
  CHECK(info);
  GetPowerStatus(&info->power_status);
  if (!info->power_status.battery_is_present)
    return true;

  battery_info_->ReadString("vendor", &info->battery_vendor);
  battery_info_->ReadString("model_name", &info->battery_model);
  battery_info_->ReadString("serial_number", &info->battery_serial);
  battery_info_->ReadString("technology", &info->battery_technology);

  switch (info->power_status.battery_state) {
    case BATTERY_STATE_CHARGING:
      info->battery_state_string = "Charging";
      break;
    case BATTERY_STATE_DISCHARGING:
      info->battery_state_string = "Discharging";
      break;
    case BATTERY_STATE_EMPTY:
      info->battery_state_string = "Empty";
      break;
    case BATTERY_STATE_FULLY_CHARGED:
      info->battery_state_string = "Fully charged";
      break;
    default:
      info->battery_state_string = "Unknown";
      break;
  }
  return true;
}

void PowerSupply::SetSuspendState(bool state) {
  // Do not take any action if there is no change in suspend state.
  if (is_suspended_ == state)
    return;
  is_suspended_ = state;

  // Record the suspend time.
  if (is_suspended_) {
    suspend_time_ = time_now_func();
    return;
  }

  // If resuming, deduct the time suspended from the hysteresis state machine
  // timestamps.
  base::TimeDelta offset = time_now_func() - suspend_time_;
  AdjustHysteresisTimes(offset);
}

void PowerSupply::GetPowerSupplyPaths() {
  // First check if both line power and battery paths have been found and still
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
        DLOG(INFO) << "Battery path found: " << path.value();
        battery_path_ = path;
        battery_info_ = new PowerInfoReader(path);
      } else if (buf == "Mains" && !line_power_info_) {
        DLOG(INFO) << "Line power path found: " << path.value();
        line_power_path_ = path;
        line_power_info_ = new PowerInfoReader(path);
      }
    }
  }
}

double PowerSupply::GetLinearTimeToEmpty() {
  return HoursToSecondsDouble(nominal_voltage_ * charge_now_ / (current_now_ *
                                                                voltage_now_));
}

void PowerSupply::CalculateRemainingTime(PowerStatus *status) {
  CHECK(time_now_func);
  base::Time time_now = time_now_func();
  // This function might be called due to a race condition between the suspend
  // process and the battery polling.  If that's the case, handle it gracefully
  // by updating the hysteresis times and suspend time.
  //
  // Since the time between suspend and now has been taken into account in the
  // hysteresis times, the recorded suspend time should be updated to the
  // current time, to compensate.
  //
  // Example:
  // Hysteresis time = 3
  // At time t=0, there is a read of the power supply.
  // At time t=1, the system is suspended.
  // At time t=4, the system is resumed.  There is a power supply read at t=4.
  // At time t=4.5, SetSuspendState(false) is called (latency in resume process)
  //
  // At t=4, the remaining time could be set to something very high, based on
  // the low suspend current, since the time since last read is greater than the
  // hysteresis time.
  //
  // The solution is to shift the last read time forward by 3, which is the time
  // elapsed between suspend (t=1) and the next reading (t=4).  Thus, the time
  // of last read becomes t=3, and time since last read becomes 1 instead of 4.
  // This avoids triggering the time hysteresis adjustment.
  //
  // At this point, the suspend time is also reset to the current time.  This is
  // so that when AdjustHysteresisTimes() is called again (e.g. during resume),
  // the previous period of t=1 to t=4 is not used again in the adjustment.
  // Continuing the example:
  // At t=4.5, SetSuspendState(false) is called, and it calls
  //   AdjustHysteresisTimes().  Since suspend time has been adjusted from t=1
  //   to t=4, the new offset is only 0.5.  So time of last read gets shifted
  //   from t=3 to t=3.5.
  // If suspend time was not reset to t=4, then we'd have an offset of 3.5
  // instead of 0.5, and time of last read gets set from t=3 to t=6.5, which is
  // invalid.
  if (is_suspended_) {
    AdjustHysteresisTimes(time_now - suspend_time_);
    suspend_time_ = time_now;
  }

  // Check to make sure there isn't a division by zero.
  if (current_now_ > 0) {
    double time_to_empty = 0;
    if (status->line_power_on) {
      status->battery_time_to_full =
          HoursToSecondsInt((charge_full_ - charge_now_) / current_now_);
      // Reset the remaining-time-calculation state machine when AC plugged in.
      found_acceptable_time_range_ = false;
      last_poll_time_ = base::Time();
      discharge_start_time_ = base::Time();
      last_acceptable_range_time_ = base::Time();
      // Make sure that when the system switches to battery power, the initial
      // hysteresis time will be very short, so it can find an acceptable
      // battery remaining time as quickly as possible.
      hysteresis_time_ = kHysteresisTimeFast;
    } else if (!found_acceptable_time_range_) {
      // No base range found, need to give it some time to stabilize.  For now,
      // use the simple linear calculation for time.
      if (discharge_start_time_.is_null())
        discharge_start_time_ = time_now;
      time_to_empty = GetLinearTimeToEmpty();
      status->battery_time_to_empty = lround(time_to_empty);
      // Select an acceptable remaining time once the system has been
      // discharging for the necessary amount of time.
      if (time_now - discharge_start_time_ >= hysteresis_time_) {
        acceptable_time_ = time_to_empty;
        found_acceptable_time_range_ = true;
        last_poll_time_ = last_acceptable_range_time_ = time_now;
        // Since an acceptable time has been found, start using the normal
        // hysteresis time going forward.
        hysteresis_time_ = kHysteresisTime;
      }
    } else {
      double calculated_time = GetLinearTimeToEmpty();
      double allowed_time_variation = acceptable_time_ * acceptable_variance_;
      // Reduce the acceptable time range as time goes by.
      acceptable_time_ -= (time_now - last_poll_time_).InSecondsF();
      if (fabs(calculated_time - acceptable_time_) <= allowed_time_variation) {
        last_acceptable_range_time_ = time_now;
        time_to_empty = calculated_time;
      } else if (time_now - last_acceptable_range_time_ >= hysteresis_time_) {
        // If the calculated time has been outside the acceptable range for a
        // long enough period of time, make it the basis for a new acceptable
        // range.
        acceptable_time_ = calculated_time;
        time_to_empty = calculated_time;
        found_acceptable_time_range_ = true;
        last_acceptable_range_time_ = time_now;
      } else if (calculated_time < acceptable_time_ - allowed_time_variation) {
        // Clip remaining time at lower bound if it is too low.
        time_to_empty = acceptable_time_ - allowed_time_variation;
      } else {
        // Clip remaining time at upper bound if it is too high.
        time_to_empty = acceptable_time_ + allowed_time_variation;
      }
      last_poll_time_ = time_now;
    }
    status->battery_time_to_empty = lround(time_to_empty);
  } else {
    status->battery_time_to_empty = 0;
    status->battery_time_to_full = 0;
  }
}

void PowerSupply::AdjustHysteresisTimes(const base::TimeDelta& offset) {
  if (!discharge_start_time_.is_null())
    discharge_start_time_ += offset;
  if (!last_acceptable_range_time_.is_null())
    last_acceptable_range_time_ += offset;
  if (!last_poll_time_.is_null())
    last_poll_time_ += offset;
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
