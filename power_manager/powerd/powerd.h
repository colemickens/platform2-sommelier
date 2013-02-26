// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWERD_POWERD_H_
#define POWER_MANAGER_POWERD_POWERD_H_
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
#include "power_manager/common/inotify.h"
#include "power_manager/common/prefs_observer.h"
#include "power_manager/common/signal_callback.h"
#include "power_manager/common/util_dbus_handler.h"
#include "power_manager/powerd/backlight_controller.h"
#include "power_manager/powerd/file_tagger.h"
#include "power_manager/powerd/keyboard_backlight_controller.h"
#include "power_manager/powerd/metrics_store.h"
#include "power_manager/powerd/policy/input_controller.h"
#include "power_manager/powerd/power_supply.h"
#include "power_manager/powerd/rolling_average.h"
#include "power_manager/powerd/suspender.h"
#include "power_manager/powerd/system/audio_observer.h"

// Forward declarations of structs from libudev.h.
struct udev;
struct udev_monitor;

namespace power_manager {

class DBusSenderInterface;
class Prefs;
class StateControl;
class StateController;

namespace policy {
class StateController;
}  // namespace policy

namespace system {
class AudioDetector;
class Input;
}  // namespace system

typedef std::vector<int64> IdleThresholds;

enum PluggedState {
  PLUGGED_STATE_DISCONNECTED,
  PLUGGED_STATE_CONNECTED,
  PLUGGED_STATE_UNKNOWN,
};

// The raw battery percentage value that we receive from the battery controller
// is not fit for displaying to the user since it does not repersent the actual
// usable percentage since we do a safe shutdown in low battery conditions and
// the battery might not charge to full capacity under certain
// circumstances. During regular operation we linearly scale the raw value so
// that the low level cut off is 0%. This being done is indicated by
// BATTERY_REPORT_ADJUSTED. Once the battery has ceased to charge and is marked
// as full, 100% is displayed which is is indicated by the state
// BATTERY_REPORT_FULL. When we start discharging from full the battery value is
// held/pinned at 100% for a brief period to avoid an immediate drop in
// percentage due to the difference between the adjusted/raw value and 100%,
// which is indicated by BATTERY_REPORT_PINNED. After holding the percentage at
// 100% is done the system linearly tapers from 100% to the true adjust value
// over a period of time to eliminate any jumps, which is indicated by the state
// BATTERY_REPORT_TAPERED.
enum BatteryReportState {
  BATTERY_REPORT_ADJUSTED,
  BATTERY_REPORT_FULL,
  BATTERY_REPORT_PINNED,
  BATTERY_REPORT_TAPERED,
};

class Daemon : public BacklightControllerObserver,
               public policy::InputController::Delegate,
               public system::AudioObserver {
 public:
  // Note that keyboard_controller is an optional parameter (it can be NULL) and
  // that the memory is owned by the caller.
  Daemon(BacklightController* ctl,
         PrefsInterface* prefs,
         MetricsLibraryInterface* metrics_lib,
         KeyboardBacklightController* keyboard_controller,
         const base::FilePath& run_dir);
  ~Daemon();

  BacklightController* backlight_controller() { return backlight_controller_; }

  const std::string& current_user() const { return current_user_; }

  void set_disable_dbus_for_testing(bool disable) {
    disable_dbus_for_testing_ = disable;
  }

  void Init();
  void Run();
  void SetActive();
  void UpdateIdleStates();
  void SetPlugged(bool plugged);

  void OnRequestRestart();
  void OnRequestShutdown();

  // Shuts the system down in response to a failed suspend attempt.
  void ShutdownForFailedSuspend();

  // Add an idle threshold to notify on.
  void AddIdleThreshold(int64 threshold);

  // Notify chrome that an idle event happened.
  void IdleEventNotify(int64 threshold);

  // If in the active-but-off state, turn up the brightness when user presses a
  // key so user can see that the screen has been locked.
  void BrightenScreenIfOff();

  // Overridden from BacklightControllerObserver:
  virtual void OnBrightnessChanged(double brightness_percent,
                                   BrightnessChangeCause cause,
                                   BacklightController* source) OVERRIDE;

  // Removes the current power supply polling timer.
  void HaltPollPowerSupply();

  // Removes the current power supply polling timer. It then schedules an
  // immediate poll that knows the value is suspect and another in 5s once the
  // battery state has settled.
  void ResumePollPowerSupply();

  // Flags the current information about the power supply as stale, so that if a
  // delayed request comes in for data, we know to poll the power supply
  // again. This is used in the case of Suspend/Resume, since we may have told
  // Chrome there is new information before suspending and Chrome requests it on
  // resume before we have updated.
  void MarkPowerStatusStale();

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
  virtual void EnsureBacklightIsOn() OVERRIDE;
  virtual void SendPowerButtonMetric(bool down, base::TimeTicks timestamp)
      OVERRIDE;

  // Overridden from system::AudioObserver:
  virtual void OnAudioActivity(base::TimeTicks last_audio_time) OVERRIDE;

 private:
  friend class DaemonTest;
  FRIEND_TEST(DaemonTest, AdjustWindowSizeMax);
  FRIEND_TEST(DaemonTest, AdjustWindowSizeMin);
  FRIEND_TEST(DaemonTest, AdjustWindowSizeCalc);
  FRIEND_TEST(DaemonTest, ExtendTimeoutsWhenProjecting);
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
  FRIEND_TEST(DaemonTest, UpdateAveragedTimesChargingAndCalculating);
  FRIEND_TEST(DaemonTest, UpdateAveragedTimesChargingAndNotCalculating);
  FRIEND_TEST(DaemonTest, UpdateAveragedTimesDischargingAndCalculating);
  FRIEND_TEST(DaemonTest, UpdateAveragedTimesDischargingAndNotCalculating);
  FRIEND_TEST(DaemonTest, UpdateAveragedTimesWithSetThreshold);
  FRIEND_TEST(DaemonTest, UpdateAveragedTimesWithBadStatus);
  FRIEND_TEST(DaemonTest, TurnBacklightOnForPowerButton);
  FRIEND_TEST(DaemonTest, DetectUSBDevices);
  FRIEND_TEST(DaemonTest, GetDisplayBatteryPercent);
  FRIEND_TEST(DaemonTest, GetUsableBatteryPercent);

  enum IdleState {
    IDLE_STATE_UNKNOWN,
    IDLE_STATE_NORMAL,
    IDLE_STATE_DIM,
    IDLE_STATE_SCREEN_OFF,
    IDLE_STATE_SUSPEND
  };

  enum ShutdownState {
    SHUTDOWN_STATE_NONE,
    SHUTDOWN_STATE_RESTARTING,
    SHUTDOWN_STATE_POWER_OFF
  };

  class StateControllerDelegate;

  // Reads settings from disk
  void ReadSettings();

  // Reads lock screen settings
  void ReadLockScreenSettings();

  // Reads suspend disable/timeout settings
  void ReadSuspendSettings();

  // Initializes metrics
  void MetricInit();

  // Updates our idle state based on the provided |idle_time_ms|
  void SetIdleState(int64 idle_time_ms);

  // Decrease / increase the keyboard brightness; direction should be +1 for
  // increase and -1 for decrease.
  void AdjustKeyboardBrightness(int direction);

  // Shared code between keyboard and screen brightness changed handling
  void SendBrightnessChangedSignal(double brightness_percent,
                                   BrightnessChangeCause cause,
                                   const std::string& signal_name);

  // Sets up idle timers, adding the provided offset to all timeouts
  // starting with the state provided except the locking timeout.
  void SetIdleOffset(int64 offset_ms, IdleState state);

  static void OnPowerEvent(void* object, const PowerStatus& info);

  // Handles power supply udev events.
  static gboolean UdevEventHandler(GIOChannel*,
                                   GIOCondition condition,
                                   gpointer data);

  // Registers udev event handler with GIO.
  void RegisterUdevEventHandler();

  // Registers the dbus message handler with appropriate dbus events.
  void RegisterDBusMessageHandler();

  // Callbacks for handling dbus messages.
  bool HandleRequestSuspendSignal(DBusMessage* message);
  bool HandleCleanShutdownSignal(DBusMessage* message);
  bool HandleSessionManagerSessionStateChangedSignal(DBusMessage* message);
  DBusMessage* HandleRequestShutdownMethod(DBusMessage* message);
  DBusMessage* HandleRequestRestartMethod(DBusMessage* message);
  DBusMessage* HandleDecreaseScreenBrightnessMethod(DBusMessage* message);
  DBusMessage* HandleIncreaseScreenBrightnessMethod(DBusMessage* message);
  DBusMessage* HandleGetScreenBrightnessMethod(DBusMessage* message);
  DBusMessage* HandleSetScreenBrightnessMethod(DBusMessage* message);
  DBusMessage* HandleDecreaseKeyboardBrightnessMethod(DBusMessage* message);
  DBusMessage* HandleIncreaseKeyboardBrightnessMethod(DBusMessage* message);
  DBusMessage* HandleGetIdleTimeMethod(DBusMessage* message);
  DBusMessage* HandleRequestIdleNotificationMethod(DBusMessage* message);
  DBusMessage* HandleGetPowerSupplyPropertiesMethod(DBusMessage* message);
  DBusMessage* HandleStateOverrideRequestMethod(DBusMessage* message);
  DBusMessage* HandleStateOverrideCancelMethod(DBusMessage* message);
  DBusMessage* HandleVideoActivityMethod(DBusMessage* message);
  DBusMessage* HandleUserActivityMethod(DBusMessage* message);
  DBusMessage* HandleSetIsProjectingMethod(DBusMessage* message);
  DBusMessage* HandleSetPolicyMethod(DBusMessage* message);

  // Removes the previous power supply polling timer and replaces it with one
  // that fires every 5s and calls ShortPollPowerSupply. The nature of this
  // callback will cause the timer to only fire once and then return to the
  // regular PollPowerSupply.
  void ScheduleShortPollPowerSupply();

  // Removes the previous power supply polling timer and replaces it with one
  // that fires every 30s and calls PollPowerSupply.
  void SchedulePollPowerSupply();

  // Handles polling the power supply due to change in its state. Reschedules
  // the polling timer, so it doesn't fire too close to a state change. It then
  // reads power supply status and sets is_calculating_battery_time to true to
  // indicate that this value shouldn't be trusted to be accurate. It then calls
  // a shared handler to signal chrome that fresh data is available.
  gboolean EventPollPowerSupply();

  // Read the power supply status once and then schedules the regular
  // polling. This is done to allow for a one off short duration poll right
  // after a power event.
  SIGNAL_CALLBACK_0(Daemon, gboolean, ShortPollPowerSupply);

  // Reads power supply status at regular intervals, and sends a signal to
  // indicate that fresh power supply data is available.
  SIGNAL_CALLBACK_0(Daemon, gboolean, PollPowerSupply);

  // Shared handler used for servicing when we have polled the state of the
  // battery. This method sends a signal to chrome about there being fresh data
  // and generates related metrics.
  gboolean HandlePollPowerSupply();

  // Update the averaged values in |power_status_| and add the battery time
  // estimate values from |power_status_| to the appropriate rolling
  // averages. Returns false if the input to the function was invalid.
  bool UpdateAveragedTimes(RollingAverage* empty_average,
                           RollingAverage* full_average);

  // Given the current battery time estimate adjust the rolling
  // average window sizes to give the desired linear tapering.
  void AdjustWindowSize(int64 battery_time,
                        RollingAverage* empty_average,
                        RollingAverage* full_average);

  // Checks for extremely low battery condition.
  void OnLowBattery(int64 time_remaining_s,
                    int64 time_full_s,
                    double battery_percentage);

  // Timeout handler for clean shutdown. If we don't hear back from
  // clean shutdown because the stopping is taking too long or hung,
  // go through with the shutdown now.
  SIGNAL_CALLBACK_0(Daemon, gboolean, CleanShutdownTimedOut);

  // Handles information from the session manager about the session state.
  // Invoked by RetrieveSessionState() and also in response to
  // SessionStateChanged D-Bus signals.
  void OnSessionStateChange(const std::string& state_str,
                            const std::string& user);

  void StartCleanShutdown();
  void Shutdown();
  void Suspend();
  void SuspendDisable();
  void SuspendEnable();

  // Generates UMA metrics on when leaving the idle state.
  void GenerateMetricsOnLeavingIdle();

  // Generates UMA metrics on every power event based on the current
  // power status.
  void GenerateMetricsOnPowerEvent(const PowerStatus& info);

  // Generates UMA metrics about the current backlight level.
  // Always returns true.
  SIGNAL_CALLBACK_0(Daemon, gboolean, GenerateBacklightLevelMetric);

  // Generates a battery discharge rate UMA metric sample. Returns
  // true if a sample was sent to UMA, false otherwise.
  bool GenerateBatteryDischargeRateMetric(const PowerStatus& info,
                                          time_t now);

  // Generates a remaining battery charge and percent of full charge when charge
  // starts UMA metric sample if the current state is correct. Returns true if a
  // sample was sent to UMA, false otherwise.
  void GenerateBatteryInfoWhenChargeStartsMetric(
      const PluggedState& plugged_state,
      const PowerStatus& info);

  // Calls all of the metric generation functions that need to be called at the
  // end of a session.
  void GenerateEndOfSessionMetrics(const PowerStatus& info,
                                   const BacklightController& backlight,
                                   const base::TimeTicks& now,
                                   const base::TimeTicks& start);

  // Generates a remaining battery charge at end of session UMA metric
  // sample. Returns true if a sample was sent to UMA, false otherwise.
  bool GenerateBatteryRemainingAtEndOfSessionMetric(const PowerStatus& info);

  // Generates a remaining battery charge at start of session UMA metric
  // sample. Returns true if a sample was sent to UMA, false otherwise.
  bool GenerateBatteryRemainingAtStartOfSessionMetric(const PowerStatus& info);

  // Generates a number of tiumes the ALS adjusted the backlight during a
  // session UMA metric sample. Returns true if a sample was sent to UMA, false
  // otherwise.
  bool GenerateNumberOfAlsAdjustmentsPerSessionMetric(
    const BacklightController& backlight);

  // Generates a number of tiumes the user adjusted the backlight during a
  // session UMA metric sample. Returns true if a sample was sent to UMA, false
  // otherwise.
  bool GenerateUserBrightnessAdjustmentsPerSessionMetric(
    const BacklightController& backlight);

  // Generates length of session UMA metric sample. Returns true if a
  // sample was sent to UMA, false otherwise.
  bool GenerateLengthOfSessionMetric(const base::TimeTicks& now,
                                     const base::TimeTicks& start);

  // Generates number of sessions per charge UMA metric sample if the current
  // stored value is greater then 0. The stored value being 0 are spurious and
  // shouldn't be occuring, since they indicate we are on AC. Returns true if
  // a sample was sent to UMA or a 0 is silently ignored, false otherwise.
  bool GenerateNumOfSessionsPerChargeMetric(MetricsStore* store);

  // Utility method used on plugged state change to do the right thing wrt to
  // the NumberOfSessionsPerCharge Metric.
  void HandleNumOfSessionsPerChargeOnSetPlugged(
      MetricsStore* metrics_store,
      const PluggedState& plugged_state);

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

  // Sends a synchronous D-Bus request to the session manager to retrieve the
  // session state and updates |current_user_| based on the response.
  void RetrieveSessionState();

  // Sets idle timeouts based on whether the system is projecting to an
  // external display.
  void AdjustIdleTimeoutsForProjection();

  // Checks if power should be maintained due to attached speakers.  This is
  // true for stumpy whenever the headphone jack is used and it avoids a nasty
  // buzzing sound when suspended.
  bool ShouldStayAwakeForHeadphoneJack();

  // Send changes to the backlight power state to the backlight
  // controllers. This also will determine if the ALS needs to be toggled
  // on/off.
  void SetPowerState(PowerState state);

  // Updates |battery_report_state_| to account for changes in the power state
  // of the device and passage of time. This value is used to control the value
  // we display to the user for battery time, so this should be called before
  // making a call to GetDisplayBatteryPercent.
  void UpdateBatteryReportState();

  // Generates the battery percentage that will be sent to Chrome for display to
  // the user. This value is an adjusted version of the raw value to be more
  // useful to the end user.
  double GetDisplayBatteryPercent() const;

  // Generates an adjusted form of the raw battery percentage that accounts for
  // the raw value being out of range and for the small bit lost due to low
  // battery shutdown.
  double GetUsableBatteryPercent() const;

  scoped_ptr<StateControllerDelegate> state_controller_delegate_;

  BacklightController* backlight_controller_;
  PrefsInterface* prefs_;
  MetricsLibraryInterface* metrics_lib_;
  KeyboardBacklightController* keyboard_controller_;  // non-owned

  scoped_ptr<DBusSenderInterface> dbus_sender_;
  scoped_ptr<system::Input> input_;
  scoped_ptr<policy::InputController> input_controller_;
  scoped_ptr<system::AudioDetector> audio_detector_;
  scoped_ptr<policy::StateController> state_controller_;

  int64 low_battery_shutdown_time_s_;
  double low_battery_shutdown_percent_;
  int64 sample_window_max_;
  int64 sample_window_min_;
  int64 sample_window_diff_;
  int64 taper_time_max_s_;
  int64 taper_time_min_s_;
  int64 taper_time_diff_s_;
  bool clean_shutdown_initiated_;
  bool low_battery_;
  int64 clean_shutdown_timeout_ms_;
  guint clean_shutdown_timeout_id_;
  int64 battery_poll_interval_ms_;
  int64 battery_poll_short_interval_ms_;
  PluggedState plugged_state_;
  FileTagger file_tagger_;
  ShutdownState shutdown_state_;
  scoped_ptr<Suspender::Delegate> suspender_delegate_;
  Suspender suspender_;
  base::FilePath run_dir_;
  PowerSupply power_supply_;
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
  std::map<PowerState, base::TimeTicks> idle_transition_timestamps_;

  // User whose session is currently active, or empty if no session is
  // active or we're in guest mode.
  std::string current_user_;

  // Last session state that we have been informed of. Initialized as stopped.
  SessionState current_session_state_;

  // Keep a local copy of power status reading from power_supply.  This way,
  // requests for each field of the power status can be read directly from
  // this struct.  Otherwise we'd have to read the whole struct from
  // power_supply since it doesn't support reading individual fields.
  PowerStatus power_status_;

  // For listening to udev events.
  struct udev_monitor* udev_monitor_;
  struct udev* udev_;

  // Persistent storage for metrics that need to exist for more then one session
  MetricsStore metrics_store_;

  // The state_control_ class manages requests to disable different parts
  // of the state machine.  kiosk mode and autoupdate are clients of this
  // as they may need to disable different idle timeouts when they are running
  scoped_ptr<StateControl> state_control_;

  // Value returned when we add a timer for polling the power supply. This is
  // needed for removing the timer when we want to interrupt polling.
  guint32 poll_power_supply_timer_id_;

  // This is the DBus helper object that dispatches DBus messages to handlers
  util::DBusHandler dbus_handler_;

  // Rolling averages used to iron out instabilities in the time estimates
  RollingAverage time_to_empty_average_;
  RollingAverage time_to_full_average_;

  // String that indicates reason for shutting down.  See power_constants.cc for
  // valid values.
  std::string shutdown_reason_;

  // Variables used for pinning and tapering the battery after we have adjusted
  // it to account for being near full but not charging. The state value tells
  // use what we should be doing with the value and time values are used for
  // controlling when to transition states and calculate values.
  BatteryReportState battery_report_state_;
  base::TimeTicks battery_report_pinned_start_;
  base::TimeTicks battery_report_tapered_start_;

  // Set by tests to disable emitting D-Bus signals.
  bool disable_dbus_for_testing_;

  // Has |state_controller_| been initialized?  Daemon::Init() invokes a
  // bunch of event-handling functions directly, but events shouldn't be
  // passed to |state_controller_| until it's been initialized.
  bool state_controller_initialized_;
};

}  // namespace power_manager

#endif  // POWER_MANAGER_POWERD_POWERD_H_
