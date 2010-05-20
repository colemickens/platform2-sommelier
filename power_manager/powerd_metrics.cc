// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd.h"

#include <cmath>

#include "base/logging.h"

using base::TimeDelta;
using base::TimeTicks;

namespace power_manager {

const char Daemon::kMetricBatteryDischargeRateName[] =
    "Power.BatteryDischargeRate";  // mW
const int Daemon::kMetricBatteryDischargeRateMin = 1000;
const int Daemon::kMetricBatteryDischargeRateMax = 30000;
const int Daemon::kMetricBatteryDischargeRateBuckets = 50;
const time_t Daemon::kMetricBatteryDischargeRateInterval = 30;  // seconds
const char Daemon::kMetricBatteryRemainingChargeName[] =
    "Power.BatteryRemainingCharge";  // %
const int Daemon::kMetricBatteryRemainingChargeMax = 101;
const time_t Daemon::kMetricBatteryRemainingChargeInterval = 30;  // seconds
const char Daemon::kMetricBatteryTimeToEmptyName[] =
    "Power.BatteryTimeToEmpty";  // minutes
const int Daemon::kMetricBatteryTimeToEmptyMin = 1;
const int Daemon::kMetricBatteryTimeToEmptyMax = 1000;
const int Daemon::kMetricBatteryTimeToEmptyBuckets = 50;
const time_t Daemon::kMetricBatteryTimeToEmptyInterval = 30;  // seconds
const char Daemon::kMetricIdleName[] = "Power.IdleTime";  // ms
const int Daemon::kMetricIdleMin = 60 * 1000;
const int Daemon::kMetricIdleMax = 60 * 60 * 1000;
const int Daemon::kMetricIdleBuckets = 50;
const char Daemon::kMetricIdleAfterDimName[] = "Power.IdleTimeAfterDim";  // ms
const int Daemon::kMetricIdleAfterDimMin = 100;
const int Daemon::kMetricIdleAfterDimMax = 10 * 60 * 1000;
const int Daemon::kMetricIdleAfterDimBuckets = 50;
const char Daemon::kMetricIdleAfterScreenOffName[] =
    "Power.IdleTimeAfterScreenOff";  // ms
const int Daemon::kMetricIdleAfterScreenOffMin = 100;
const int Daemon::kMetricIdleAfterScreenOffMax = 10 * 60 * 1000;
const int Daemon::kMetricIdleAfterScreenOffBuckets = 50;

// Checks if |now| is the time to generate a new sample of a given
// metric. Returns true if the last metric sample was generated at
// least |interval| seconds ago (or if there is a clock jump back in
// time).
bool CheckMetricInterval(time_t now, time_t last, time_t interval) {
  return now < last || now - last >= interval;
}

void Daemon::GenerateMetricsOnIdleEvent(bool is_idle, int64 idle_time_ms) {
  if (is_idle) {
    last_idle_event_timestamp_ = TimeTicks::Now();
    last_idle_timedelta_ = TimeDelta::FromMilliseconds(idle_time_ms);
  } else if (!last_idle_event_timestamp_.is_null() &&
             idle_time_ms < last_idle_timedelta_.InMilliseconds()) {
    TimeDelta event_delta = TimeTicks::Now() - last_idle_event_timestamp_;
    TimeDelta total_delta = event_delta + last_idle_timedelta_;
    int64 total_delta_ms = total_delta.InMilliseconds();
    last_idle_event_timestamp_ = TimeTicks();
    if (total_delta_ms >= kMetricIdleMin || total_delta_ms >= dim_ms_) {
      SendMetricWithPowerState(kMetricIdleName, total_delta_ms, kMetricIdleMin,
          kMetricIdleMax, kMetricIdleBuckets);

      int64 event_delta_ms = event_delta.InMilliseconds();
      int64 last_idle_ms = last_idle_timedelta_.InMilliseconds();
      if (last_idle_ms >= off_ms_ && last_idle_ms < suspend_ms_) {
        SendMetricWithPowerState(kMetricIdleAfterScreenOffName, event_delta_ms,
            kMetricIdleAfterScreenOffMin, kMetricIdleAfterScreenOffMax,
            kMetricIdleAfterScreenOffBuckets);
      } else if (last_idle_ms >= dim_ms_ && last_idle_ms < off_ms_) {
        SendMetricWithPowerState(kMetricIdleAfterDimName, event_delta_ms,
            kMetricIdleAfterDimMin, kMetricIdleAfterDimMax,
            kMetricIdleAfterDimBuckets);
      }
    }
  }
}

void Daemon::GenerateMetricsOnPowerEvent(const chromeos::PowerStatus& info) {
  time_t now = time(NULL);
  GenerateBatteryDischargeRateMetric(info, now);
  GenerateBatteryRemainingChargeMetric(info, now);
  GenerateBatteryTimeToEmptyMetric(info, now);
}

bool Daemon::GenerateBatteryDischargeRateMetric(
    const chromeos::PowerStatus& info, time_t now) {
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

bool Daemon::GenerateBatteryRemainingChargeMetric(
    const chromeos::PowerStatus& info, time_t now) {
  // The remaining battery charge metric is relevant and collected
  // only when running on battery.
  if (plugged_state_ != kPowerDisconnected)
    return false;

  // Ensures that the metric is not generated too frequently.
  if (!CheckMetricInterval(now, battery_remaining_charge_metric_last_,
                           kMetricBatteryRemainingChargeInterval))
    return false;

  int charge = static_cast<int>(round(info.battery_percentage));
  if (!SendEnumMetric(kMetricBatteryRemainingChargeName, charge,
                      kMetricBatteryRemainingChargeMax))
    return false;

  battery_remaining_charge_metric_last_ = now;
  return true;
}

bool Daemon::GenerateBatteryTimeToEmptyMetric(
    const chromeos::PowerStatus& info, time_t now) {
  // The battery's remaining time to empty metric is relevant and
  // collected only when running on battery.
  if (plugged_state_ != kPowerDisconnected)
    return false;

  // Ensures that the metric is not generated too frequently.
  if (!CheckMetricInterval(now, battery_time_to_empty_metric_last_,
                           kMetricBatteryTimeToEmptyInterval))
    return false;

  // Converts the time to empty from seconds to minutes by rounding to
  // the nearest whole minute.
  int time = (info.battery_time_to_empty + 30) / 60;
  if (!SendMetric(kMetricBatteryTimeToEmptyName, time,
                  kMetricBatteryTimeToEmptyMin,
                  kMetricBatteryTimeToEmptyMax,
                  kMetricBatteryTimeToEmptyBuckets))
    return false;

  battery_time_to_empty_metric_last_ = now;
  return true;
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

}  // namespace power_manager
