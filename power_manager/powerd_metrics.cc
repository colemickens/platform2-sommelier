// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd.h"

#include <cmath>

#include "base/logging.h"

namespace power_manager {

const char Daemon::kMetricBatteryDischargeRateName[] =
    "Power.BatteryDischargeRate";  // mW
const int Daemon::kMetricBatteryDischargeRateMin = 1000;
const int Daemon::kMetricBatteryDischargeRateMax = 30000;
const int Daemon::kMetricBatteryDischargeRateBuckets = 50;
const char Daemon::kMetricBatteryRemainingChargeName[] =
    "Power.BatteryRemainingCharge";  // %
const int Daemon::kMetricBatteryRemainingChargeMax = 101;

void Daemon::GenerateMetricsOnPowerEvent(const chromeos::PowerStatus& info) {
  time_t now = time(NULL);
  GenerateBatteryDischargeRateMetric(info, now);
  GenerateBatteryRemainingChargeMetric(info, now);
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
  const int kMinMetricInterval = 30;  // seconds
  if (now >= battery_discharge_rate_metric_last_ &&
      now - battery_discharge_rate_metric_last_ < kMinMetricInterval)
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
  const int kMinMetricInterval = 30;  // seconds
  if (now >= battery_remaining_charge_metric_last_ &&
      now - battery_remaining_charge_metric_last_ < kMinMetricInterval)
    return false;

  int charge = static_cast<int>(round(info.battery_percentage));
  if (!SendEnumMetric(kMetricBatteryRemainingChargeName, charge,
                      kMetricBatteryRemainingChargeMax))
    return false;

  battery_remaining_charge_metric_last_ = now;
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

}  // namespace power_manager
