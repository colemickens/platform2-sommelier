// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/metrics_reporter.h"

#include <algorithm>
#include <cmath>

#include "base/logging.h"
#include "metrics/metrics_library.h"
#include "power_manager/common/prefs.h"
#include "power_manager/common/util.h"
#include "power_manager/powerd/metrics_constants.h"
#include "power_manager/powerd/policy/backlight_controller.h"

namespace power_manager {

// static
std::string MetricsReporter::AppendPowerSourceToEnumName(
    const std::string& enum_name,
    PowerSource power_source) {
  return enum_name +
      (power_source == POWER_AC ? kMetricACSuffix : kMetricBatterySuffix);
}

MetricsReporter::MetricsReporter()
    : prefs_(NULL),
      metrics_lib_(NULL),
      display_backlight_controller_(NULL),
      keyboard_backlight_controller_(NULL),
      session_state_(SESSION_STOPPED),
      battery_energy_before_suspend_(0.0),
      on_line_power_before_suspend_(false),
      report_battery_discharge_rate_while_suspended_(false) {
}

MetricsReporter::~MetricsReporter() {}

void MetricsReporter::Init(
    PrefsInterface* prefs,
    MetricsLibraryInterface* metrics_lib,
    policy::BacklightController* display_backlight_controller,
    policy::BacklightController* keyboard_backlight_controller,
    const system::PowerStatus& power_status) {
  prefs_ = prefs;
  metrics_lib_ = metrics_lib;
  display_backlight_controller_ = display_backlight_controller;
  keyboard_backlight_controller_ = keyboard_backlight_controller;
  last_power_status_ = power_status;

  if (display_backlight_controller_ || keyboard_backlight_controller_) {
    generate_backlight_metrics_timer_.Start(FROM_HERE,
        base::TimeDelta::FromMilliseconds(kMetricBacklightLevelIntervalMs),
        this, &MetricsReporter::GenerateBacklightLevelMetrics);
  }
}

void MetricsReporter::HandleScreenDimmedChange(
    bool dimmed,
    base::TimeTicks last_user_activity_time) {
  if (dimmed) {
    base::TimeTicks now = clock_.GetCurrentTime();
    screen_dim_timestamp_ = now;
    last_idle_event_timestamp_ = now;
    last_idle_timedelta_ = now - last_user_activity_time;
  } else {
    screen_dim_timestamp_ = base::TimeTicks();
  }
}

void MetricsReporter::HandleScreenOffChange(
    bool off,
    base::TimeTicks last_user_activity_time) {
  if (off) {
    base::TimeTicks now = clock_.GetCurrentTime();
    screen_off_timestamp_ = now;
    last_idle_event_timestamp_ = now;
    last_idle_timedelta_ = now - last_user_activity_time;
  } else {
    screen_off_timestamp_ = base::TimeTicks();
  }
}

void MetricsReporter::HandleSessionStateChange(SessionState state) {
  if (state == session_state_)
    return;

  session_state_ = state;

  switch (state) {
    case SESSION_STARTED:
      session_start_time_ = clock_.GetCurrentTime();
      if (!last_power_status_.line_power_on)
        IncrementNumOfSessionsPerChargeMetric();
      if (last_power_status_.battery_is_present) {
        // Enum to avoid exponential histogram's varyingly-sized buckets.
        SendEnumMetricWithPowerSource(
            kMetricBatteryRemainingAtStartOfSessionName,
            static_cast<int>(round(last_power_status_.battery_percentage)),
            kMetricMaxPercent);
      }
      break;
    case SESSION_STOPPED: {
      if (last_power_status_.battery_is_present) {
        // Enum to avoid exponential histogram's varyingly-sized buckets.
        SendEnumMetricWithPowerSource(
            kMetricBatteryRemainingAtEndOfSessionName,
            static_cast<int>(round(last_power_status_.battery_percentage)),
            kMetricMaxPercent);
      }

      const int session_length_sec = std::min(kMetricLengthOfSessionMax,
          static_cast<int>(
              (clock_.GetCurrentTime() - session_start_time_).InSeconds()));
      SendMetric(kMetricLengthOfSessionName,
                 session_length_sec,
                 kMetricLengthOfSessionMin,
                 kMetricLengthOfSessionMax,
                 kMetricDefaultBuckets);

      if (display_backlight_controller_) {
        SendMetric(kMetricNumberOfAlsAdjustmentsPerSessionName,
                   display_backlight_controller_->
                       GetNumAmbientLightSensorAdjustments(),
                   kMetricNumberOfAlsAdjustmentsPerSessionMin,
                   kMetricNumberOfAlsAdjustmentsPerSessionMax,
                   kMetricDefaultBuckets);
        SendMetricWithPowerSource(
            kMetricUserBrightnessAdjustmentsPerSessionName,
            display_backlight_controller_->GetNumUserAdjustments(),
            kMetricUserBrightnessAdjustmentsPerSessionMin,
            kMetricUserBrightnessAdjustmentsPerSessionMax,
            kMetricDefaultBuckets);
      }
      break;
    }
  }
}

void MetricsReporter::HandlePowerStatusUpdate(
    const system::PowerStatus& status) {
  const bool previously_on_line_power = last_power_status_.line_power_on;
  last_power_status_ = status;

  if (status.line_power_on && !previously_on_line_power) {
    GenerateNumOfSessionsPerChargeMetric();
    if (status.battery_is_present) {
      // Enum to avoid exponential histogram's varyingly-sized buckets.
      SendEnumMetric(kMetricBatteryRemainingWhenChargeStartsName,
                     static_cast<int>(round(status.battery_percentage)),
                     kMetricMaxPercent);
      SendEnumMetric(kMetricBatteryChargeHealthName,
                     static_cast<int>(round(100.0 * status.battery_charge_full /
                                            status.battery_charge_full_design)),
                     kMetricBatteryChargeHealthMax);
    }
  } else if (!status.line_power_on && previously_on_line_power) {
    if (session_state_ == SESSION_STARTED)
      IncrementNumOfSessionsPerChargeMetric();
  }

  GenerateBatteryDischargeRateMetric();
  GenerateBatteryDischargeRateWhileSuspendedMetric();

  SendEnumMetric(kMetricBatteryInfoSampleName,
                 BATTERY_INFO_READ,
                 BATTERY_INFO_MAX);
  // TODO(derat): Continue sending BATTERY_INFO_BAD in some situations?
  // Remove this metric entirely?
  SendEnumMetric(kMetricBatteryInfoSampleName,
                 BATTERY_INFO_GOOD,
                 BATTERY_INFO_MAX);
}

void MetricsReporter::HandleShutdown(ShutdownReason reason) {
  SendEnumMetric(kMetricShutdownReasonName, static_cast<int>(reason),
                 kMetricShutdownReasonMax);
}

void MetricsReporter::PrepareForSuspend() {
  battery_energy_before_suspend_ = last_power_status_.battery_energy;
  on_line_power_before_suspend_ = last_power_status_.line_power_on;
  time_before_suspend_ = clock_.GetCurrentWallTime();
}

void MetricsReporter::HandleResume(int num_suspend_attempts) {
  SendMetric(kMetricSuspendAttemptsBeforeSuccessName, num_suspend_attempts,
             kMetricSuspendAttemptsMin, kMetricSuspendAttemptsMax,
             kMetricSuspendAttemptsBuckets);
  // Report the discharge rate in response to the next
  // OnPowerStatusUpdate() call.
  report_battery_discharge_rate_while_suspended_ = true;
}

void MetricsReporter::HandleCanceledSuspendRequest(int num_suspend_attempts) {
  SendMetric(kMetricSuspendAttemptsBeforeCancelName, num_suspend_attempts,
             kMetricSuspendAttemptsMin, kMetricSuspendAttemptsMax,
             kMetricSuspendAttemptsBuckets);
}

void MetricsReporter::GenerateUserActivityMetrics() {
  if (last_idle_event_timestamp_.is_null())
    return;

  base::TimeTicks current_time = clock_.GetCurrentTime();
  base::TimeDelta event_delta = current_time - last_idle_event_timestamp_;
  base::TimeDelta total_delta = event_delta + last_idle_timedelta_;
  last_idle_event_timestamp_ = base::TimeTicks();

  SendMetricWithPowerSource(kMetricIdleName, total_delta.InMilliseconds(),
      kMetricIdleMin, kMetricIdleMax, kMetricDefaultBuckets);

  if (!screen_dim_timestamp_.is_null()) {
    base::TimeDelta dim_event_delta = current_time - screen_dim_timestamp_;
    SendMetricWithPowerSource(kMetricIdleAfterDimName,
                              dim_event_delta.InMilliseconds(),
                              kMetricIdleAfterDimMin,
                              kMetricIdleAfterDimMax,
                              kMetricDefaultBuckets);
    screen_dim_timestamp_ = base::TimeTicks();
  }
  if (!screen_off_timestamp_.is_null()) {
    base::TimeDelta screen_off_event_delta =
        current_time - screen_off_timestamp_;
    SendMetricWithPowerSource(kMetricIdleAfterScreenOffName,
                              screen_off_event_delta.InMilliseconds(),
                              kMetricIdleAfterScreenOffMin,
                              kMetricIdleAfterScreenOffMax,
                              kMetricDefaultBuckets);
    screen_off_timestamp_ = base::TimeTicks();
  }
}

void MetricsReporter::GenerateBacklightLevelMetrics() {
  if (!screen_dim_timestamp_.is_null() || !screen_off_timestamp_.is_null())
    return;

  double percent = 0.0;
  if (display_backlight_controller_ &&
      display_backlight_controller_->GetBrightnessPercent(&percent)) {
    // Enum to avoid exponential histogram's varyingly-sized buckets.
    SendEnumMetricWithPowerSource(kMetricBacklightLevelName, lround(percent),
                                  kMetricMaxPercent);
  }
  if (keyboard_backlight_controller_ &&
      keyboard_backlight_controller_->GetBrightnessPercent(&percent)) {
    // Enum to avoid exponential histogram's varyingly-sized buckets.
    SendEnumMetric(kMetricKeyboardBacklightLevelName, lround(percent),
                   kMetricMaxPercent);
  }
}

void MetricsReporter::HandlePowerButtonEvent(ButtonState state) {
  // Just keep track of the time when the button was pressed.
  if (state == BUTTON_DOWN) {
    if (!last_power_button_down_timestamp_.is_null())
      LOG(ERROR) << "Got power-button-down event while button was already down";
    last_power_button_down_timestamp_ = clock_.GetCurrentTime();
    return;
  }

  // Metrics are sent after the button is released.
  if (last_power_button_down_timestamp_.is_null()) {
    LOG(ERROR) << "Got power-button-up event while button was already up";
    return;
  }
  base::TimeDelta delta =
      clock_.GetCurrentTime() - last_power_button_down_timestamp_;
  last_power_button_down_timestamp_ = base::TimeTicks();
  SendMetric(kMetricPowerButtonDownTimeName,
             delta.InMilliseconds(),
             kMetricPowerButtonDownTimeMin,
             kMetricPowerButtonDownTimeMax,
             kMetricDefaultBuckets);
}

void MetricsReporter::SendPowerButtonAcknowledgmentDelayMetric(
    base::TimeDelta delay) {
  SendMetric(kMetricPowerButtonAcknowledgmentDelayName,
             delay.InMilliseconds(),
             kMetricPowerButtonAcknowledgmentDelayMin,
             kMetricPowerButtonAcknowledgmentDelayMax,
             kMetricDefaultBuckets);
}

bool MetricsReporter::SendMetric(const std::string& name,
                                 int sample,
                                 int min,
                                 int max,
                                 int nbuckets) {
  VLOG(1) << "Sending metric " << name << " (sample=" << sample << " min="
          << min << " max=" << max << " nbuckets=" << nbuckets << ")";

  if (sample < min) {
    LOG(WARNING) << name << " sample " << sample << " is less than " << min;
    sample = min;
  } else if (sample > max) {
    LOG(WARNING) << name << " sample " << sample << " is greater than " << max;
    sample = max;
  }

  if (!metrics_lib_->SendToUMA(name, sample, min, max, nbuckets)) {
    LOG(ERROR) << "Failed to send metric " << name;
    return false;
  }
  return true;
}

bool MetricsReporter::SendEnumMetric(const std::string& name,
                                     int sample,
                                     int max) {
  VLOG(1) << "Sending enum metric " << name << " (sample=" << sample
          << " max=" << max << ")";

  if (sample > max) {
    LOG(WARNING) << name << " sample " << sample << " is greater than " << max;
    sample = max;
  }

  if (!metrics_lib_->SendEnumToUMA(name, sample, max)) {
    LOG(ERROR) << "Failed to send enum metric " << name;
    return false;
  }
  return true;
}

bool MetricsReporter::SendMetricWithPowerSource(const std::string& name,
                                                int sample,
                                                int min,
                                                int max,
                                                int nbuckets) {
  const std::string full_name = AppendPowerSourceToEnumName(
      name, last_power_status_.line_power_on ? POWER_AC : POWER_BATTERY);
  return SendMetric(full_name, sample, min, max, nbuckets);
}

bool MetricsReporter::SendEnumMetricWithPowerSource(const std::string& name,
                                                    int sample,
                                                    int max) {
  const std::string full_name = AppendPowerSourceToEnumName(
      name, last_power_status_.line_power_on ? POWER_AC : POWER_BATTERY);
  return SendEnumMetric(full_name, sample, max);
}

void MetricsReporter::GenerateBatteryDischargeRateMetric() {
  // The battery discharge rate metric is relevant and collected only
  // when running on battery.
  if (!last_power_status_.battery_is_present ||
      last_power_status_.line_power_on)
    return;

  // Converts the discharge rate from W to mW.
  int rate = static_cast<int>(
      round(last_power_status_.battery_energy_rate * 1000));
  if (rate <= 0)
    return;

  // Ensures that the metric is not generated too frequently.
  if (!last_battery_discharge_rate_metric_timestamp_.is_null() &&
      (clock_.GetCurrentTime() -
       last_battery_discharge_rate_metric_timestamp_).InSeconds() <
      kMetricBatteryDischargeRateInterval) {
    return;
  }

  if (SendMetric(kMetricBatteryDischargeRateName, rate,
                 kMetricBatteryDischargeRateMin,
                 kMetricBatteryDischargeRateMax,
                 kMetricDefaultBuckets))
    last_battery_discharge_rate_metric_timestamp_ = clock_.GetCurrentTime();
}

void MetricsReporter::GenerateBatteryDischargeRateWhileSuspendedMetric() {
  // Do nothing unless this is the first time we're called after resuming.
  if (!report_battery_discharge_rate_while_suspended_)
    return;
  report_battery_discharge_rate_while_suspended_ = false;

  if (!last_power_status_.battery_is_present ||
      on_line_power_before_suspend_ ||
      last_power_status_.line_power_on)
    return;

  base::TimeDelta elapsed_time =
      clock_.GetCurrentWallTime() - time_before_suspend_;
  if (elapsed_time.InSeconds() <
      kMetricBatteryDischargeRateWhileSuspendedMinSuspendSec)
    return;

  double discharged_watt_hours =
      battery_energy_before_suspend_ - last_power_status_.battery_energy;
  double discharge_rate_watts =
      discharged_watt_hours / (elapsed_time.InSecondsF() / 3600);

  // Maybe the charger was connected while the system was suspended but
  // disconnected before it resumed.
  if (discharge_rate_watts < 0.0)
    return;

  SendMetric(kMetricBatteryDischargeRateWhileSuspendedName,
             static_cast<int>(round(discharge_rate_watts * 1000)),
             kMetricBatteryDischargeRateWhileSuspendedMin,
             kMetricBatteryDischargeRateWhileSuspendedMax,
             kMetricDefaultBuckets);
}

void MetricsReporter::IncrementNumOfSessionsPerChargeMetric() {
  int64 num = 0;
  prefs_->GetInt64(kNumSessionsOnCurrentChargePref, &num);
  num = std::max(num, static_cast<int64>(0));
  prefs_->SetInt64(kNumSessionsOnCurrentChargePref, num + 1);
}

void MetricsReporter::GenerateNumOfSessionsPerChargeMetric() {
  int64 sample = 0;
  prefs_->GetInt64(kNumSessionsOnCurrentChargePref, &sample);
  if (sample <= 0)
    return;

  sample = std::min(
      sample, static_cast<int64>(kMetricNumOfSessionsPerChargeMax));
  prefs_->SetInt64(kNumSessionsOnCurrentChargePref, 0);
  SendMetric(kMetricNumOfSessionsPerChargeName,
             sample,
             kMetricNumOfSessionsPerChargeMin,
             kMetricNumOfSessionsPerChargeMax,
             kMetricDefaultBuckets);
}

}  // namespace power_manager
