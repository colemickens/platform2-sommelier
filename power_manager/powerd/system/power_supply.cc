// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/system/power_supply.h"

#include <cmath>

#include "base/file_util.h"
#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "power_manager/common/clock.h"
#include "power_manager/common/power_constants.h"
#include "power_manager/common/prefs.h"
#include "power_manager/common/util.h"

namespace power_manager {
namespace system {

namespace {

// sysfs reports only integer values.  For non-integral values, it scales them
// up by 10^6.  This factor scales them back down accordingly.
const double kDoubleScaleFactor = 0.000001;

// How much the remaining time can vary, as a fraction of the baseline time.
const double kAcceptableVariance = 0.02;

// Initially, allow 10 seconds before deciding on an acceptable time.
const base::TimeDelta kHysteresisTimeFast = base::TimeDelta::FromSeconds(10);

// Allow three minutes before deciding on a new acceptable time.
const base::TimeDelta kHysteresisTime = base::TimeDelta::FromMinutes(3);

// Report batteries as full if they're at or above this level (out of a max of
// 1.0).
const double kDefaultFullFactor = 0.97;

// Default time interval between polls, in milliseconds.
const int kDefaultPollMs = 30000;

// Default shorter time interval used after a failed poll or to wait before
// calculating the remaining battery time after suspend/resume or a udev
// event, in milliseconds.
const int kDefaultShortPollMs = 5000;

// Upper limit to accept for raw battery times, in seconds. If the time of
// interest is above this level assume something is wrong.
const int64 kBatteryTimeMaxValidSec = 24 * 60 * 60;

// Different power supply types reported by the kernel.
const char kBatteryType[] = "Battery";
const char kMainsType[] = "Mains";

// Battery states reported by the kernel. This is not the full set of
// possible states; see drivers/power/power_supply_sysfs.c.
const char kBatteryStatusCharging[] = "Charging";
const char kBatteryStatusFull[] = "Full";

// Converts time from hours to seconds.
inline double HoursToSecondsDouble(double num_hours) {
  return num_hours * 3600.;
}
// Same as above, but rounds to nearest integer.
inline int64 HoursToSecondsInt(double num_hours) {
  return lround(HoursToSecondsDouble(num_hours));
}

// Reads the contents of |filename| within |directory| into |out|, trimming
// trailing whitespace.  Returns true on success.
bool ReadAndTrimString(const base::FilePath& directory,
                       const std::string& filename,
                       std::string* out) {
  if (!file_util::ReadFileToString(directory.Append(filename), out))
    return false;

  TrimWhitespaceASCII(*out, TRIM_TRAILING, out);
  return true;
}

// Reads a 64-bit integer value from a file and returns true on success.
bool ReadInt64(const base::FilePath& directory,
               const std::string& filename,
               int64* out) {
  std::string buffer;
  if (!ReadAndTrimString(directory, filename, &buffer))
    return false;
  return base::StringToInt64(buffer, out);
}

// Reads an integer value and scales it to a double (see |kDoubleScaleFactor|.
// Returns -1.0 on failure.
double ReadScaledDouble(const base::FilePath& directory,
                        const std::string& filename) {
  int64 value = 0;
  return ReadInt64(directory, filename, &value) ?
      kDoubleScaleFactor * value : -1.;
}

// Computes time remaining based on energy drain rate.
double GetLinearTimeToEmpty(const PowerStatus& status) {
  return HoursToSecondsDouble(status.nominal_voltage * status.battery_charge /
      (status.battery_current * status.battery_voltage));
}

// Returns true if |type|, a power supply type read from a "type" file in
// sysfs, indicates USB.
bool IsUsbType(const std::string& type) {
  // These are defined in drivers/power/power_supply_sysfs.c in the kernel.
  return type == "USB" || type == "USB_DCP" || type == "USB_CDP" ||
      type == "USB_ACA";
}

}  // namespace

void PowerSupply::TestApi::SetCurrentTime(base::TimeTicks now) {
  power_supply_->clock_->set_current_time_for_testing(now);
}

bool PowerSupply::TestApi::TriggerPollTimeout() {
  if (!power_supply_->poll_timeout_id_)
    return false;

  guint old_timeout_id = power_supply_->poll_timeout_id_;
  CHECK(!power_supply_->HandlePollTimeout());
  CHECK(power_supply_->poll_timeout_id_ != old_timeout_id);
  util::RemoveTimeout(&old_timeout_id);
  return true;
}

const int PowerSupply::kCalculateBatterySlackMs = 50;

PowerSupply::PowerSupply(const base::FilePath& power_supply_path,
                         PrefsInterface* prefs)
    : prefs_(prefs),
      clock_(new Clock),
      power_supply_path_(power_supply_path),
      acceptable_variance_(kAcceptableVariance),
      hysteresis_time_(kHysteresisTimeFast),
      found_acceptable_time_range_(false),
      sample_window_max_(10),
      sample_window_min_(1),
      taper_time_max_(base::TimeDelta::FromSeconds(3600)),
      taper_time_min_(base::TimeDelta::FromSeconds(600)),
      low_battery_shutdown_time_(base::TimeDelta::FromSeconds(180)),
      low_battery_shutdown_percent_(0.0),
      is_suspended_(false),
      full_factor_(kDefaultFullFactor),
      poll_timeout_id_(0),
      notify_observers_timeout_id_(0) {
  DCHECK(prefs);
}

PowerSupply::~PowerSupply() {
  util::RemoveTimeout(&poll_timeout_id_);
  util::RemoveTimeout(&notify_observers_timeout_id_);
}

void PowerSupply::Init() {
  GetPowerSupplyPaths();

  int64 poll_ms = kDefaultPollMs;
  prefs_->GetInt64(kBatteryPollIntervalPref, &poll_ms);
  poll_delay_ = base::TimeDelta::FromMilliseconds(poll_ms);

  int64 short_poll_ms = kDefaultShortPollMs;
  prefs_->GetInt64(kBatteryPollShortIntervalPref, &short_poll_ms);
  short_poll_delay_ = base::TimeDelta::FromMilliseconds(short_poll_ms);

  prefs_->GetDouble(kPowerSupplyFullFactorPref, &full_factor_);
  full_factor_ = std::min(std::max(kEpsilon, full_factor_), 1.0);

  int64 shutdown_time_sec = 0;
  if (prefs_->GetInt64(kLowBatteryShutdownTimePref, &shutdown_time_sec)) {
    low_battery_shutdown_time_ =
        base::TimeDelta::FromSeconds(shutdown_time_sec);
  }

  prefs_->GetDouble(kLowBatteryShutdownPercentPref,
                    &low_battery_shutdown_percent_);

  // This log message is needed by power_LoadTest in autotest
  LOG(INFO) << "Using low battery time threshold of "
            << low_battery_shutdown_time_.InSeconds()
            << " secs and using low battery percent threshold of "
            << low_battery_shutdown_percent_;

  prefs_->GetInt64(kSampleWindowMaxPref, &sample_window_max_);
  prefs_->GetInt64(kSampleWindowMinPref, &sample_window_min_);
  CHECK(sample_window_min_ > 0);
  CHECK(sample_window_max_ >= sample_window_min_);

  int64 taper_time_max_sec = 0;
  if (prefs_->GetInt64(kTaperTimeMaxPref, &taper_time_max_sec))
    taper_time_max_ = base::TimeDelta::FromSeconds(taper_time_max_sec);
  int64 taper_time_min_sec = taper_time_min_.InSeconds();
  if (prefs_->GetInt64(kTaperTimeMinPref, &taper_time_min_sec))
    taper_time_min_ = base::TimeDelta::FromSeconds(taper_time_min_sec);
  CHECK(taper_time_min_ > base::TimeDelta());
  CHECK(taper_time_max_ > taper_time_min_);

  time_to_empty_average_.Init(sample_window_max_);
  time_to_full_average_.Init(sample_window_max_);

  SetCalculatingBatteryTime();
  SchedulePoll(false);
}

void PowerSupply::AddObserver(PowerSupplyObserver* observer) {
  DCHECK(observer);
  observers_.AddObserver(observer);
}

void PowerSupply::RemoveObserver(PowerSupplyObserver* observer) {
  DCHECK(observer);
  observers_.RemoveObserver(observer);
}

bool PowerSupply::RefreshImmediately() {
  bool success = UpdatePowerStatus();
  if (!is_suspended_)
    SchedulePoll(success);
  if (success && !notify_observers_timeout_id_) {
    notify_observers_timeout_id_ =
        g_timeout_add(0, HandleNotifyObserversTimeoutThunk, this);
  }
  return success;
}

bool PowerSupply::GetPowerInformation(PowerInformation* info) {
  CHECK(info);
  if (!UpdatePowerStatus())
    return false;

  info->power_status = power_status_;
  info->line_power_path = line_power_path_.value();
  info->battery_path = battery_path_.value();
  if (!info->power_status.battery_is_present)
    return true;

  info->battery_vendor.clear();
  info->battery_model.clear();
  info->battery_serial.clear();
  info->battery_technology.clear();

  // POWER_SUPPLY_PROP_VENDOR does not seem to be a valid property
  // defined in <linux/power_supply.h>.
  if (file_util::PathExists(battery_path_.Append("manufacturer")))
    ReadAndTrimString(battery_path_, "manufacturer", &info->battery_vendor);
  else
    ReadAndTrimString(battery_path_, "vendor", &info->battery_vendor);
  ReadAndTrimString(battery_path_, "model_name", &info->battery_model);
  ReadAndTrimString(battery_path_, "serial_number", &info->battery_serial);
  ReadAndTrimString(battery_path_, "technology", &info->battery_technology);
  return true;
}

void PowerSupply::SetSuspended(bool suspended) {
  if (is_suspended_ == suspended)
    return;

  is_suspended_ = suspended;
  SetCalculatingBatteryTime();
  if (is_suspended_) {
    VLOG(1) << "Stopping polling due to suspend";
    suspend_time_ = clock_->GetCurrentTime();
    util::RemoveTimeout(&poll_timeout_id_);
    current_poll_delay_for_testing_ = base::TimeDelta();
  } else {
    // base::TimeTicks::Now() doesn't increase while the system is
    // suspended; ensure that a stale timestamp isn't used.
    time_to_empty_average_.Clear();
    time_to_full_average_.Clear();
    // If resuming, deduct the time suspended from the hysteresis state
    // machine timestamps.
    AdjustHysteresisTimes(clock_->GetCurrentTime() - suspend_time_);
    RefreshImmediately();
  }
}

void PowerSupply::HandleUdevEvent() {
  VLOG(1) << "Heard about udev event";
  SetCalculatingBatteryTime();
  if (!is_suspended_)
    RefreshImmediately();
}

void PowerSupply::SetCalculatingBatteryTime() {
  VLOG(1) << "Waiting " << short_poll_delay_.InMilliseconds() << " ms before "
          << "calculating battery time";
  done_calculating_battery_time_timestamp_ =
      clock_->GetCurrentTime() + short_poll_delay_;
}

bool PowerSupply::UpdatePowerStatus() {
  VLOG(1) << "Updating power status";
  PowerStatus status;

  if (battery_path_.empty() || line_power_path_.empty())
    GetPowerSupplyPaths();

  std::string battery_status_string;
  if (file_util::PathExists(battery_path_)) {
    int64 present_value = 0;
    ReadInt64(battery_path_, "present", &present_value);
    status.battery_is_present = present_value != 0;
    if (status.battery_is_present)
      ReadAndTrimString(battery_path_, "status", &battery_status_string);
  } else {
    status.battery_is_present = false;
  }

  if (file_util::PathExists(line_power_path_)) {
    int64 online_value = 0;
    ReadInt64(line_power_path_, "online", &online_value);
    status.line_power_on = online_value != 0;
    ReadAndTrimString(line_power_path_, "type", &status.line_power_type);
    if (status.line_power_on) {
      status.external_power = IsUsbType(status.line_power_type) ?
          PowerSupplyProperties_ExternalPower_USB:
          PowerSupplyProperties_ExternalPower_AC;
    } else {
      status.external_power = PowerSupplyProperties_ExternalPower_DISCONNECTED;
    }
  } else if (!status.battery_is_present) {
    // If there's no battery, assume that the system is on AC power.
    status.line_power_on = true;
    status.line_power_type = kMainsType;
    status.external_power = PowerSupplyProperties_ExternalPower_AC;
  } else {
    // Otherwise, infer the external power source from the kernel-reported
    // battery status.
    status.line_power_on =
        battery_status_string == kBatteryStatusCharging ||
        battery_status_string == kBatteryStatusFull;
    status.external_power = status.line_power_on ?
        PowerSupplyProperties_ExternalPower_AC :
        PowerSupplyProperties_ExternalPower_DISCONNECTED;
  }

  // The rest of the calculations all require a battery.
  if (!status.battery_is_present) {
    status.battery_state = PowerSupplyProperties_BatteryState_NOT_PRESENT;
    power_status_ = status;
    return true;
  }

  double battery_voltage = ReadScaledDouble(battery_path_, "voltage_now");
  status.battery_voltage = battery_voltage;

  // Attempt to determine nominal voltage for time remaining calculations.
  // The battery voltage used in calculating time remaining.  This may or may
  // not be the same as the instantaneous voltage |battery_voltage|, as voltage
  // levels vary over the time the battery is charged or discharged.
  double nominal_voltage = -1.0;
  if (file_util::PathExists(battery_path_.Append("voltage_min_design")))
    nominal_voltage = ReadScaledDouble(battery_path_, "voltage_min_design");
  else if (file_util::PathExists(battery_path_.Append("voltage_max_design")))
    nominal_voltage = ReadScaledDouble(battery_path_, "voltage_max_design");

  // Nominal voltage is not required to obtain charge level.  If it is missing,
  // just log a message, set to |battery_voltage| so time remaining
  // calculations will function, and proceed.
  if (nominal_voltage <= 0) {
    LOG(WARNING) << "Invalid voltage_min/max_design reading: "
                 << nominal_voltage << "V."
                 << " Time remaining calculations will not be available.";
    nominal_voltage = battery_voltage;
  }
  status.nominal_voltage = nominal_voltage;

  // ACPI has two different battery types: charge_battery and energy_battery.
  // The main difference is that charge_battery type exposes
  // 1. current_now in A
  // 2. charge_{now, full, full_design} in Ah
  // while energy_battery type exposes
  // 1. power_now W
  // 2. energy_{now, full, full_design} in Wh
  // Change all the energy readings to charge format.
  // If both energy and charge reading are present (some non-ACPI drivers
  // expose both readings), read only the charge format.
  double battery_charge_full = 0;
  double battery_charge_full_design = 0;
  double battery_charge = 0;

  if (file_util::PathExists(battery_path_.Append("charge_full"))) {
    battery_charge_full = ReadScaledDouble(battery_path_, "charge_full");
    battery_charge_full_design =
        ReadScaledDouble(battery_path_, "charge_full_design");
    battery_charge = ReadScaledDouble(battery_path_, "charge_now");
  } else if (file_util::PathExists(battery_path_.Append("energy_full"))) {
    // Valid |battery_voltage| is required to determine the charge so return
    // early if it is not present. In this case, we know nothing about
    // battery state or remaining percentage, so set proper status.
    if (battery_voltage <= 0) {
      LOG(WARNING) << "Invalid voltage_now reading for energy-to-charge"
                   << " conversion: " << battery_voltage;
      return false;
    }
    battery_charge_full =
        ReadScaledDouble(battery_path_, "energy_full") / battery_voltage;
    battery_charge_full_design =
        ReadScaledDouble(battery_path_, "energy_full_design") / battery_voltage;
    battery_charge =
        ReadScaledDouble(battery_path_, "energy_now") / battery_voltage;
  } else {
    LOG(WARNING) << "No charge/energy readings for battery";
    return false;
  }

  status.battery_charge_full = battery_charge_full;
  status.battery_charge_full_design = battery_charge_full_design;
  status.battery_charge = battery_charge;
  if (status.battery_charge_full <= 0.0) {
    LOG(WARNING) << "Got battery-full charge of " << status.battery_charge_full;
    return false;
  }

  // The current can be reported as negative on some systems but not on
  // others, so it can't be used to determine whether the battery is
  // charging or discharging.
  double battery_current = 0;
  if (file_util::PathExists(battery_path_.Append("power_now"))) {
    battery_current = fabs(ReadScaledDouble(battery_path_, "power_now")) /
        battery_voltage;
  } else {
    battery_current = fabs(ReadScaledDouble(battery_path_, "current_now"));
  }
  status.battery_current = battery_current;

  status.battery_energy = battery_charge * nominal_voltage;
  status.battery_energy_rate = battery_current * battery_voltage;

  if (status.line_power_on)
    discharge_start_time_ = base::TimeTicks();
  else if (discharge_start_time_.is_null())
    discharge_start_time_ = clock_->GetCurrentTime();

  status.battery_percentage = util::ClampPercent(
      100.0 * battery_charge / battery_charge_full);
  status.display_battery_percentage = util::ClampPercent(
      100.0 * (status.battery_percentage - low_battery_shutdown_percent_) /
      (100.0 * full_factor_ - low_battery_shutdown_percent_));

  const bool battery_is_full =
      battery_charge >= battery_charge_full * full_factor_;

  if (status.line_power_on) {
    if (battery_is_full) {
      status.battery_state = PowerSupplyProperties_BatteryState_FULL;
    } else if (battery_current > 0.0 &&
               (battery_status_string == kBatteryStatusCharging ||
                battery_status_string == kBatteryStatusFull)) {
      status.battery_state = PowerSupplyProperties_BatteryState_CHARGING;
    } else {
      status.battery_state = PowerSupplyProperties_BatteryState_DISCHARGING;
    }
  } else {
    status.battery_state = PowerSupplyProperties_BatteryState_DISCHARGING;
  }

  status.is_calculating_battery_time =
      !done_calculating_battery_time_timestamp_.is_null() &&
      clock_->GetCurrentTime() < done_calculating_battery_time_timestamp_;

  UpdateRemainingTime(&status);
  UpdateAveragedTimes(&status);

  status.battery_below_shutdown_threshold =
      IsBatteryBelowShutdownThreshold(status);

  power_status_ = status;
  return true;
}

void PowerSupply::GetPowerSupplyPaths() {
  // First check if both line power and battery paths have been found and still
  // exist.  If so, there is no need to do anything else.
  if (file_util::PathExists(battery_path_) &&
      file_util::PathExists(line_power_path_))
    return;
  // Use a FileEnumerator to browse through all files/subdirectories in the
  // power supply sysfs directory.
  file_util::FileEnumerator file_enum(power_supply_path_, false,
      file_util::FileEnumerator::DIRECTORIES);
  // Read type info from all power sources, and try to identify battery and line
  // power sources.  Their paths are to be stored locally.
  for (base::FilePath path = file_enum.Next();
       !path.empty();
       path = file_enum.Next()) {
    // External devices have "scope" attributes containing the value "Device".
    // Skip them.
    FilePath scope_path = path.Append("scope");
    if (file_util::PathExists(scope_path)) {
      std::string buf;
      file_util::ReadFileToString(scope_path, &buf);
      TrimWhitespaceASCII(buf, TRIM_TRAILING, &buf);
      if (buf == "Device") {
        VLOG(1) << "Skipping Power supply " << path.value()
                << " with scope: " << buf;
        continue;
      }
    }
    std::string buf;
    if (file_util::ReadFileToString(path.Append("type"), &buf)) {
      TrimWhitespaceASCII(buf, TRIM_TRAILING, &buf);
      // Only look for battery / line power paths if they haven't been found
      // already.  This makes the assumption that they don't change (but battery
      // path can disappear if removed).  So this code should only be run once
      // for each power source.
      if (buf == kBatteryType && battery_path_.empty()) {
        VLOG(1) << "Battery path found: " << path.value();
        battery_path_ = path;
      } else if (buf != kBatteryType && line_power_path_.empty()) {
        VLOG(1) << "Line power path found: " << path.value();
        line_power_path_ = path;
      }
    }
  }
}

void PowerSupply::UpdateRemainingTime(PowerStatus* status) {
  base::TimeTicks time_now = clock_->GetCurrentTime();
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
  if (status->battery_current > 0) {
    double time_to_empty = 0;
    if (status->line_power_on) {
      status->battery_time_to_full =
          HoursToSecondsInt(std::max(status->battery_charge_full -
              status->battery_charge, 0.0) / status->battery_current);
      // Reset the remaining-time-calculation state machine when AC plugged in.
      found_acceptable_time_range_ = false;
      last_poll_time_ = base::TimeTicks();
      last_acceptable_range_time_ = base::TimeTicks();
      // Make sure that when the system switches to battery power, the initial
      // hysteresis time will be very short, so it can find an acceptable
      // battery remaining time as quickly as possible.
      hysteresis_time_ = kHysteresisTimeFast;
    } else if (!found_acceptable_time_range_) {
      // No base range found, need to give it some time to stabilize.  For now,
      // use the simple linear calculation for time.
      time_to_empty = GetLinearTimeToEmpty(*status);
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
      double calculated_time = GetLinearTimeToEmpty(*status);
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

void PowerSupply::UpdateAveragedTimes(PowerStatus* status) {
  // Some devices give us bogus values for battery information right after boot
  // or a power event. We attempt to avoid sampling at these times, but
  // this guard is to save us when we do sample a bad value. After working
  // out the time values, if we have a negative we know something is bad.
  // If the time we are interested in (to empty or full) is beyond a day
  // then we have a bad value since it is too high. For some devices the
  // value for the uninteresting time, that we are not using, might be
  // bizarre, so we cannot just check both times for overly high values.
  if ((status->line_power_on &&
       (status->battery_time_to_full < 0 ||
        status->battery_time_to_full > kBatteryTimeMaxValidSec)) ||
      (!status->line_power_on &&
       (status->battery_time_to_empty < 0 ||
        status->battery_time_to_empty > kBatteryTimeMaxValidSec))) {
    LOG(ERROR) << "Battery time-to-"
               << (status->line_power_on ? "full" : "empty") << " is "
               << (status->line_power_on ?
                   status->battery_time_to_full :
                   status->battery_time_to_empty);
    status->battery_times_are_bad = true;
    status->averaged_battery_time_to_empty = 0;
    status->averaged_battery_time_to_full = 0;
    status->is_calculating_battery_time = true;
    return;
  }

  int64 battery_time = 0;
  if (status->line_power_on) {
    battery_time = status->battery_time_to_full;
    if (!status->is_calculating_battery_time)
      time_to_full_average_.AddSample(battery_time);
    time_to_empty_average_.Clear();
  } else {
    // If the time threshold is set use it, otherwise determine the time
    // equivalent of the percentage threshold.
    int64 time_threshold_s = low_battery_shutdown_time_ > base::TimeDelta() ?
        low_battery_shutdown_time_.InSeconds() :
        (status->battery_time_to_empty *
         low_battery_shutdown_percent_ / status->battery_percentage);
    battery_time = std::max(static_cast<int64>(0),
        status->battery_time_to_empty - time_threshold_s);
    if (!status->is_calculating_battery_time)
      time_to_empty_average_.AddSample(battery_time);
    time_to_full_average_.Clear();
  }

  if (!status->is_calculating_battery_time) {
    if (!status->line_power_on)
      AdjustWindowSize(base::TimeDelta::FromSeconds(battery_time));
    else
      time_to_empty_average_.ChangeWindowSize(sample_window_max_);
  }
  status->averaged_battery_time_to_full = time_to_full_average_.GetAverage();
  status->averaged_battery_time_to_empty = time_to_empty_average_.GetAverage();
}

void PowerSupply::AdjustWindowSize(base::TimeDelta battery_time) {
  // For the rolling averages we want the window size to taper off in a linear
  // fashion from |sample_window_max| to |sample_window_min| on the battery time
  // remaining interval from |taper_time_max_| to |taper_time_min_|. The two
  // point equation for the line is:
  //   (x - x0)/(x1 - x0) = (t - t0)/(t1 - t0)
  // which solved for x is:
  //   x = (t - t0)*(x1 - x0)/(t1 - t0) + x0
  // We let x be the size of the window and t be the battery time
  // remaining.
  unsigned int window_size = sample_window_max_;
  if (battery_time >= taper_time_max_) {
    window_size = sample_window_max_;
  } else if (battery_time <= taper_time_min_) {
    window_size = sample_window_min_;
  } else {
    window_size = (battery_time - taper_time_min_).InSeconds();
    window_size *= (sample_window_max_ - sample_window_min_);
    window_size /= (taper_time_max_ - taper_time_min_).InSeconds();
    window_size += sample_window_min_;
  }
  time_to_empty_average_.ChangeWindowSize(window_size);
}

bool PowerSupply::IsBatteryBelowShutdownThreshold(
    const PowerStatus& status) const {
  if (low_battery_shutdown_time_ == base::TimeDelta() &&
      low_battery_shutdown_percent_ <= kEpsilon)
    return false;

  // TODO(derat): Figure out what's causing http://crosbug.com/38912.
  if (status.battery_percentage < 0.001) {
    LOG(WARNING) << "Ignoring probably-bogus zero battery percentage "
                 << "(time-to-empty is " << status.battery_time_to_empty
                 << " sec, time-to-full is " << status.battery_time_to_full
                 << " sec)";
    return false;
  }

  // If we're connected to an AC ("Mains") charger, don't shut down for a
  // low battery charge. Other types of chargers may not be able to deliver
  // enough current to prevent the battery from discharging, though.
  if (status.line_power_on && status.line_power_type == kMainsType)
    return false;

  return (status.battery_time_to_empty > 0 &&
          status.battery_time_to_empty <=
          low_battery_shutdown_time_.InSeconds()) ||
      status.battery_percentage <= low_battery_shutdown_percent_;
}

void PowerSupply::AdjustHysteresisTimes(const base::TimeDelta& offset) {
  if (!discharge_start_time_.is_null())
    discharge_start_time_ += offset;
  if (!last_acceptable_range_time_.is_null())
    last_acceptable_range_time_ += offset;
  if (!last_poll_time_.is_null())
    last_poll_time_ += offset;
}

void PowerSupply::SchedulePoll(bool last_poll_succeeded) {
  base::TimeDelta delay = last_poll_succeeded ? poll_delay_ : short_poll_delay_;
  base::TimeTicks now = clock_->GetCurrentTime();
  if (done_calculating_battery_time_timestamp_ > now) {
    delay = std::min(delay,
        done_calculating_battery_time_timestamp_ - now +
        base::TimeDelta::FromMilliseconds(kCalculateBatterySlackMs));
  }

  VLOG(1) << "Scheduling update in " << delay.InMilliseconds() << " ms";
  util::RemoveTimeout(&poll_timeout_id_);
  poll_timeout_id_ = g_timeout_add(
      delay.InMilliseconds(), HandlePollTimeoutThunk, this);
  current_poll_delay_for_testing_ = delay;
}

gboolean PowerSupply::HandlePollTimeout() {
  poll_timeout_id_ = 0;
  current_poll_delay_for_testing_ = base::TimeDelta();
  bool success = UpdatePowerStatus();
  SchedulePoll(success);
  if (success)
    FOR_EACH_OBSERVER(PowerSupplyObserver, observers_, OnPowerStatusUpdate());
  return FALSE;
}

gboolean PowerSupply::HandleNotifyObserversTimeout() {
  notify_observers_timeout_id_ = 0;
  FOR_EACH_OBSERVER(PowerSupplyObserver, observers_, OnPowerStatusUpdate());
  return FALSE;
}

}  // namespace system
}  // namespace power_manager
