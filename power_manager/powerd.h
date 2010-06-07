// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_DAEMON_H_
#define POWER_DAEMON_H_

#include <dbus/dbus-glib-lowlevel.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST

#include <ctime>

#include "base/scoped_ptr.h"
#include "base/time.h"
#include "cros/chromeos_power.h"
#include "metrics/metrics_library.h"
#include "power_manager/backlight.h"
#include "power_manager/backlight_controller.h"
#include "power_manager/power_prefs.h"
#include "power_manager/screen_locker.h"
#include "power_manager/suspender.h"
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
  FRIEND_TEST(DaemonTest, GenerateBacklightLevelMetric);
  FRIEND_TEST(DaemonTest, GenerateBatteryDischargeRateMetric);
  FRIEND_TEST(DaemonTest, GenerateBatteryDischargeRateMetricInterval);
  FRIEND_TEST(DaemonTest, GenerateBatteryDischargeRateMetricNotDisconnected);
  FRIEND_TEST(DaemonTest, GenerateBatteryDischargeRateMetricRateNonPositive);
  FRIEND_TEST(DaemonTest, GenerateBatteryRemainingChargeMetric);
  FRIEND_TEST(DaemonTest, GenerateBatteryRemainingChargeMetricInterval);
  FRIEND_TEST(DaemonTest, GenerateBatteryRemainingChargeMetricNotDisconnected);
  FRIEND_TEST(DaemonTest, GenerateBatteryTimeToEmptyMetric);
  FRIEND_TEST(DaemonTest, GenerateBatteryTimeToEmptyMetricInterval);
  FRIEND_TEST(DaemonTest, GenerateBatteryTimeToEmptyMetricNotDisconnected);
  FRIEND_TEST(DaemonTest, GenerateMetricsOnIdleEvent);
  FRIEND_TEST(DaemonTest, GenerateMetricsOnPowerEvent);
  FRIEND_TEST(DaemonTest, SendEnumMetric);
  FRIEND_TEST(DaemonTest, SendMetric);
  FRIEND_TEST(DaemonTest, SendMetricWithPowerState);

  enum PluggedState { kPowerDisconnected, kPowerConnected, kPowerUnknown };
  enum IdleState { kIdleUnknown, kIdleNormal, kIdleDim, kIdleScreenOff,
                   kIdleSuspend };

  // UMA metrics parameters.
  static const char kMetricBacklightLevelName[];
  static const int kMetricBacklightLevelMax;
  static const time_t kMetricBacklightLevelInterval;
  static const char kMetricBatteryDischargeRateName[];
  static const int kMetricBatteryDischargeRateMin;
  static const int kMetricBatteryDischargeRateMax;
  static const int kMetricBatteryDischargeRateBuckets;
  static const time_t kMetricBatteryDischargeRateInterval;
  static const char kMetricBatteryRemainingChargeName[];
  static const int kMetricBatteryRemainingChargeMax;
  static const time_t kMetricBatteryRemainingChargeInterval;
  static const char kMetricBatteryTimeToEmptyName[];
  static const int kMetricBatteryTimeToEmptyMin;
  static const int kMetricBatteryTimeToEmptyMax;
  static const int kMetricBatteryTimeToEmptyBuckets;
  static const time_t kMetricBatteryTimeToEmptyInterval;
  static const char kMetricIdleName[];
  static const int kMetricIdleMin;
  static const int kMetricIdleMax;
  static const int kMetricIdleBuckets;
  static const char kMetricIdleAfterDimName[];
  static const int kMetricIdleAfterDimMin;
  static const int kMetricIdleAfterDimMax;
  static const int kMetricIdleAfterDimBuckets;
  static const char kMetricIdleAfterScreenOffName[];
  static const int kMetricIdleAfterScreenOffMin;
  static const int kMetricIdleAfterScreenOffMax;
  static const int kMetricIdleAfterScreenOffBuckets;

  // Read settings from disk
  void ReadSettings();

  // Initialize metrics
  void MetricInit();

  // Update our idle state based on the provided |idle_time_ms|
  void SetIdleState(int64 idle_time_ms);

  // Setup idle timers, adding the provided offset to all timeouts
  // except the locking timeout.
  void SetIdleOffset(int64 offset_ms);

  static void OnPowerEvent(void* object, const chromeos::PowerStatus& info);

  static GdkFilterReturn gdk_event_filter(GdkXEvent* gxevent,
    GdkEvent* gevent, gpointer data);

  // Standard handler for dbus messages. |data| contains a pointer to a
  // Daemon object.
  static DBusHandlerResult DBusMessageHandler(DBusConnection*,
                                              DBusMessage* message,
                                              void* data);

  // Register the dbus message handler with appropriate dbus events.
  void RegisterDBusMessageHandler();

  // Generates UMA metrics on every idle event.
  void GenerateMetricsOnIdleEvent(bool is_idle, int64 idle_time_ms);

  // Generates UMA metrics on every power event based on the current
  // power status.
  void GenerateMetricsOnPowerEvent(const chromeos::PowerStatus& info);

  // Generates UMA metrics about the current backlight level.
  // Always returns true.
  static gboolean GenerateBacklightLevelMetric(gpointer data);

  // Generates a battery discharge rate UMA metric sample. Returns
  // true if a sample was sent to UMA, false otherwise.
  bool GenerateBatteryDischargeRateMetric(const chromeos::PowerStatus& info,
                                          time_t now);

  // Generates a remaining battery charge UMA metric sample. Returns
  // true if a sample was sent to UMA, false otherwise.
  bool GenerateBatteryRemainingChargeMetric(const chromeos::PowerStatus& info,
                                            time_t now);

  // Generates the battery's remaining time to empty UMA metric
  // sample. Returns true if a sample was sent to UMA, false
  // otherwise.
  bool GenerateBatteryTimeToEmptyMetric(const chromeos::PowerStatus& info,
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

  // Sends a regular (exponential) histogram sample to Chrome for
  // transport to UMA. Appends the current power state to the name of the
  // metric. Returns true on success. See MetricsLibrary::SendToUMA in
  // metrics/metrics_library.h for a description of the arguments.
  bool SendMetricWithPowerState(const std::string& name, int sample,
                                int min, int max, int nbuckets);

  // Sends an enumeration (linear) histogram sample to Chrome for
  // transport to UMA. Appends the current power state to the name of the
  // metric. Returns true on success. See MetricsLibrary::SendEnumToUMA in
  // metrics/metrics_library.h for a description of the arguments.
  bool SendEnumMetricWithPowerState(const std::string& name, int sample,
                                    int max);

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
  int64 lock_ms_;
  int64 offset_ms_;
  bool use_xscreensaver_;
  PluggedState plugged_state_;
  IdleState idle_state_;
  ScreenLocker locker_;
  Suspender suspender_;

  // Timestamp the last generated battery discharge rate metric.
  time_t battery_discharge_rate_metric_last_;

  // Timestamp the last generated remaining battery charge metric.
  time_t battery_remaining_charge_metric_last_;

  // Timestamp the last generated battery's remaining time to empty
  // metric.
  time_t battery_time_to_empty_metric_last_;

  // Timestamp of the last idle event.
  base::TimeTicks last_idle_event_timestamp_;

  // Idle time as of last idle event.
  base::TimeDelta last_idle_timedelta_;

  // Key symbols for brightness up and down
  KeyCode key_brightness_up_;
  KeyCode key_brightness_down_;
};

}  // namespace power_manager

#endif  // POWER_DAEMON_H_
