// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWERD_H_
#define POWER_MANAGER_POWERD_H_
#pragma once

#include <dbus/dbus-glib-lowlevel.h>
#include <glib.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST
#include <libudev.h>

#include <map>
#include <string>

#include <ctime>

#include "base/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/time.h"
#include "chromeos/dbus/dbus.h"
#include "chromeos/glib/object.h"
#include "cros/chromeos_power.h"
#include "metrics/metrics_library.h"
#include "power_manager/backlight_controller.h"
#include "power_manager/file_tagger.h"
#include "power_manager/inotify.h"
#include "power_manager/power_prefs.h"
#include "power_manager/power_supply.h"
#include "power_manager/screen_locker.h"
#include "power_manager/signal_callback.h"
#include "power_manager/suspender.h"
#include "power_manager/xidle.h"
#include "power_manager/xidle_observer.h"

namespace power_manager {

class AudioDetectorInterface;
class MonitorReconfigure;
class PowerButtonHandler;
class VideoDetectorInterface;

class Daemon : public XIdleObserver {
 public:
  Daemon(BacklightController* ctl,
         PowerPrefs* prefs,
         MetricsLibraryInterface* metrics_lib,
         VideoDetectorInterface* video_detector,
         AudioDetectorInterface* audio_detector,
         MonitorReconfigure* monitor_reconfigure,
         const FilePath& run_dir);
  ~Daemon();

  ScreenLocker* locker() { return &locker_; }
  BacklightController* backlight_controller() { return backlight_controller_; }

  const std::string& current_user() const { return current_user_; }

  void Init();
  void Run();
  void SetActive();
  void SetPlugged(bool plugged);

  void OnRequestRestart(bool notify_window_manager);
  void OnRequestShutdown(bool notify_window_manager);

  // Decreases the screen brightness and announces the change as a D-Bus signal.
  //
  // If |allow_off| is FALSE, the backlight will never be entirely turned off.
  // This should be used with on-screen controls to prevent their becoming
  // impossible for the user to see.
  void DecreaseScreenBrightness(bool allow_off, bool user_initiated);

  // Increases the screen brightness and announces the change as a D-Bus signal.
  void IncreaseScreenBrightness(bool user_initiated);

  // Set idle_time_ms to how long the user has been idle, in milliseconds.
  // On success, return true; otherwise return false. Used in idle API on
  // chrome side.
  bool GetIdleTime(int64* idle_time_ms);

  // If in the active-but-off state, turn up the brightness when user presses a
  // key so user can see that the screen has been locked.
  void BrightenScreenIfOff();

  // Overridden from XIdleObserver:
  virtual void OnIdleEvent(bool is_idle, int64 idle_time_ms);

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

  // Reads settings from disk
  void ReadSettings();

  // Reads lock screen settings
  void ReadLockScreenSettings();

  // Initializes metrics
  void MetricInit();

  // Updates our idle state based on the provided |idle_time_ms|
  void SetIdleState(int64 idle_time_ms);

  // Sets up idle timers, adding the provided offset to all timeouts
  // starting with the state provided except the locking timeout.
  void SetIdleOffset(int64 offset_ms, IdleState state);

  static void OnPowerEvent(void* object, const chromeos::PowerStatus& info);

  SIGNAL_CALLBACK_2(Daemon, GdkFilterReturn, GdkEventFilter, GdkXEvent*,
                    GdkEvent*);

  // Tells X we are interested in the specified key/mask combination.
  // Capslock and Numlock are always ignored.
  void GrabKey(KeyCode key, uint32 mask);

  // Handles power supply udev events.
  static gboolean UdevEventHandler(GIOChannel* source,
                                   GIOCondition condition,
                                   gpointer data);

  // Registers udev event handler with GIO.
  void RegisterUdevEventHandler();

  // Standard handler for dbus messages. |data| contains a pointer to a
  // Daemon object.
  static DBusHandlerResult DBusMessageHandler(DBusConnection*,
                                              DBusMessage* message,
                                              void* data);

  // Registers the dbus message handler with appropriate dbus events.
  void RegisterDBusMessageHandler();

  // Reads power supply status at regular intervals, and sends a signal to
  // indicate that fresh power supply data is available.
  SIGNAL_CALLBACK_0(Daemon, gboolean, PollPowerSupply);

  // Checks for extremely low battery condition.
  void OnLowBattery(double battery_percentage);

  // Timeout handler for clean shutdown. If we don't hear back from
  // clean shutdown because the stopping is taking too long or hung,
  // go through with the shutdown now.
  SIGNAL_CALLBACK_0(Daemon, gboolean, CleanShutdownTimedOut);

  // Handles power state changes from powerd_suspend.
  // |state| is "on" when resuming from suspend.
  void OnPowerStateChange(const char* state);

  // Handles information from the session manager about the session state.
  // Invoked by RetrieveSessionState() and also in response to
  // SessionStateChanged D-Bus signals.
  void OnSessionStateChange(const char* state, const char* user);

  void StartCleanShutdown();
  void Shutdown();
  void Suspend();

  // Callback for Inotify of Preference directory changes.
  static gboolean PrefChangeHandler(const char* name, int wd,
                                    unsigned int mask,
                                    gpointer data);

  // Sends a D-Bus signal announcing that the screen brightness has been set to
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

  // Sends a synchronous D-Bus request to the session manager to retrieve the
  // session state and updates |current_user_| based on the response.
  void RetrieveSessionState();

  // Sets idle timeouts based on whether the system is projecting to an
  // external display.
  SIGNAL_CALLBACK_0(Daemon, void, AdjustIdleTimeoutsForProjection);

  BacklightController* backlight_controller_;
  PowerPrefs* prefs_;
  MetricsLibraryInterface* metrics_lib_;
  VideoDetectorInterface* video_detector_;
  AudioDetectorInterface* audio_detector_;
  XIdle idle_;
  MonitorReconfigure* monitor_reconfigure_;
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
  int64 react_ms_;
  int64 fuzz_ms_;
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
  FileTagger file_tagger_;
  ShutdownState shutdown_state_;
  ScreenLocker locker_;
  Suspender suspender_;
  FilePath run_dir_;
  PowerSupply power_supply_;
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
  KeyCode key_f6_;
  KeyCode key_f7_;

  // User whose session is currently active, or empty if no session is
  // active or we're in guest mode.
  std::string current_user_;

  // Stores normal timeout values, to be used for switching between projecting
  // and non-projecting timeouts.  Map keys are variable names found in
  // power_constants.h.
  std::map<std::string, int64> base_timeout_values_;

  // Keep a local copy of power status reading from power_supply.  This way,
  // requests for each field of the power status can be read directly from
  // this struct.  Otherwise we'd have to read the whole struct from
  // power_supply since it doesn't support reading individual fields.
  chromeos::PowerStatus power_status_;

  // For listening to udev events.
  struct udev_monitor* udev_monitor_;
  struct udev* udev_;
};

}  // namespace power_manager

#endif  // POWER_MANAGER_POWERD_H_
