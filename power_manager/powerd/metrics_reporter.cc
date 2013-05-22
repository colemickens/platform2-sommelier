// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/metrics_reporter.h"

#include <algorithm>
#include <cmath>

#include <glib.h>

#include "base/logging.h"
#include "metrics/metrics_library.h"
#include "power_manager/common/prefs.h"
#include "power_manager/common/util.h"
#include "power_manager/powerd/metrics_constants.h"
#include "power_manager/powerd/policy/backlight_controller.h"
#include "power_manager/powerd/system/power_supply.h"

namespace power_manager {

// static
bool MetricsReporter::CheckMetricInterval(time_t now,
                                          time_t last,
                                          time_t interval) {
  return now < last || now - last >= interval;
}

// static
std::string MetricsReporter::AppendPowerSourceToEnumName(
    const std::string& enum_name,
    PowerSource power_source) {
  switch (power_source) {
    case POWER_AC:
      return enum_name + kMetricACSuffix;
    case POWER_BATTERY:
      return enum_name + kMetricBatterySuffix;
  }
  NOTREACHED() << "Unhandled power source " << power_source;
  return enum_name;
}

MetricsReporter::MetricsReporter(
    PrefsInterface* prefs,
    MetricsLibraryInterface* metrics_lib,
    policy::BacklightController* backlight_controller)
    : prefs_(prefs),
      metrics_lib_(metrics_lib),
      backlight_controller_(backlight_controller),
      power_source_(POWER_AC),
      saw_initial_power_source_(false),
      generate_backlight_metrics_timeout_id_(0),
      generate_thermal_metrics_timeout_id_(0),
      battery_discharge_rate_metric_last_(0),
      battery_energy_before_suspend_(0.0),
      on_line_power_before_suspend_(false),
      report_battery_discharge_rate_while_suspended_(false) {
}

MetricsReporter::~MetricsReporter() {
  util::RemoveTimeout(&generate_backlight_metrics_timeout_id_);
  util::RemoveTimeout(&generate_thermal_metrics_timeout_id_);
}

void MetricsReporter::Init() {
  if (backlight_controller_) {
    generate_backlight_metrics_timeout_id_ = g_timeout_add(
        kMetricBacklightLevelIntervalMs,
        &MetricsReporter::GenerateBacklightLevelMetricThunk, this);
  }

  // Run GenerateThermalMetrics once now as it's a long interval.
  GenerateThermalMetrics();
  generate_thermal_metrics_timeout_id_ =
      g_timeout_add(kMetricThermalIntervalMs,
                    &MetricsReporter::GenerateThermalMetricsThunk, this);
}

void MetricsReporter::HandleScreenDimmedChange(
    bool dimmed,
    base::TimeTicks last_user_activity_time) {
  if (dimmed) {
    base::TimeTicks now = base::TimeTicks::Now();
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
    base::TimeTicks now = base::TimeTicks::Now();
    screen_off_timestamp_ = now;
    last_idle_event_timestamp_ = now;
    last_idle_timedelta_ = now - last_user_activity_time;
  } else {
    screen_off_timestamp_ = base::TimeTicks();
  }
}

void MetricsReporter::HandlePowerSourceChange(PowerSource power_source) {
  power_source_ = power_source;
  saw_initial_power_source_ = true;
}

void MetricsReporter::PrepareForSuspend(const system::PowerStatus& status,
                                        base::Time now) {
  battery_energy_before_suspend_ = status.battery_energy;
  on_line_power_before_suspend_ = status.line_power_on;
  time_before_suspend_ = now;
}

void MetricsReporter::HandleResume() {
  // Report the discharge rate in response to the next
  // GenerateMetricsOnPowerEvent() call.
  report_battery_discharge_rate_while_suspended_ = true;
}

bool MetricsReporter::SendMetric(const std::string& name,
                                 int sample,
                                 int min,
                                 int max,
                                 int nbuckets) {
  VLOG(1) << "Sending metric: " << name << " " << sample << " "
          << min << " " << max << " " << nbuckets;
  return metrics_lib_->SendToUMA(name, sample, min, max, nbuckets);
}

bool MetricsReporter::SendEnumMetric(const std::string& name,
                                     int sample,
                                     int max) {
  VLOG(1) << "Sending enum metric: " << name << " " << sample << " " << max;
  return metrics_lib_->SendEnumToUMA(name, sample, max);
}

bool MetricsReporter::SendMetricWithPowerSource(const std::string& name,
                                                int sample,
                                                int min,
                                                int max,
                                                int nbuckets) {
  if (!saw_initial_power_source_)
    return false;
  return SendMetric(AppendPowerSourceToEnumName(name, power_source_),
                    sample, min, max, nbuckets);
}

bool MetricsReporter::SendEnumMetricWithPowerSource(const std::string& name,
                                                    int sample,
                                                    int max) {
  if (!saw_initial_power_source_)
    return false;
  return SendEnumMetric(AppendPowerSourceToEnumName(name, power_source_),
                        sample, max);
}

void MetricsReporter::GenerateRetrySuspendMetric(int num_retries,
                                                 int max_retries) {
  if (num_retries == 0)
    return;

  SendMetric(kMetricRetrySuspendCountName, num_retries,
             kMetricRetrySuspendCountMin, max_retries,
             kMetricRetrySuspendCountBuckets);
}

void MetricsReporter::GenerateUserActivityMetrics() {
  if (last_idle_event_timestamp_.is_null())
    return;

  base::TimeTicks current_time = base::TimeTicks::Now();
  base::TimeDelta event_delta = current_time - last_idle_event_timestamp_;
  base::TimeDelta total_delta = event_delta + last_idle_timedelta_;
  last_idle_event_timestamp_ = base::TimeTicks();

  SendMetricWithPowerSource(kMetricIdleName, total_delta.InMilliseconds(),
      kMetricIdleMin, kMetricIdleMax, kMetricIdleBuckets);

  if (!screen_dim_timestamp_.is_null()) {
    base::TimeDelta dim_event_delta = current_time - screen_dim_timestamp_;
    SendMetricWithPowerSource(kMetricIdleAfterDimName,
                              dim_event_delta.InMilliseconds(),
                              kMetricIdleAfterDimMin,
                              kMetricIdleAfterDimMax,
                              kMetricIdleAfterDimBuckets);
    screen_dim_timestamp_ = base::TimeTicks();
  }
  if (!screen_off_timestamp_.is_null()) {
    base::TimeDelta screen_off_event_delta =
        current_time - screen_off_timestamp_;
    SendMetricWithPowerSource(kMetricIdleAfterScreenOffName,
                              screen_off_event_delta.InMilliseconds(),
                              kMetricIdleAfterScreenOffMin,
                              kMetricIdleAfterScreenOffMax,
                              kMetricIdleAfterScreenOffBuckets);
    screen_off_timestamp_ = base::TimeTicks();
  }
}

void MetricsReporter::GenerateMetricsOnPowerEvent(
    const system::PowerStatus& info) {
  GenerateBatteryDischargeRateMetric(info, time(NULL));
  GenerateBatteryDischargeRateWhileSuspendedMetric(info, base::Time::Now());

  SendEnumMetric(kMetricBatteryInfoSampleName,
                 BATTERY_INFO_READ,
                 BATTERY_INFO_MAX);
  if (info.battery_times_are_bad) {
    SendEnumMetric(kMetricBatteryInfoSampleName,
                   BATTERY_INFO_BAD,
                   BATTERY_INFO_MAX);
  } else {
    SendEnumMetric(kMetricBatteryInfoSampleName,
                   BATTERY_INFO_GOOD,
                   BATTERY_INFO_MAX);
  }
}

gboolean MetricsReporter::GenerateBacklightLevelMetric() {
  double percent = 0.0;
  if (screen_dim_timestamp_.is_null() && screen_off_timestamp_.is_null() &&
      backlight_controller_ &&
      backlight_controller_->GetBrightnessPercent(&percent)) {
    SendEnumMetricWithPowerSource(kMetricBacklightLevelName, lround(percent),
                                  kMetricBacklightLevelMax);
  }
  return TRUE;
}

bool MetricsReporter::GenerateBatteryDischargeRateMetric(
    const system::PowerStatus& info,
    time_t now) {
  // The battery discharge rate metric is relevant and collected only
  // when running on battery.
  if (!saw_initial_power_source_ || power_source_ != POWER_BATTERY)
    return false;

  // Converts the discharge rate from W to mW.
  int rate = static_cast<int>(round(info.battery_energy_rate * 1000));
  if (rate <= 0)
    return false;

  // Ensures that the metric is not generated too frequently.
  if (!CheckMetricInterval(now, battery_discharge_rate_metric_last_,
                           kMetricBatteryDischargeRateInterval))
    return false;

  if (!SendMetric(kMetricBatteryDischargeRateName, rate,
                  kMetricBatteryDischargeRateMin,
                  kMetricBatteryDischargeRateMax,
                  kMetricBatteryDischargeRateBuckets))
    return false;

  battery_discharge_rate_metric_last_ = now;
  return true;
}

bool MetricsReporter::GenerateBatteryDischargeRateWhileSuspendedMetric(
    const system::PowerStatus& status,
    base::Time now) {
  // Do nothing unless this is the first time we're called after resuming.
  if (!report_battery_discharge_rate_while_suspended_)
    return false;
  report_battery_discharge_rate_while_suspended_ = false;

  if (on_line_power_before_suspend_ || status.line_power_on)
    return false;

  base::TimeDelta elapsed_time = now - time_before_suspend_;
  if (elapsed_time.InSeconds() <
      kMetricBatteryDischargeRateWhileSuspendedMinSuspendSec)
    return false;

  double discharged_watt_hours =
      battery_energy_before_suspend_ - status.battery_energy;
  double discharge_rate_watts =
      discharged_watt_hours / (elapsed_time.InSecondsF() / 3600);

  // Maybe the charger was connected while the system was suspended but
  // disconnected before it resumed.
  if (discharge_rate_watts < 0.0)
    return false;

  return SendMetric(kMetricBatteryDischargeRateWhileSuspendedName,
                    static_cast<int>(round(discharge_rate_watts * 1000)),
                    kMetricBatteryDischargeRateWhileSuspendedMin,
                    kMetricBatteryDischargeRateWhileSuspendedMax,
                    kMetricBatteryDischargeRateWhileSuspendedBuckets);
}

void MetricsReporter::GenerateBatteryInfoWhenChargeStartsMetric(
    const system::PowerStatus& power_status) {
  // Need to make sure that we are actually charging a battery
  if (!power_status.line_power_on || !power_status.battery_is_present)
    return;

  int charge = static_cast<int>(round(power_status.battery_percentage));
  LOG_IF(ERROR, !SendEnumMetric(kMetricBatteryRemainingWhenChargeStartsName,
                                charge,
                                kMetricBatteryRemainingWhenChargeStartsMax))
      << "Unable to send battery remaining when charge starts metric!";

  charge = static_cast<int>(round(power_status.battery_charge_full /
                                  power_status.battery_charge_full_design *
                                  100));

  LOG_IF(ERROR, !SendEnumMetric(kMetricBatteryChargeHealthName, charge,
                            kMetricBatteryChargeHealthMax))
      << "Unable to send battery charge health metric!";
}

void MetricsReporter::GenerateEndOfSessionMetrics(
    const system::PowerStatus& info,
    const base::TimeTicks& now,
    const base::TimeTicks& start) {
  if (!GenerateBatteryRemainingAtEndOfSessionMetric(info))
    LOG(ERROR) << "Unable to generate battery remaining metric";
  if (!GenerateNumberOfAlsAdjustmentsPerSessionMetric())
    LOG(ERROR) << "Unable to generate ALS adjustments per session";
  if (!GenerateUserBrightnessAdjustmentsPerSessionMetric())
    LOG(ERROR) << "Unable to generate user brightness adjustments per session";
  if (!GenerateLengthOfSessionMetric(now, start))
    LOG(ERROR) << "Unable to generate length of session metric";
}

bool MetricsReporter::GenerateBatteryRemainingAtEndOfSessionMetric(
    const system::PowerStatus& info) {
  int charge = static_cast<int>(round(info.battery_percentage));
  return SendEnumMetricWithPowerSource(
      kMetricBatteryRemainingAtEndOfSessionName, charge,
      kMetricBatteryRemainingAtEndOfSessionMax);
}

bool MetricsReporter::GenerateBatteryRemainingAtStartOfSessionMetric(
    const system::PowerStatus& info) {
  int charge = static_cast<int>(round(info.battery_percentage));
  return SendEnumMetricWithPowerSource(
      kMetricBatteryRemainingAtStartOfSessionName, charge,
      kMetricBatteryRemainingAtStartOfSessionMax);
}

bool MetricsReporter::GenerateNumberOfAlsAdjustmentsPerSessionMetric() {
  if (!backlight_controller_)
    return false;

  int num_of_adjustments =
      backlight_controller_->GetNumAmbientLightSensorAdjustments();

  if (num_of_adjustments < 0) {
    LOG(ERROR) <<
        "Generated negative value for NumberOfAlsAdjustmentsPerSession Metrics";
    return false;
  } else if (num_of_adjustments > kMetricNumberOfAlsAdjustmentsPerSessionMax) {
    LOG(INFO) << "Clamping value for NumberOfAlsAdjustmentsPerSession to "
              << kMetricNumberOfAlsAdjustmentsPerSessionMax;
    num_of_adjustments = kMetricNumberOfAlsAdjustmentsPerSessionMax;
  }

  return SendMetric(kMetricNumberOfAlsAdjustmentsPerSessionName,
                    num_of_adjustments,
                    kMetricNumberOfAlsAdjustmentsPerSessionMin,
                    kMetricNumberOfAlsAdjustmentsPerSessionMax,
                    kMetricNumberOfAlsAdjustmentsPerSessionBuckets);
}

bool MetricsReporter::GenerateUserBrightnessAdjustmentsPerSessionMetric() {
  if (!backlight_controller_)
    return false;

  int adjustment_count = backlight_controller_->GetNumUserAdjustments();
  if (adjustment_count < 0) {
    LOG(ERROR) << "Calculation for user brightness adjustments per session "
               << "returned a negative value";
    return false;
  } else if (adjustment_count > kMetricUserBrightnessAdjustmentsPerSessionMax) {
    adjustment_count = kMetricUserBrightnessAdjustmentsPerSessionMax;
  }

  return SendMetricWithPowerSource(
      kMetricUserBrightnessAdjustmentsPerSessionName, adjustment_count,
      kMetricUserBrightnessAdjustmentsPerSessionMin,
      kMetricUserBrightnessAdjustmentsPerSessionMax,
      kMetricUserBrightnessAdjustmentsPerSessionBuckets);
}

bool MetricsReporter::GenerateLengthOfSessionMetric(
    const base::TimeTicks& now,
    const base::TimeTicks& start) {
  int session_length = (now - start).InSeconds();
  if (session_length < 0) {
    LOG(ERROR) << "Calculation for length of session returned a negative value";

    return false;
  } else if (session_length > kMetricLengthOfSessionMax) {
    LOG(INFO) << "Clamping LengthOfSession metric to "
              << kMetricLengthOfSessionMax;

    session_length = kMetricLengthOfSessionMax;
  }

  return SendMetric(kMetricLengthOfSessionName,
                    session_length,
                    kMetricLengthOfSessionMin,
                    kMetricLengthOfSessionMax,
                    kMetricLengthOfSessionBuckets);
}

void MetricsReporter::IncrementNumOfSessionsPerChargeMetric() {
  int64 num = 0;
  prefs_->GetInt64(kNumSessionsOnCurrentChargePref, &num);
  num = std::max(num, static_cast<int64>(0));
  prefs_->SetInt64(kNumSessionsOnCurrentChargePref, num + 1);
}

bool MetricsReporter::GenerateNumOfSessionsPerChargeMetric() {
  int64 sample = 0;
  prefs_->GetInt64(kNumSessionsOnCurrentChargePref, &sample);
  if (sample <= 0)
    return true;

  sample = std::min(
      sample, static_cast<int64>(kMetricNumOfSessionsPerChargeMax));
  prefs_->SetInt64(kNumSessionsOnCurrentChargePref, 0);
  return SendMetric(kMetricNumOfSessionsPerChargeName,
                    sample,
                    kMetricNumOfSessionsPerChargeMin,
                    kMetricNumOfSessionsPerChargeMax,
                    kMetricNumOfSessionsPerChargeBuckets);
}

void MetricsReporter::GeneratePowerButtonMetric(bool down,
                                                base::TimeTicks timestamp) {
  // Just keep track of the time when the button was pressed.
  if (down) {
    if (!last_power_button_down_timestamp_.is_null())
      LOG(ERROR) << "Got power-button-down event while button was already down";
    last_power_button_down_timestamp_ = timestamp;
    return;
  }

  // Metrics are sent after the button is released.
  if (last_power_button_down_timestamp_.is_null()) {
    LOG(ERROR) << "Got power-button-up event while button was already up";
    return;
  }
  base::TimeDelta delta = timestamp - last_power_button_down_timestamp_;
  if (delta.InMilliseconds() < 0) {
    LOG(ERROR) << "Negative duration between power button events";
    return;
  }
  last_power_button_down_timestamp_ = base::TimeTicks();
  if (!SendMetric(kMetricPowerButtonDownTimeName,
                  delta.InMilliseconds(),
                  kMetricPowerButtonDownTimeMin,
                  kMetricPowerButtonDownTimeMax,
                  kMetricPowerButtonDownTimeBuckets)) {
    LOG(ERROR) << "Could not send " << kMetricPowerButtonDownTimeName;
  }
}

void MetricsReporter::SendThermalMetrics(unsigned int aborted,
                                         unsigned int turned_on,
                                         unsigned int multiple) {
  unsigned int total = aborted + turned_on;
  if (total == 0) {
    LOG(WARNING) << "SendThermalMetrics: total is 0 (aborted = "
                 << aborted << ", turnd_on = " << turned_on << ")";
    return;
  }

  unsigned int aborted_percent = (100 * aborted) / total;
  unsigned int multiple_percent = (100 * multiple) / total;

  LOG_IF(ERROR, !SendEnumMetric(kMetricThermalAbortedFanTurnOnName,
                                aborted_percent,
                                kMetricThermalAbortedFanTurnOnMax))
      << "Unable to send aborted fan turn on metric!";
  LOG_IF(ERROR, !SendEnumMetric(kMetricThermalMultipleFanTurnOnName,
                                multiple_percent,
                                kMetricThermalMultipleFanTurnOnMax))
      << "Unable to send multiple fan turn on metric!";
}

gboolean MetricsReporter::GenerateThermalMetrics() {
  unsigned int aborted = 0, turned_on = 0, multiple = 0;
  if (!util::GetUintFromFile(kMetricThermalAbortedFanFilename, &aborted) ||
      !util::GetUintFromFile(kMetricThermalTurnedOnFanFilename, &turned_on) ||
      !util::GetUintFromFile(kMetricThermalMultipleFanFilename, &multiple)) {
    LOG(ERROR) << "Unable to read values from debugfs thermal files. "
               << "UMA metrics not being sent this poll period.";
    return TRUE;
  }

  SendThermalMetrics(aborted, turned_on, multiple);
  return TRUE;
}

}  // namespace power_manager
