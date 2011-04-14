// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_DAEMON_H_
#define POWER_DAEMON_H_

#include <dbus/dbus-glib-lowlevel.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST

#include <string>

#include <ctime>

#include "base/file_path.h"
#include "base/scoped_ptr.h"
#include "base/time.h"
#include "cros/chromeos_power.h"
#include "metrics/metrics_library.h"
#include "power_manager/backlight_controller.h"
#include "power_manager/file_tagger.h"
#include "power_manager/inotify.h"
#include "power_manager/power_prefs.h"
#include "power_manager/screen_locker.h"
#include "power_manager/suspender.h"
#include "power_manager/video_detector.h"
#include "power_manager/xidle.h"
#include "power_manager/xidle_monitor.h"

namespace power_manager {

class PowerButtonHandler;

class Daemon : public XIdleMonitor {
 public:
  Daemon(BacklightController* ctl, PowerPrefs* prefs,
         MetricsLibraryInterface* metrics_lib,
         VideoDetectorInterface* video_detector,
         const FilePath& run_dir);
  ~Daemon();

  ScreenLocker* locker() { return &locker_; }
  BacklightController* backlight_controller() { return backlight_controller_; }

  const std::string& current_user() const { return current_user_; }

  void Init();
  void Run();
  void SetActive();
  void OnIdleEvent(bool is_idle, int64 idle_time_ms);
  void SetPlugged(bool plugged);

  void OnRequestRestart(bool notify_window_manager);
  void OnRequestShutdown(bool notify_window_manager);

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
  FRIEND_TEST(DaemonTest, GenerateMetricsOnPowerEvent);
  FRIEND_TEST(DaemonTest, SendEnumMetric);
  FRIEND_TEST(DaemonTest, SendMetric);
  FRIEND_TEST(DaemonTest, SendMetricWithPowerState);

  enum IdleState { kIdleUnknown, kIdleNormal, kIdleDim, kIdleScreenOff,
                   kIdleSuspend };

  enum ShutdownState { kShutdownNone, kShutdownRestarting,
                       kShutdownPowerOff };

  // UMA metrics parameters.
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

  // Read settings from disk
  void ReadSettings();

  // Read lock screen settings
  void ReadLockScreenSettings();

  // Initialize metrics
  void MetricInit();

  // Update our idle state based on the provided |idle_time_ms|
  void SetIdleState(int64 idle_time_ms);

  // Setup idle timers, adding the provided offset to all timeouts
  // starting with the state provided except the locking timeout.
  void SetIdleOffset(int64 offset_ms, IdleState state);

  static void OnPowerEvent(void* object, const chromeos::PowerStatus& info);

  static GdkFilterReturn gdk_event_filter(GdkXEvent* gxevent,
    GdkEvent* gevent, gpointer data);

  // Tell X we are interested in the specified key/mask combination.
  // Capslock and Numlock are always ignored.
  void GrabKey(KeyCode key, uint32 mask);

  // Standard handler for dbus messages. |data| contains a pointer to a
  // Daemon object.
  static DBusHandlerResult DBusMessageHandler(DBusConnection*,
                                              DBusMessage* message,
                                              void* data);

  // Register the dbus message handler with appropriate dbus events.
  void RegisterDBusMessageHandler();

  // Check for extremely low battery condition.
  void OnLowBattery(double battery_percentage);

  // Timeout handler for clean shutdown. If we don't hear back from
  // clean shutdown because the stopping is taking too long or hung,
  // go through with the shutdown now.
  SIGNAL_CALLBACK_0(Daemon, gboolean, CleanShutdownTimedOut);

  // Handle power state changes from powerd_suspend.
  // |state| is "on" when resuming from suspend.
  void OnPowerStateChange(const char* state);

  // Handle a D-Bus signal from the session manager telling us that the
  // session state has changed.
  void OnSessionStateChange(const char* state, const char* user);

  void StartCleanShutdown();
  void Shutdown();
  void Suspend();

  // Callback for Inotify of Preference directory changes.
  static gboolean PrefChangeHandler(const char* name, int wd,
                                    unsigned int mask,
                                    gpointer data);

  // Send a D-Bus signal announcing that the screen brightness has been set to
  // its current level, given as a percentage between 0 and 100.
  // |user_initiated| specifies whether this brightness change was requested by
  // the user (i.e. brightness keys) instead of being automatic (e.g. idle, AC
  // plugged or unplugged, etc.).
  void SendBrightnessChangedSignal(bool user_initiated);

  // Generates UMA metrics on every idle event.
  void GenerateMetricsOnIdleEvent(bool is_idle, int64 idle_time_ms);

  // Generates UMA metrics on every power event based on the current
  // power status.
  void GenerateMetricsOnPowerEvent(const chromeos::PowerStatus& info);

  // Generates UMA metrics about the current backlight level.
  // Always returns true.
  SIGNAL_CALLBACK_0(Daemon, gboolean, GenerateBacklightLevelMetric);

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

  // Called by dbus handler when resume signal is received
  void HandleResume();

  BacklightController* backlight_controller_;
  PowerPrefs* prefs_;
  MetricsLibraryInterface* metrics_lib_;
  VideoDetectorInterface* video_detector_;
  XIdle idle_;
  double low_battery_suspend_percent_;
  bool clean_shutdown_initiated_;
  bool low_battery_;
  int64 clean_shutdown_timeout_ms_;
  int64 plugged_dim_ms_;
  int64 plugged_off_ms_;
  int64 plugged_suspend_ms_;
  int64 unplugged_dim_ms_;
  int64 unplugged_off_ms_;
  int64 unplugged_suspend_ms_;
  int64 default_lock_ms_;
  int64 dim_ms_;
  int64 off_ms_;
  int64 suspend_ms_;
  int64 lock_ms_;
  int64 offset_ms_;
  int64 min_backlight_percent_;
  bool enforce_lock_;
  bool lock_on_idle_suspend_;
  bool use_xscreensaver_;
  PluggedState plugged_state_;
  IdleState idle_state_;
  FileTagger file_tagger_;
  ShutdownState shutdown_state_;
  ScreenLocker locker_;
  Suspender suspender_;
  FilePath run_dir_;
  scoped_ptr<PowerButtonHandler> power_button_handler_;

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

  // Key symbols
  KeyCode key_brightness_up_;
  KeyCode key_brightness_down_;
  KeyCode key_power_off_;
  KeyCode key_f6_;
  KeyCode key_f7_;

  // User whose session is currently active, or empty if no session is
  // active or we're in guest mode.
  std::string current_user_;
};

}  // namespace power_manager

#endif  // POWER_DAEMON_H_
