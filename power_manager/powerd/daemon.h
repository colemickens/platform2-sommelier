// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWERD_DAEMON_H_
#define POWER_MANAGER_POWERD_DAEMON_H_
#pragma once

#include <dbus/dbus-glib-lowlevel.h>
#include <glib.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST

#include <map>
#include <string>
#include <utility>
#include <vector>

#include <ctime>

#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/time.h"
#include "metrics/metrics_library.h"
#include "power_manager/common/dbus_handler.h"
#include "power_manager/common/inotify.h"
#include "power_manager/common/prefs_observer.h"
#include "power_manager/common/signal_callback.h"
#include "power_manager/powerd/file_tagger.h"
#include "power_manager/powerd/policy/backlight_controller.h"
#include "power_manager/powerd/policy/backlight_controller_observer.h"
#include "power_manager/powerd/policy/dark_resume_policy.h"
#include "power_manager/powerd/policy/input_controller.h"
#include "power_manager/powerd/policy/keyboard_backlight_controller.h"
#include "power_manager/powerd/policy/suspender.h"
#include "power_manager/powerd/system/audio_observer.h"
#include "power_manager/powerd/system/peripheral_battery_watcher.h"
#include "power_manager/powerd/system/power_supply.h"
#include "power_manager/powerd/system/power_supply_observer.h"

// Forward declarations of structs from libudev.h.
struct udev;
struct udev_monitor;

namespace power_manager {

class DBusSenderInterface;
class Prefs;

namespace policy {
class StateController;
}  // namespace policy

namespace system {
class AudioClient;
class Input;
}  // namespace system

enum PluggedState {
  PLUGGED_STATE_DISCONNECTED,
  PLUGGED_STATE_CONNECTED,
  PLUGGED_STATE_UNKNOWN,
};

// Main class within the powerd daemon that ties all other classes together.
class Daemon : public policy::BacklightControllerObserver,
               public policy::InputController::Delegate,
               public system::AudioObserver,
               public system::PowerSupplyObserver {
 public:
  // Ownership of pointers remains with the caller.  |backlight_controller|
  // and |keyboard_controller| may be NULL.
  Daemon(PrefsInterface* prefs,
         MetricsLibraryInterface* metrics_lib,
         policy::BacklightController* backlight_controller,
         policy::KeyboardBacklightController* keyboard_controller,
         const base::FilePath& run_dir);
  ~Daemon();

  void Init();
  void Run();
  void SetPlugged(bool plugged);

  void OnRequestRestart();
  void OnRequestShutdown();

  // Shuts the system down in response to a failed suspend attempt.
  void ShutdownForFailedSuspend();

  // Notify chrome that an idle event happened.
  void IdleEventNotify(int64 threshold);

  // Overridden from policy::BacklightControllerObserver:
  virtual void OnBrightnessChanged(
      double brightness_percent,
      policy::BacklightController::BrightnessChangeCause cause,
      policy::BacklightController* source) OVERRIDE;

  // Called by |suspender_| before other processes are informed that the
  // system will be suspending soon.
  void PrepareForSuspendAnnouncement();

  // Called by |suspender_| if a suspend request is aborted before
  // PrepareForSuspend() has been called.
  void HandleCanceledSuspendAnnouncement();

  // Called by |suspender_| just before a suspend attempt begins.
  void PrepareForSuspend();

  // Called by |suspender_| after the completion of a suspend/resume cycle
  // (which did not necessarily succeed).
  void HandleResume(bool suspend_was_successful,
                    int num_suspend_retries,
                    int max_suspend_retries);

  // Sends a metric describing a suspend attempt that didn't succeed on its
  // first attempt.  Doesn't send anything if |num_retries| is 0.
  void GenerateRetrySuspendMetric(int num_retries, int max_retries);

  // Overridden from policy::InputController::Delegate:
  virtual void HandleLidClosed() OVERRIDE;
  virtual void HandleLidOpened() OVERRIDE;
  virtual void SendPowerButtonMetric(bool down, base::TimeTicks timestamp)
      OVERRIDE;

  // Overridden from system::AudioObserver:
  virtual void OnAudioActivity(base::TimeTicks last_audio_time) OVERRIDE;

  // Overridden from system::PowerSupplyObserver:
  virtual void OnPowerStatusUpdate(const system::PowerStatus& status) OVERRIDE;

 private:
  friend class DaemonTest;
  FRIEND_TEST(DaemonTest, GenerateBacklightLevelMetric);
  FRIEND_TEST(DaemonTest, GenerateBatteryDischargeRateMetric);
  FRIEND_TEST(DaemonTest, GenerateBatteryDischargeRateMetricInterval);
  FRIEND_TEST(DaemonTest, GenerateBatteryDischargeRateMetricNotDisconnected);
  FRIEND_TEST(DaemonTest, GenerateBatteryDischargeRateMetricRateNonPositive);
  FRIEND_TEST(DaemonTest, GenerateBatteryRemainingAtEndOfSessionMetric);
  FRIEND_TEST(DaemonTest, GenerateBatteryRemainingAtStartOfSessionMetric);
  FRIEND_TEST(DaemonTest, GenerateBatteryInfoWhenChargeStartsMetric);
  FRIEND_TEST(DaemonTest, GenerateEndOfSessionMetrics);
  FRIEND_TEST(DaemonTest, GenerateLengthOfSessionMetric);
  FRIEND_TEST(DaemonTest, GenerateLengthOfSessionMetricOverflow);
  FRIEND_TEST(DaemonTest, GenerateLengthOfSessionMetricUnderflow);
  FRIEND_TEST(DaemonTest, GenerateMetricsOnPowerEvent);
  FRIEND_TEST(DaemonTest, GenerateNumOfSessionsPerChargeMetric);
  FRIEND_TEST(DaemonTest, GenerateNumberOfAlsAdjustmentsPerSessionMetric);
  FRIEND_TEST(DaemonTest,
              GenerateNumberOfAlsAdjustmentsPerSessionMetricOverflow);
  FRIEND_TEST(DaemonTest,
              GenerateNumberOfAlsAdjustmentsPerSessionMetricUnderflow);
  FRIEND_TEST(DaemonTest, GenerateUserBrightnessAdjustmentsPerSessionMetric);
  FRIEND_TEST(DaemonTest,
              GenerateUserBrightnessAdjustmentsPerSessionMetricOverflow);
  FRIEND_TEST(DaemonTest,
              GenerateUserBrightnessAdjustmentsPerSessionMetricUnderflow);
  FRIEND_TEST(DaemonTest, HandleNumOfSessionsPerChargeOnSetPlugged);
  FRIEND_TEST(DaemonTest, PowerButtonDownMetric);
  FRIEND_TEST(DaemonTest, SendEnumMetric);
  FRIEND_TEST(DaemonTest, SendMetric);
  FRIEND_TEST(DaemonTest, SendMetricWithPowerState);
  FRIEND_TEST(DaemonTest, SendThermalMetrics);

  enum ShutdownState {
    SHUTDOWN_STATE_NONE,
    SHUTDOWN_STATE_RESTARTING,
    SHUTDOWN_STATE_POWER_OFF
  };

  class StateControllerDelegate;
  class SuspenderDelegate;

  // Initializes metrics
  void MetricInit();

  // Decrease / increase the keyboard brightness; direction should be +1 for
  // increase and -1 for decrease.
  void AdjustKeyboardBrightness(int direction);

  // Shared code between keyboard and screen brightness changed handling
  void SendBrightnessChangedSignal(
      double brightness_percent,
      policy::BacklightController::BrightnessChangeCause cause,
      const std::string& signal_name);

  // Handles power supply udev events.
  static gboolean UdevEventHandler(GIOChannel*,
                                   GIOCondition condition,
                                   gpointer data);

  // Registers udev event handler with GIO.
  void RegisterUdevEventHandler();

  // Registers the dbus message handler with appropriate dbus events.
  void RegisterDBusMessageHandler();

  // Handles changes to D-Bus name ownership.
  void HandleDBusNameOwnerChanged(const std::string& name,
                                  const std::string& old_owner,
                                  const std::string& new_owner);

  // Callbacks for handling dbus messages.
  bool HandleCleanShutdownSignal(DBusMessage* message);
  bool HandleSessionStateChangedSignal(DBusMessage* message);
  bool HandleUpdateEngineStatusUpdateSignal(DBusMessage* message);
  DBusMessage* HandleRequestShutdownMethod(DBusMessage* message);
  DBusMessage* HandleRequestRestartMethod(DBusMessage* message);
  DBusMessage* HandleRequestSuspendMethod(DBusMessage* message);
  DBusMessage* HandleDecreaseScreenBrightnessMethod(DBusMessage* message);
  DBusMessage* HandleIncreaseScreenBrightnessMethod(DBusMessage* message);
  DBusMessage* HandleGetScreenBrightnessMethod(DBusMessage* message);
  DBusMessage* HandleSetScreenBrightnessMethod(DBusMessage* message);
  DBusMessage* HandleDecreaseKeyboardBrightnessMethod(DBusMessage* message);
  DBusMessage* HandleIncreaseKeyboardBrightnessMethod(DBusMessage* message);
  DBusMessage* HandleRequestIdleNotificationMethod(DBusMessage* message);
  DBusMessage* HandleGetPowerSupplyPropertiesMethod(DBusMessage* message);
  DBusMessage* HandleVideoActivityMethod(DBusMessage* message);
  DBusMessage* HandleUserActivityMethod(DBusMessage* message);
  DBusMessage* HandleSetIsProjectingMethod(DBusMessage* message);
  DBusMessage* HandleSetPolicyMethod(DBusMessage* message);

  // Timeout handler for clean shutdown. If we don't hear back from
  // clean shutdown because the stopping is taking too long or hung,
  // go through with the shutdown now.
  SIGNAL_CALLBACK_0(Daemon, gboolean, CleanShutdownTimedOut);

  // Handles information from the session manager about the session state.
  void OnSessionStateChange(const std::string& state_str);

  void StartCleanShutdown();
  void Shutdown();
  void Suspend();
  void SuspendDisable();
  void SuspendEnable();

  // Generates UMA metrics on when leaving the idle state.
  void GenerateMetricsOnLeavingIdle();

  // Generates UMA metrics on every power event based on the current
  // power status.
  void GenerateMetricsOnPowerEvent(const system::PowerStatus& info);

  // Generates UMA metrics about the current backlight level.
  // Always returns true.
  SIGNAL_CALLBACK_0(Daemon, gboolean, GenerateBacklightLevelMetric);

  // Generates a battery discharge rate UMA metric sample. Returns
  // true if a sample was sent to UMA, false otherwise.
  bool GenerateBatteryDischargeRateMetric(const system::PowerStatus& info,
                                          time_t now);

  // Generates a remaining battery charge and percent of full charge when charge
  // starts UMA metric sample if the current state is correct. Returns true if a
  // sample was sent to UMA, false otherwise.
  void GenerateBatteryInfoWhenChargeStartsMetric(
      const PluggedState& plugged_state,
      const system::PowerStatus& info);

  // Calls all of the metric generation functions that need to be called at the
  // end of a session.
  void GenerateEndOfSessionMetrics(const system::PowerStatus& info,
                                   const base::TimeTicks& now,
                                   const base::TimeTicks& start);

  // Generates a remaining battery charge at end of session UMA metric
  // sample. Returns true if a sample was sent to UMA, false otherwise.
  bool GenerateBatteryRemainingAtEndOfSessionMetric(
      const system::PowerStatus& info);

  // Generates a remaining battery charge at start of session UMA metric
  // sample. Returns true if a sample was sent to UMA, false otherwise.
  bool GenerateBatteryRemainingAtStartOfSessionMetric(
      const system::PowerStatus& info);

  // Generates a number of tiumes the ALS adjusted the backlight during a
  // session UMA metric sample. Returns true if a sample was sent to UMA, false
  // otherwise.
  bool GenerateNumberOfAlsAdjustmentsPerSessionMetric();

  // Generates a number of tiumes the user adjusted the backlight during a
  // session UMA metric sample. Returns true if a sample was sent to UMA, false
  // otherwise.
  bool GenerateUserBrightnessAdjustmentsPerSessionMetric();

  // Generates length of session UMA metric sample. Returns true if a
  // sample was sent to UMA, false otherwise.
  bool GenerateLengthOfSessionMetric(const base::TimeTicks& now,
                                     const base::TimeTicks& start);

  // Increments the number of user sessions that have been active on the
  // current battery charge.
  void IncrementNumOfSessionsPerChargeMetric();

  // Generates number of sessions per charge UMA metric sample if the current
  // stored value is greater then 0. The stored value being 0 are spurious and
  // shouldn't be occuring, since they indicate we are on AC. Returns true if
  // a sample was sent to UMA or a 0 is silently ignored, false otherwise.
  bool GenerateNumOfSessionsPerChargeMetric();

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

  // Send Thermal metrics to Chrome UMA
  void SendThermalMetrics(unsigned int aborted, unsigned int turned_on,
                          unsigned int multiple);

  // Generates UMA metrics for fan thermal state transitions
  // Always returns true.
  SIGNAL_CALLBACK_0(Daemon, gboolean, GenerateThermalMetrics);

  // Sets idle timeouts based on whether the system is projecting to an
  // external display.
  void AdjustIdleTimeoutsForProjection();

  // Checks if power should be maintained due to attached speakers.  This is
  // true for stumpy whenever the headphone jack is used and it avoids a nasty
  // buzzing sound when suspended.
  bool ShouldStayAwakeForHeadphoneJack();

  // Updates state in |backlight_controller_| and |keyboard_controller_|
  // (if non-NULL).
  void SetBacklightsDimmedForInactivity(bool dimmed);
  void SetBacklightsOffForInactivity(bool off);
  void SetBacklightsSuspended(bool suspended);
  void SetBacklightsDocked(bool docked);

  scoped_ptr<StateControllerDelegate> state_controller_delegate_;

  policy::BacklightController* backlight_controller_;
  PrefsInterface* prefs_;
  MetricsLibraryInterface* metrics_lib_;
  policy::KeyboardBacklightController* keyboard_controller_;  // non-owned

  scoped_ptr<DBusSenderInterface> dbus_sender_;
  scoped_ptr<system::Input> input_;
  scoped_ptr<policy::StateController> state_controller_;
  scoped_ptr<policy::InputController> input_controller_;
  scoped_ptr<system::AudioClient> audio_client_;
  scoped_ptr<system::PeripheralBatteryWatcher> peripheral_battery_watcher_;

  bool clean_shutdown_initiated_;
  bool low_battery_;
  int64 clean_shutdown_timeout_ms_;
  guint clean_shutdown_timeout_id_;
  PluggedState plugged_state_;
  FileTagger file_tagger_;
  ShutdownState shutdown_state_;
  scoped_ptr<system::PowerSupply> power_supply_;
  scoped_ptr<policy::DarkResumePolicy> dark_resume_policy_;
  scoped_ptr<SuspenderDelegate> suspender_delegate_;
  policy::Suspender suspender_;
  base::FilePath run_dir_;
  base::TimeTicks session_start_;
  bool is_power_status_stale_;

  // GLib timeouts for running GenerateBacklightLevelMetric() and
  // GenerateThermalMetricsThunk(), or 0 if unset.
  guint generate_backlight_metrics_timeout_id_;
  guint generate_thermal_metrics_timeout_id_;

  // Timestamp the last generated battery discharge rate metric.
  time_t battery_discharge_rate_metric_last_;

  // Timestamp of the last time power button is down.
  base::TimeTicks last_power_button_down_timestamp_;

  // Timestamp of the last idle event.
  base::TimeTicks last_idle_event_timestamp_;

  // Idle time as of last idle event.
  base::TimeDelta last_idle_timedelta_;

  // Timestamps of the last idle-triggered power state transitions.
  base::TimeTicks screen_dim_timestamp_;
  base::TimeTicks screen_off_timestamp_;

  // Last session state that we have been informed of. Initialized as stopped.
  SessionState session_state_;

  // Keep a local copy of power status reading from power_supply.  This way,
  // requests for each field of the power status can be read directly from
  // this struct.  Otherwise we'd have to read the whole struct from
  // power_supply since it doesn't support reading individual fields.
  system::PowerStatus power_status_;

  // For listening to udev events.
  struct udev_monitor* udev_monitor_;
  struct udev* udev_;

  // This is the DBus helper object that dispatches DBus messages to handlers
  util::DBusHandler dbus_handler_;

  // String that indicates reason for shutting down.  See power_constants.cc for
  // valid values.
  std::string shutdown_reason_;

  // Has |state_controller_| been initialized?  Daemon::Init() invokes a
  // bunch of event-handling functions directly, but events shouldn't be
  // passed to |state_controller_| until it's been initialized.
  bool state_controller_initialized_;
};

}  // namespace power_manager

#endif  // POWER_MANAGER_POWERD_DAEMON_H_
