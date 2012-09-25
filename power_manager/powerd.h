// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWERD_H_
#define POWER_MANAGER_POWERD_H_
#pragma once

#include <dbus/dbus-glib-lowlevel.h>
#include <glib.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST

#include <map>
#include <string>
#include <utility>
#include <vector>

#include <ctime>

#include "base/bind.h"
#include "base/callback.h"
#include "base/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/time.h"
#include "chromeos/dbus/dbus.h"
#include "chromeos/glib/object.h"
#include "metrics/metrics_library.h"
#include "power_manager/backlight_controller.h"
#include "power_manager/backlight_interface.h"
#include "power_manager/file_tagger.h"
#include "power_manager/idle_detector.h"
#include "power_manager/inotify.h"
#include "power_manager/keyboard_backlight_controller.h"
#include "power_manager/metrics_store.h"
#include "power_manager/power_prefs.h"
#include "power_manager/power_supply.h"
#include "power_manager/rolling_average.h"
#include "power_manager/screen_locker.h"
#include "power_manager/signal_callback.h"
#include "power_manager/suspender.h"

// Forward declarations of structs from libudev.h.
struct udev;
struct udev_monitor;
// From cras_client.h
struct cras_client;

namespace power_manager {

class ActivityDetectorInterface;
class PowerButtonHandler;
class StateControl;

typedef std::vector<int64> IdleThresholds;

enum PluggedState {
  kPowerDisconnected,
  kPowerConnected,
  kPowerUnknown,
};

class Daemon : public BacklightControllerObserver,
               public IdleObserver {
 public:
  // Note that keyboard_controller is an optional parameter (it can be NULL) and
  // that the memory is owned by the caller.
  Daemon(BacklightController* ctl,
         PowerPrefs* prefs,
         MetricsLibraryInterface* metrics_lib,
         VideoDetector* video_detector,
         ActivityDetectorInterface* audio_detector,
         IdleDetector* idle,
         KeyboardBacklightController* keyboard_controller,
         AmbientLightSensor* als,
         const FilePath& run_dir);
  ~Daemon();

  ScreenLocker* locker() { return &locker_; }
  BacklightController* backlight_controller() { return backlight_controller_; }

  const std::string& current_user() const { return current_user_; }

  void Init();
  void Run();
  void SetActive();
  void UpdateIdleStates();
  void SetPlugged(bool plugged);

  void OnRequestRestart();
  void OnRequestShutdown();

  // Add an idle threshold to notify on.
  void AddIdleThreshold(int64 threshold);

  // Notify chrome that an idle event happened.
  void IdleEventNotify(int64 threshold);

  // If in the active-but-off state, turn up the brightness when user presses a
  // key so user can see that the screen has been locked.
  void BrightenScreenIfOff();

  // Overridden from IdleObserver:
  virtual void OnIdleEvent(bool is_idle, int64 idle_time_ms);

  // Overridden from BacklightControllerObserver:
  virtual void OnBrightnessChanged(double brightness_percent,
                                   BrightnessChangeCause cause,
                                   BacklightController* source);

  // Removes the current power supply polling timer.
  void HaltPollPowerSupply();

  // Removes the current power supply polling timer. It then schedules an
  // immediate poll that knows the value is suspect and another in 5s once the
  // battery state has settled.
  void ResumePollPowerSupply();

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
  FRIEND_TEST(DaemonTest, GenerateBatteryRemainingWhenChargeStartsMetric);
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
  FRIEND_TEST(DaemonTest, TurnBacklightOnForPowerButton);
  FRIEND_TEST(DaemonTest, DetectUSBDevices);

  enum IdleState { kIdleUnknown, kIdleNormal, kIdleDim, kIdleScreenOff,
                   kIdleSuspend };

  enum ShutdownState { kShutdownNone, kShutdownRestarting,
                       kShutdownPowerOff };

  typedef std::pair<std::string, std::string> DBusInterfaceMemberPair;
  typedef base::Callback<bool(DBusMessage*)> DBusSignalHandler;
  typedef base::Callback<DBusMessage*(DBusMessage*)> DBusMethodHandler;
  typedef bool (Daemon::*DBusSignalHandlerFunc)(DBusMessage*);
  typedef DBusMessage* (Daemon::*DBusMethodHandlerFunc)(DBusMessage*);

  typedef std::map<DBusInterfaceMemberPair, DBusSignalHandler>
      DBusSignalHandlerTable;
  typedef std::map<DBusInterfaceMemberPair, DBusMethodHandler>
      DBusMethodHandlerTable;

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

  // Standard handlers for dbus method calls and signals. |data| contains a
  // pointer to a Daemon object.
  static DBusHandlerResult MainDBusMethodHandler(
      DBusConnection*, DBusMessage* message, void* data);
  static DBusHandlerResult MainDBusSignalHandler(
      DBusConnection*, DBusMessage* message, void* data);

  // Registers the dbus message handler with appropriate dbus events.
  void RegisterDBusMessageHandler();

  // Callbacks for handling dbus messages.
  bool HandleRequestSuspendSignal(DBusMessage* message);
  bool HandleLidClosedSignal(DBusMessage* message);
  bool HandleLidOpenedSignal(DBusMessage* message);
  bool HandleButtonEventSignal(DBusMessage* message);
  bool HandleCleanShutdownSignal(DBusMessage* message);
  bool HandlePowerStateChangedSignal(DBusMessage* message);
  bool HandleSessionManagerSessionStateChangedSignal(DBusMessage* message);
  bool HandleSessionManagerScreenIsLockedSignal(DBusMessage* message);
  bool HandleSessionManagerScreenIsUnlockedSignal(DBusMessage* message);
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

  void AddDBusSignalHandler(const std::string& interface,
                            const std::string& member,
                            DBusSignalHandlerFunc handler);

  void AddDBusMethodHandler(const std::string& interface,
                            const std::string& member,
                            DBusMethodHandlerFunc handler);

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

  // Update the averaged values in |status| and add the battery time estimate
  // values from |status| to the appropriate rolling averages.
  void UpdateAveragedTimes(PowerStatus* status,
                           RollingAverage* empty_average,
                           RollingAverage* full_average);

  // Given the current battery time estimate adjust the rolling
  // average window sizes to give the desired linear tapering.
  void AdjustWindowSize(int64 battery_time,
                        RollingAverage* empty_average,
                        RollingAverage* full_average);

  // Checks for extremely low battery condition.
  void OnLowBattery(int64 time_threshold_s,
                    int64 time_remaining_s);

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

  // Handles notification from powerm that a button has been pressed or
  // released.
  void OnButtonEvent(const std::string& button_name,
                     bool down,
                     const base::TimeTicks& timestamp);

  // Sends metrics in response to the power button being pressed or released.
  // Called by HandleButtonEventSignal().
  void SendPowerButtonMetric(bool down, const base::TimeTicks& timestamp);

  void StartCleanShutdown();
  void Shutdown();
  void Suspend();
  void SuspendDisable();
  void SuspendEnable();

  // Callback for Inotify of Preference directory changes.
  static gboolean PrefChangeHandler(const char* name, int watch_handle,
                                    unsigned int mask, gpointer data);

  // Generates UMA metrics on every idle event.
  void GenerateMetricsOnIdleEvent(bool is_idle, int64 idle_time_ms);

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

  // Generates a remaining battery charge when charge starts UMA metric sample
  // if the current state is correct. Returns true if a sample was sent to UMA,
  // false otherwise.
  void GenerateBatteryRemainingWhenChargeStartsMetric(
      const PluggedState& plugged_state,
      const PowerStatus& info);

  // Calls all of the metric generation functions that need to be called at the
  // end of a session.
  void GenerateEndOfSessionMetrics(const PowerStatus& info,
                                   const BacklightController& backlight,
                                   const base::Time& now,
                                   const base::Time& start);

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
  bool GenerateLengthOfSessionMetric(const base::Time& now,
                                     const base::Time& start);

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

  // Called by dbus handler when resume signal is received
  void HandleResume();

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

  // Attempts to connect to ChromeOS audio server.  Used in glib main loop.
  // Returns TRUE if it does not connect, so it tries again.
  // Returns FALSE if it successfully connected, so it stops trying.
  SIGNAL_CALLBACK_0(Daemon, gboolean, ConnectToCras);

  // Send changes to the backlight power state to the backlight
  // controllers. This also will determine if the ALS needs to be toggled
  // on/off.
  void SetPowerState(PowerState state);

  // Checks cras to determine if audio has been playing recently.
  // "Recently" is defined by |kAudioActivityThresholdMs| in powerd.cc.
  bool IsAudioPlaying();

  // Checks if any USB input devices are connected, by scanning sysfs for input
  // devices whose paths contain "usb".
  bool USBInputDeviceConnected() const;

  BacklightController* backlight_controller_;
  PowerPrefs* prefs_;
  MetricsLibraryInterface* metrics_lib_;
  VideoDetector* video_detector_;
  ActivityDetectorInterface* audio_detector_;
  IdleDetector* idle_;
  KeyboardBacklightController* keyboard_controller_;  // non-owned
  AmbientLightSensor* light_sensor_; // non-owned
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
  bool enforce_lock_;
  bool lock_on_idle_suspend_;
  PluggedState plugged_state_;
  FileTagger file_tagger_;
  ShutdownState shutdown_state_;
  ScreenLocker locker_;
  Suspender suspender_;
  FilePath run_dir_;
  PowerSupply power_supply_;
  PowerState power_state_;
  base::Time session_start_;

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
  std::string current_session_state_;

  // Stores normal timeout values, to be used for switching between projecting
  // and non-projecting timeouts.  Map keys are variable names found in
  // power_constants.h.
  std::map<std::string, int64> base_timeout_values_;

  // List of thresholds to notify Chrome on.
  IdleThresholds thresholds_;

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

  // These are lookup tables that map dbus message interface/names to handlers.
  DBusSignalHandlerTable dbus_signal_handler_table_;
  DBusMethodHandlerTable dbus_method_handler_table_;

  // Rolling averages used to iron out instabilities in the time estimates
  RollingAverage time_to_empty_average_;
  RollingAverage time_to_full_average_;

  // Flag indicating whether the system is projecting to an external display.
  bool is_projecting_;

  // Chrome OS audio server client.  Used to check if headphone jack is plugged.
  cras_client* cras_client_;

  // Indicates whether the cras client has connected to cras server and is up
  // and running.
  bool connected_to_cras_;

  // String that indicates reason for shutting down.  See power_constants.cc for
  // valid values.
  std::string shutdown_reason_;

  // Flag indicating that this system needs a USB input device connected before
  // suspending, otherwise it cannot wake up from suspend.
  bool require_usb_input_device_to_suspend_;

  // Used by USBInputDeviceConnected() instead of the default input path, if
  // this string is non-empty.  Used for testing purposes.
  std::string sysfs_input_path_for_testing_;
};

}  // namespace power_manager

#endif  // POWER_MANAGER_POWERD_H_
