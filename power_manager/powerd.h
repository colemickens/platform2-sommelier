// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_DAEMON_H_
#define POWER_DAEMON_H_

#include <gtest/gtest_prod.h>  // for FRIEND_TEST

#include <ctime>

#include "cros/chromeos_power.h"
#include "metrics/metrics_library.h"
#include "power_manager/backlight.h"
#include "power_manager/backlight_controller.h"
#include "power_manager/power_prefs.h"
#include "power_manager/xidle.h"

namespace power_manager {

class Daemon : public XIdleMonitor {
 public:
  Daemon(BacklightController* ctl, PowerPrefs* prefs,
         MetricsLibraryInterface* metrics_lib);

  void Init();
  void Run();
  void OnIdleEvent(bool is_idle, int64 idle_time_ms);
  void SetPlugged(bool plugged);

 private:
  friend class DaemonTest;
  FRIEND_TEST(DaemonTest, GenerateBatteryDischargeRateMetric);
  FRIEND_TEST(DaemonTest, GenerateBatteryDischargeRateMetricBackInTime);
  FRIEND_TEST(DaemonTest, GenerateBatteryDischargeRateMetricInterval);
  FRIEND_TEST(DaemonTest, GenerateBatteryDischargeRateMetricNotDisconnected);
  FRIEND_TEST(DaemonTest, GenerateBatteryDischargeRateMetricRateNonPositive);
  FRIEND_TEST(DaemonTest, GenerateBatteryRemainingChargeMetric);
  FRIEND_TEST(DaemonTest, GenerateBatteryRemainingChargeMetricBackInTime);
  FRIEND_TEST(DaemonTest, GenerateBatteryRemainingChargeMetricInterval);
  FRIEND_TEST(DaemonTest, GenerateBatteryRemainingChargeMetricNotDisconnected);
  FRIEND_TEST(DaemonTest, GenerateMetricsOnPowerEvent);
  FRIEND_TEST(DaemonTest, SendEnumMetric);
  FRIEND_TEST(DaemonTest, SendMetric);

  enum PluggedState { kPowerDisconnected, kPowerConnected, kPowerUnknown };

  // UMA metrics parameters.
  static const char kMetricBatteryDischargeRateName[];
  static const int kMetricBatteryDischargeRateMin;
  static const int kMetricBatteryDischargeRateMax;
  static const int kMetricBatteryDischargeRateBuckets;
  static const char kMetricBatteryRemainingChargeName[];
  static const int kMetricBatteryRemainingChargeMax;

  static void OnPowerEvent(void* object, const chromeos::PowerStatus& info);

  // Generates UMA metrics on every power event based on the current
  // power status.
  void GenerateMetricsOnPowerEvent(const chromeos::PowerStatus& info);

  // Generates a battery discharge rate UMA metric sample. Returns
  // true if a sample was sent to UMA, false otherwise.
  bool GenerateBatteryDischargeRateMetric(const chromeos::PowerStatus& info,
                                          time_t now);

  // Generates a remaining battery charge UMA metric sample. Returns
  // true if a sample was sent to UMA, false otherwise.
  bool GenerateBatteryRemainingChargeMetric(const chromeos::PowerStatus& info,
                                            time_t now);

  // Sends a regular (exponential) histogram sample to Chrome for
  // transport to UMA. Returns true on success. See
  // MetricsLibrary::SendToUMA in metrics/metrics_library.h for a
  // description of the arguments.
  bool SendMetric(const std::string& name, int sample,
                  int min, int max, int nbuckets);

  // Sends an enumeration (linear) histogram sample to Chrome for
  // transport to UMA. Returns true on success. See
  // MetricsLibrary::SendEnumToUMA in metrics/metrics_library.h for a
  // description of the arguments.
  bool SendEnumMetric(const std::string& name, int sample, int max);

  BacklightController* ctl_;
  PowerPrefs* prefs_;
  MetricsLibraryInterface* metrics_lib_;
  XIdle idle_;
  int64 plugged_dim_ms_;
  int64 plugged_off_ms_;
  int64 plugged_suspend_ms_;
  int64 unplugged_dim_ms_;
  int64 unplugged_off_ms_;
  int64 unplugged_suspend_ms_;
  int64 dim_ms_;
  int64 off_ms_;
  int64 suspend_ms_;
  PluggedState plugged_state_;

  // Timestamp the last generated battery discharge rate metric.
  time_t battery_discharge_rate_metric_last_;

  // Timestamp the last generated remaining battery charge metric.
  time_t battery_remaining_charge_metric_last_;
};

}  // namespace power_manager

#endif  // POWER_DAEMON_H_
