// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/powerd.h"

#include <cmath>

#include "base/logging.h"
#include "power_manager/common/util.h"
#include "power_manager/powerd/metrics_constants.h"
#include "power_manager/powerd/metrics_store.h"

using base::TimeDelta;
using base::TimeTicks;

namespace power_manager {

// Checks if |now| is the time to generate a new sample of a given
// metric. Returns true if the last metric sample was generated at
// least |interval| seconds ago (or if there is a clock jump back in
// time).
bool CheckMetricInterval(time_t now, time_t last, time_t interval) {
  return now < last || now - last >= interval;
}

void Daemon::MetricInit() {
  g_timeout_add(kMetricBacklightLevelIntervalMs,
                &Daemon::GenerateBacklightLevelMetricThunk, this);
  // Run GenerateThermalMetrics once now as its a long interval
  GenerateThermalMetrics();
  g_timeout_add(kMetricThermalIntervalMs,
                &Daemon::GenerateThermalMetricsThunk, this);
}

void Daemon::GenerateMetricsOnIdleEvent(bool is_idle, int64 idle_time_ms) {
  TimeTicks current_time = TimeTicks::Now();
  TimeDelta event_delta = current_time - last_idle_event_timestamp_;
  TimeDelta total_delta = event_delta + last_idle_timedelta_;
  last_idle_event_timestamp_ = TimeTicks();

  SendMetricWithPowerState(kMetricIdleName, total_delta.InMilliseconds(),
      kMetricIdleMin, kMetricIdleMax, kMetricIdleBuckets);

  if (!idle_transition_timestamps_[BACKLIGHT_DIM].is_null()) {
    TimeDelta dim_event_delta =
      current_time - idle_transition_timestamps_[BACKLIGHT_DIM];
    SendMetricWithPowerState(kMetricIdleAfterDimName,
                             dim_event_delta.InMilliseconds(),
                             kMetricIdleAfterDimMin,
                             kMetricIdleAfterDimMax,
                             kMetricIdleAfterDimBuckets);
  }
  if (!idle_transition_timestamps_[BACKLIGHT_IDLE_OFF].is_null()) {
    TimeDelta screen_off_event_delta =
      current_time - idle_transition_timestamps_[BACKLIGHT_IDLE_OFF];
    SendMetricWithPowerState(kMetricIdleAfterScreenOffName,
                             screen_off_event_delta.InMilliseconds(),
                             kMetricIdleAfterScreenOffMin,
                             kMetricIdleAfterScreenOffMax,
                             kMetricIdleAfterScreenOffBuckets);
  }
  idle_transition_timestamps_.clear();
}

void Daemon::GenerateMetricsOnPowerEvent(const PowerStatus& info) {
  time_t now = time(NULL);
  GenerateBatteryDischargeRateMetric(info, now);
}

gboolean Daemon::GenerateBacklightLevelMetric() {
  double percent;
  if (backlight_controller_->GetPowerState() == BACKLIGHT_ACTIVE &&
      backlight_controller_->GetCurrentBrightnessPercent(&percent)) {
    SendEnumMetricWithPowerState(kMetricBacklightLevelName, lround(percent),
                                 kMetricBacklightLevelMax);
  }
  return true;
}

bool Daemon::GenerateBatteryDischargeRateMetric(const PowerStatus& info,
                                                time_t now) {
  // The battery discharge rate metric is relevant and collected only
  // when running on battery.
  if (plugged_state_ != kPowerDisconnected)
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

void Daemon::GenerateBatteryRemainingWhenChargeStartsMetric(
    const PluggedState& plugged_state,
    const PowerStatus& power_status) {
  // Need to make sure that we are actually charging a battery
  if (plugged_state != kPowerConnected)
    return;
  if (!power_status.battery_is_present)
    return;

  int charge = static_cast<int>(round(power_status.battery_percentage));
  LOG_IF(ERROR, !SendEnumMetric(kMetricBatteryRemainingWhenChargeStartsName,
                                charge,
                                kMetricBatteryRemainingWhenChargeStartsMax))
      << "Unable to send battery remaining when charge starts metric!";
}

void Daemon::GenerateEndOfSessionMetrics(const PowerStatus& info,
                                         const BacklightController& backlight,
                                         const base::Time& now,
                                         const base::Time& start) {
  LOG_IF(ERROR,
         (!GenerateBatteryRemainingAtEndOfSessionMetric(info)))
      << "Session Stopped : Unable to generate battery remaining metric!";
  LOG_IF(ERROR,
         (!GenerateNumberOfAlsAdjustmentsPerSessionMetric(
             backlight)))
      << "Session Stopped: Unable to generate ALS adjustments per session!";
  LOG_IF(ERROR,
         (!GenerateUserBrightnessAdjustmentsPerSessionMetric(
             backlight)))
      << "Session Stopped: Unable to generate user brightness adjustments per"
      << " session!";
  LOG_IF(ERROR,
         (!GenerateLengthOfSessionMetric(now, start)))
      << "Session Stopped: Unable to generate length of session metric!";
}

bool Daemon::GenerateBatteryRemainingAtEndOfSessionMetric(
    const PowerStatus& info) {
  int charge = static_cast<int>(round(info.battery_percentage));
  return SendEnumMetricWithPowerState(kMetricBatteryRemainingAtEndOfSessionName,
                                      charge,
                                      kMetricBatteryRemainingAtEndOfSessionMax);
}

bool Daemon::GenerateBatteryRemainingAtStartOfSessionMetric(
    const PowerStatus& info) {
  int charge = static_cast<int>(round(info.battery_percentage));
  return SendEnumMetricWithPowerState(
      kMetricBatteryRemainingAtStartOfSessionName,
      charge,
      kMetricBatteryRemainingAtStartOfSessionMax);
}

bool Daemon::GenerateNumberOfAlsAdjustmentsPerSessionMetric(
    const BacklightController& backlight) {
  int num_of_adjustments = backlight.GetNumAmbientLightSensorAdjustments();

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

bool Daemon::GenerateUserBrightnessAdjustmentsPerSessionMetric(
    const BacklightController& backlight) {
  int adjustment_count = backlight.GetNumUserAdjustments();

  if (adjustment_count < 0) {
    LOG(ERROR) << "Calculation for user brightness adjustments per session "
               << "returned a negative value";
    return false;
  } else if (adjustment_count > kMetricUserBrightnessAdjustmentsPerSessionMax) {
    adjustment_count = kMetricUserBrightnessAdjustmentsPerSessionMax;
  }

  return SendMetricWithPowerState(
      kMetricUserBrightnessAdjustmentsPerSessionName,
      adjustment_count,
      kMetricUserBrightnessAdjustmentsPerSessionMin,
      kMetricUserBrightnessAdjustmentsPerSessionMax,
      kMetricUserBrightnessAdjustmentsPerSessionBuckets);
}

bool Daemon::GenerateLengthOfSessionMetric(const base::Time& now,
                                           const base::Time& start) {
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

bool Daemon::GenerateNumOfSessionsPerChargeMetric(MetricsStore* store) {
  if (store == NULL) {
    LOG(ERROR) << "Attempted to generate NumOfSessionsPerCharge metric with "
               << "NULL MetricsStore";
    return false;
  }

  if (!store->IsInitialized()) {
    LOG(ERROR) << "Metrics store is not initialized, so could not generate "
               << "NumOfSessionsPerCharge metric";
    return false;
  }

  int sample = store->GetNumOfSessionsPerChargeMetric();
  if (sample == -1) {
    LOG(WARNING) << "Received -1 for NumOfSessionsPerCharge from MetricsStore";
    return false;
  }

  if (sample == 0) {
    LOG(INFO) << "A spurious call to GenerateNumOfSessionsPerChargeMetric has "
              << "occurred or we changed state at the login screen";
    return true;
  }

  if (sample > kMetricNumOfSessionsPerChargeMax) {
    LOG(INFO) << "Clamping NumberOfSessionsPerCharge to "
              << kMetricNumOfSessionsPerChargeMax;
    sample = kMetricNumOfSessionsPerChargeMax;
  }

  store->ResetNumOfSessionsPerChargeMetric();
  return SendMetric(kMetricNumOfSessionsPerChargeName,
                    sample,
                    kMetricNumOfSessionsPerChargeMin,
                    kMetricNumOfSessionsPerChargeMax,
                    kMetricNumOfSessionsPerChargeBuckets);
}

void Daemon::HandleNumOfSessionsPerChargeOnSetPlugged(
    MetricsStore* metrics_store,
    const PluggedState& plugged_state) {
  CHECK(metrics_store != NULL);
  if (plugged_state == kPowerConnected) {
    LOG_IF(ERROR, !GenerateNumOfSessionsPerChargeMetric(metrics_store))
        << "Failed to send NumOfSessionsPerCharge metrics";
  } else if (plugged_state == kPowerDisconnected) {
    int count = metrics_store->GetNumOfSessionsPerChargeMetric();
    switch (count) {
      case 1:
        break;
      case 0:
        metrics_store->IncrementNumOfSessionsPerChargeMetric();
        break;
      default:
        LOG(ERROR) << "NumOfSessionPerCharge counter was in a weird state with "
                   << "value " << count;
        metrics_store->ResetNumOfSessionsPerChargeMetric();
        metrics_store->IncrementNumOfSessionsPerChargeMetric();
        break;
    }
  }
}

bool Daemon::SendMetric(const std::string& name, int sample,
                        int min, int max, int nbuckets) {
  DLOG(INFO) << "Sending metric: " << name << " " << sample << " "
             << min << " " << max << " " << nbuckets;
  return metrics_lib_->SendToUMA(name, sample, min, max, nbuckets);
}

bool Daemon::SendEnumMetric(const std::string& name, int sample, int max) {
  DLOG(INFO) << "Sending enum metric: " << name << " " << sample << " " << max;
  return metrics_lib_->SendEnumToUMA(name, sample, max);
}

bool Daemon::SendMetricWithPowerState(const std::string& name, int sample,
                                      int min, int max, int nbuckets) {
  std::string name_with_power_state = name;
  if (plugged_state_ == kPowerDisconnected) {
    name_with_power_state += "OnBattery";
  } else if (plugged_state_ == kPowerConnected) {
    name_with_power_state += "OnAC";
  } else {
    return false;
  }
  return SendMetric(name_with_power_state, sample, min, max, nbuckets);
}

bool Daemon::SendEnumMetricWithPowerState(const std::string& name, int sample,
                                          int max) {
  std::string name_with_power_state = name;
  if (plugged_state_ == kPowerDisconnected) {
    name_with_power_state += "OnBattery";
  } else if (plugged_state_ == kPowerConnected) {
    name_with_power_state += "OnAC";
  } else {
    return false;
  }
  return SendEnumMetric(name_with_power_state, sample, max);
}

void Daemon::SendThermalMetrics(unsigned int aborted, unsigned int turned_on,
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

gboolean Daemon::GenerateThermalMetrics() {
  unsigned int aborted, turned_on, multiple;

  if (!util::GetUintFromFile(kMetricThermalAbortedFanFilename, &aborted) ||
      !util::GetUintFromFile(kMetricThermalTurnedOnFanFilename, &turned_on) ||
      !util::GetUintFromFile(kMetricThermalMultipleFanFilename, &multiple)) {
    LOG(ERROR) << "Unable to read values from debugfs thermal files. "
               << "UMA metrics not being sent this poll period.";
    return true;
  }

  SendThermalMetrics(aborted, turned_on, multiple);
  return true;
}

}  // namespace power_manager
