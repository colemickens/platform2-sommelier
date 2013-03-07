// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/powerd.h"

#include <libudev.h>
#include <stdint.h>
#include <sys/inotify.h>

#include <algorithm>
#include <cmath>
#include <set>
#include <vector>

#include "base/bind.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "chromeos/dbus/dbus.h"
#include "chromeos/dbus/service_constants.h"
#include "power_manager/common/dbus_sender.h"
#include "power_manager/common/power_constants.h"
#include "power_manager/common/prefs.h"
#include "power_manager/common/util.h"
#include "power_manager/common/util_dbus.h"
#include "power_manager/policy.pb.h"
#include "power_manager/powerd/backlight_controller.h"
#include "power_manager/powerd/metrics_constants.h"
#include "power_manager/powerd/policy/state_controller.h"
#include "power_manager/powerd/power_supply.h"
#include "power_manager/powerd/state_control.h"
#include "power_manager/powerd/system/audio_detector.h"
#include "power_manager/powerd/system/input.h"
#include "power_state_control.pb.h"
#include "power_supply_properties.pb.h"
#include "video_activity_update.pb.h"

namespace {

// Path for storing FileTagger files.
const char kTaggedFilePath[] = "/var/lib/power_manager";

// Path to power supply info.
const char kPowerStatusPath[] = "/sys/class/power_supply";

// Power supply subsystem for udev events.
const char kPowerSupplyUdevSubsystem[] = "power_supply";

// Strings for states that powerd cares about from the session manager's
// SessionStateChanged signal.
const char kSessionStarted[] = "started";
const char kSessionStopped[] = "stopped";

// Upper limit to accept for raw battery times, in seconds. If the time of
// interest is above this level assume something is wrong.
const int64 kBatteryTimeMaxValidSec = 24 * 60 * 60;

}  // namespace

namespace power_manager {

// Performs actions requested by |state_controller_|.  The reason that
// this is a nested class of Daemon rather than just being implented as
// part of Daemon is to avoid method naming conflicts.
class Daemon::StateControllerDelegate
    : public policy::StateController::Delegate {
 public:
  explicit StateControllerDelegate(Daemon* daemon)
      : daemon_(daemon),
        screen_dimmed_(false),
        screen_off_(false) {
  }

  ~StateControllerDelegate() {
    daemon_ = NULL;
  }

  // Overridden from policy::StateController::Delegate:
  virtual bool IsUsbInputDeviceConnected() OVERRIDE {
    return daemon_->input_->IsUSBInputDeviceConnected();
  }

  virtual bool IsOobeCompleted() OVERRIDE {
    return util::OOBECompleted();
  }

  virtual bool ShouldAvoidSuspendForHeadphoneJack() OVERRIDE {
#ifdef STAY_AWAKE_PLUGGED_DEVICE
    return daemon_->audio_detector_->IsHeadphoneJackConnected();
#else
    return false;
#endif
  }

  virtual LidState QueryLidState() OVERRIDE {
    LidState state = LID_OPEN;
    return daemon_->input_->QueryLidState(&state) ? state : LID_OPEN;
  }

  virtual void DimScreen() OVERRIDE {
    screen_dimmed_ = true;
    if (!screen_off_) {
      daemon_->SetPowerState(BACKLIGHT_DIM);
      base::TimeTicks now = base::TimeTicks::Now();
      daemon_->idle_transition_timestamps_[BACKLIGHT_DIM] = now;
      daemon_->last_idle_event_timestamp_ = now;
      daemon_->last_idle_timedelta_ =
          now - daemon_->state_controller_->last_user_activity_time();
    }
  }

  virtual void UndimScreen() OVERRIDE {
    screen_dimmed_ = false;
    if (!screen_off_)
      daemon_->SetPowerState(BACKLIGHT_ACTIVE);
  }

  virtual void TurnScreenOff() OVERRIDE {
    screen_off_ = true;
    daemon_->SetPowerState(BACKLIGHT_IDLE_OFF);
    base::TimeTicks now = base::TimeTicks::Now();
    daemon_->idle_transition_timestamps_[BACKLIGHT_IDLE_OFF] = now;
    daemon_->last_idle_event_timestamp_ = now;
    daemon_->last_idle_timedelta_ =
        now - daemon_->state_controller_->last_user_activity_time();
  }

  virtual void TurnScreenOn() OVERRIDE {
    screen_off_ = false;
    daemon_->SetPowerState(screen_dimmed_ ? BACKLIGHT_DIM : BACKLIGHT_ACTIVE);
  }

  virtual void LockScreen() OVERRIDE {
    util::CallSessionManagerMethod(
        login_manager::kSessionManagerLockScreen, NULL);
  }

  virtual void Suspend() OVERRIDE {
    daemon_->Suspend();
  }

  virtual void StopSession() OVERRIDE {
    // This session manager method takes a string argument, although it
    // doesn't currently do anything with it.
    util::CallSessionManagerMethod(
        login_manager::kSessionManagerStopSession, "");
  }

  virtual void ShutDown() OVERRIDE {
    // TODO(derat): Maybe pass the shutdown reason (idle vs. lid-closed)
    // and set it here.  This isn't necessary at the moment, since nothing
    // special is done for any reason besides |kShutdownReasonLowBattery|.
    daemon_->OnRequestShutdown();
  }

  virtual void EmitIdleNotification(base::TimeDelta delay) OVERRIDE {
    daemon_->IdleEventNotify(delay.InMilliseconds());
  }

  virtual void ReportUserActivityMetrics() OVERRIDE {
    if (!daemon_->last_idle_event_timestamp_.is_null())
      daemon_->GenerateMetricsOnLeavingIdle();
  }

 private:
  Daemon* daemon_;  // not owned

  bool screen_dimmed_;
  bool screen_off_;
};

// Daemon: Main power manager. Adjusts device status based on whether the
//         user is idle and on video activity indicator from Chrome.
//         This daemon is responsible for dimming of the backlight, turning
//         the screen off, and suspending to RAM. The daemon also has the
//         capability of shutting the system down.

Daemon::Daemon(BacklightController* backlight_controller,
               PrefsInterface* prefs,
               MetricsLibraryInterface* metrics_lib,
               KeyboardBacklightController* keyboard_controller,
               const base::FilePath& run_dir)
    : state_controller_delegate_(new StateControllerDelegate(this)),
      backlight_controller_(backlight_controller),
      prefs_(prefs),
      metrics_lib_(metrics_lib),
      keyboard_controller_(keyboard_controller),
      dbus_sender_(
          new DBusSender(kPowerManagerServicePath, kPowerManagerInterface)),
      input_(new system::Input),
      input_controller_(
          new policy::InputController(input_.get(), this, dbus_sender_.get(),
                                      run_dir)),
      audio_detector_(new system::AudioDetector),
      state_controller_(new policy::StateController(
          state_controller_delegate_.get(), prefs_)),
      low_battery_shutdown_time_s_(0),
      low_battery_shutdown_percent_(0.0),
      sample_window_max_(0),
      sample_window_min_(0),
      sample_window_diff_(0),
      taper_time_max_s_(0),
      taper_time_min_s_(0),
      taper_time_diff_s_(0),
      clean_shutdown_initiated_(false),
      low_battery_(false),
      clean_shutdown_timeout_id_(0),
      battery_poll_interval_ms_(0),
      battery_poll_short_interval_ms_(0),
      plugged_state_(PLUGGED_STATE_UNKNOWN),
      file_tagger_(base::FilePath(kTaggedFilePath)),
      shutdown_state_(SHUTDOWN_STATE_NONE),
      suspender_delegate_(Suspender::CreateDefaultDelegate(this, input_.get())),
      suspender_(suspender_delegate_.get(), dbus_sender_.get()),
      run_dir_(run_dir),
      power_supply_(base::FilePath(kPowerStatusPath), prefs),
      is_power_status_stale_(true),
      generate_backlight_metrics_timeout_id_(0),
      generate_thermal_metrics_timeout_id_(0),
      battery_discharge_rate_metric_last_(0),
      current_session_state_(SESSION_STOPPED),
      udev_(NULL),
      state_control_(new StateControl(this)),
      poll_power_supply_timer_id_(0),
      shutdown_reason_(kShutdownReasonUnknown),
      battery_report_state_(BATTERY_REPORT_ADJUSTED),
      disable_dbus_for_testing_(false),
      state_controller_initialized_(false) {
  audio_detector_->AddObserver(this);
}

Daemon::~Daemon() {
  audio_detector_->RemoveObserver(this);

  util::RemoveTimeout(&clean_shutdown_timeout_id_);
  util::RemoveTimeout(&generate_backlight_metrics_timeout_id_);
  util::RemoveTimeout(&generate_thermal_metrics_timeout_id_);

  if (udev_)
    udev_unref(udev_);
}

void Daemon::Init() {
  ReadSettings();
  MetricInit();
  LOG_IF(ERROR, (!metrics_store_.Init()))
      << "Unable to initialize metrics store, so we are going to drop number of"
      << " sessions per charge data";

  RegisterUdevEventHandler();
  RegisterDBusMessageHandler();
  RetrieveSessionState();
  suspender_.Init(prefs_);
  time_to_empty_average_.Init(sample_window_max_);
  time_to_full_average_.Init(sample_window_max_);
  power_supply_.Init();
  power_supply_.GetPowerStatus(&power_status_, false);
  OnPowerEvent(this, power_status_);
  UpdateAveragedTimes(&time_to_empty_average_,
                      &time_to_full_average_);
  file_tagger_.Init();
  backlight_controller_->SetObserver(this);

  std::string wakeup_inputs_str;
  std::vector<std::string> wakeup_inputs;
  if (prefs_->GetString(kWakeupInputPref, &wakeup_inputs_str))
    base::SplitString(wakeup_inputs_str, '\n', &wakeup_inputs);
  CHECK(input_->Init(wakeup_inputs));

  input_controller_->Init(prefs_);

  std::string headphone_device;
#ifdef STAY_AWAKE_PLUGGED_DEVICE
  headphone_device = STAY_AWAKE_PLUGGED_DEVICE;
#endif
  audio_detector_->Init(headphone_device);

  SetPowerState(BACKLIGHT_ACTIVE);

  bool has_lid = true;
  prefs_->GetBool(kUseLidPref, &has_lid);
  LidState lid_state =
      has_lid ? state_controller_delegate_->QueryLidState() : LID_OPEN;
  PowerSource power_source =
      plugged_state_ == PLUGGED_STATE_DISCONNECTED ? POWER_BATTERY : POWER_AC;
  state_controller_->Init(power_source, lid_state, current_session_state_,
                          DISPLAY_NORMAL);
  state_controller_initialized_ = true;

  // TODO(crosbug.com/31927): Send a signal to announce that powerd has started.
  // This is necessary for receiving external display projection status from
  // Chrome, for instance.
}

void Daemon::ReadSettings() {
  int64 low_battery_shutdown_time_s = 0;
  double low_battery_shutdown_percent = 0.0;
  if (!prefs_->GetInt64(kLowBatteryShutdownTimePref,
                        &low_battery_shutdown_time_s)) {
    LOG(INFO) << "No low battery shutdown time threshold perf found";
    low_battery_shutdown_time_s = 0;
  }
  if (!prefs_->GetDouble(kLowBatteryShutdownPercentPref,
                         &low_battery_shutdown_percent)) {
    LOG(INFO) << "No low battery shutdown percent threshold perf found";
    low_battery_shutdown_percent = 0.0;
  }
  CHECK(prefs_->GetInt64(kLowBatteryShutdownTimePref,
                         &low_battery_shutdown_time_s));
  CHECK(prefs_->GetInt64(kSampleWindowMaxPref, &sample_window_max_));
  CHECK(prefs_->GetInt64(kSampleWindowMinPref, &sample_window_min_));
  CHECK(prefs_->GetInt64(kTaperTimeMaxPref, &taper_time_max_s_));
  CHECK(prefs_->GetInt64(kTaperTimeMinPref, &taper_time_min_s_));
  CHECK(prefs_->GetInt64(kCleanShutdownTimeoutMsPref,
                         &clean_shutdown_timeout_ms_));
  CHECK(prefs_->GetInt64(kBatteryPollIntervalPref, &battery_poll_interval_ms_));
  CHECK(prefs_->GetInt64(kBatteryPollShortIntervalPref,
                         &battery_poll_short_interval_ms_));

  if ((low_battery_shutdown_time_s >= 0) &&
      (low_battery_shutdown_time_s <= 8 * 3600)) {
    low_battery_shutdown_time_s_ = low_battery_shutdown_time_s;
  } else {
    LOG(INFO) << "Unreasonable low battery shutdown time threshold:"
              << low_battery_shutdown_time_s;
    LOG(INFO) << "Disabling time based low battery shutdown.";
    low_battery_shutdown_time_s_ = 0;
  }
  if ((low_battery_shutdown_percent >= 0.0) &&
      (low_battery_shutdown_percent <= 100.0)) {
    low_battery_shutdown_percent_ = low_battery_shutdown_percent;
  } else {
    LOG(INFO) << "Unreasonable low battery shutdown percent threshold:"
              << low_battery_shutdown_percent;
    LOG(INFO) << "Disabling percent based low battery shutdown.";
    low_battery_shutdown_percent_ = 0.0;
  }

  LOG_IF(WARNING, low_battery_shutdown_percent_ == 0.0 &&
         low_battery_shutdown_time_s_ == 0) << "No low battery thresholds set!";
  // We only want one of the thresholds to be in use
  CHECK(low_battery_shutdown_percent_ == 0.0 ||
        low_battery_shutdown_time_s_ == 0)
      << "Both low battery thresholds set!";
  LOG(INFO) << "Using low battery time threshold of "
            << low_battery_shutdown_time_s_
            << " secs and using low battery percent threshold of "
            << low_battery_shutdown_percent_;

  CHECK(sample_window_max_ > 0);
  CHECK(sample_window_min_ > 0);
  if (sample_window_max_ < sample_window_min_) {
    LOG(WARNING) << "Sampling window minimum was greater then the maximum, "
                 << "swapping!";
    int64 sample_window_temp = sample_window_max_;
    sample_window_max_ = sample_window_min_;
    sample_window_min_ = sample_window_temp;
  }
  LOG(INFO) << "Using Sample Window Max = " << sample_window_max_
            << " and Min = " << sample_window_min_;
  sample_window_diff_ = sample_window_max_ - sample_window_min_;
  CHECK(taper_time_max_s_ > 0);
  CHECK(taper_time_min_s_ > 0);
  if (taper_time_max_s_ < taper_time_min_s_) {
    LOG(WARNING) << "Taper time minimum was greater then the maximum, "
                 << "swapping!";
    int64 taper_time_temp = taper_time_max_s_;
    taper_time_max_s_ = taper_time_min_s_;
    taper_time_min_s_ = taper_time_temp;
  }
  LOG(INFO) << "Using Taper Time Max(secs) = " << taper_time_max_s_
            << " and Min(secs) = " << taper_time_min_s_;
  taper_time_diff_s_ = taper_time_max_s_ - taper_time_min_s_;

  LOG(INFO) << "Using battery polling interval of " << battery_poll_interval_ms_
            << " mS and short interval of " << battery_poll_short_interval_ms_
            << " mS";

  state_control_->ReadSettings(prefs_);
}

void Daemon::Run() {
  GMainLoop* loop = g_main_loop_new(NULL, false);
  ResumePollPowerSupply();
  g_main_loop_run(loop);
}

void Daemon::UpdateIdleStates() {
  if (state_controller_initialized_) {
    state_controller_->HandleOverrideChange(
        state_control_->IsStateDisabled(STATE_CONTROL_IDLE_DIM),
        state_control_->IsStateDisabled(STATE_CONTROL_IDLE_BLANK),
        state_control_->IsStateDisabled(STATE_CONTROL_IDLE_SUSPEND),
        state_control_->IsStateDisabled(STATE_CONTROL_LID_SUSPEND));
  }
}

void Daemon::SetPlugged(bool plugged) {
  if (plugged == plugged_state_)
    return;

  LOG(INFO) << "SetPlugged: plugged=" << plugged;

  HandleNumOfSessionsPerChargeOnSetPlugged(&metrics_store_,
                                           plugged ?
                                           PLUGGED_STATE_CONNECTED :
                                           PLUGGED_STATE_DISCONNECTED);

  // If we are moving from kPowerUknown then we don't know how long the device
  // has been on AC for and thus our metric would not tell us anything about the
  // battery state when the user decided to charge.
  if (plugged_state_ != PLUGGED_STATE_UNKNOWN) {
    GenerateBatteryInfoWhenChargeStartsMetric(
        plugged ? PLUGGED_STATE_CONNECTED : PLUGGED_STATE_DISCONNECTED,
        power_status_);
  }

  plugged_state_ = plugged ? PLUGGED_STATE_CONNECTED :
      PLUGGED_STATE_DISCONNECTED;

  backlight_controller_->OnPlugEvent(plugged);

  if (state_controller_initialized_) {
    state_controller_->HandlePowerSourceChange(
        plugged ? POWER_AC : POWER_BATTERY);
  }
}

void Daemon::OnRequestRestart() {
  if (shutdown_state_ == SHUTDOWN_STATE_NONE) {
    shutdown_state_ = SHUTDOWN_STATE_RESTARTING;
    StartCleanShutdown();
  }
}

void Daemon::OnRequestShutdown() {
  if (shutdown_state_ == SHUTDOWN_STATE_NONE) {
    shutdown_state_ = SHUTDOWN_STATE_POWER_OFF;
    StartCleanShutdown();
  }
}

void Daemon::ShutdownForFailedSuspend() {
  shutdown_reason_ = kShutdownReasonSuspendFailed;
  shutdown_state_ = SHUTDOWN_STATE_POWER_OFF;
  StartCleanShutdown();
}

void Daemon::StartCleanShutdown() {
  clean_shutdown_initiated_ = true;
  suspender_.HandleShutdown();
  util::RunSetuidHelper("clean_shutdown", "", false);
  clean_shutdown_timeout_id_ = g_timeout_add(
      clean_shutdown_timeout_ms_, CleanShutdownTimedOutThunk, this);

  // If we want to display a low-battery alert while shutting down, don't turn
  // the screen off immediately.
  if (shutdown_reason_ != kShutdownReasonLowBattery) {
    backlight_controller_->SetPowerState(BACKLIGHT_SHUTTING_DOWN);
    if (keyboard_controller_)
      keyboard_controller_->SetPowerState(BACKLIGHT_SHUTTING_DOWN);
  }
}

void Daemon::OnPowerEvent(void* object, const PowerStatus& info) {
  Daemon* daemon = static_cast<Daemon*>(object);
  daemon->SetPlugged(info.line_power_on);
  daemon->GenerateMetricsOnPowerEvent(info);
  // Do not emergency suspend if no battery exists.
  if (info.battery_is_present) {
    if (info.battery_percentage < 0) {
      LOG(WARNING) << "Negative battery percent: " << info.battery_percentage
                   << "%";
    }
    if (info.battery_time_to_empty < 0 && !info.line_power_on) {
      LOG(WARNING) << "Negative battery time remaining: "
                   << info.battery_time_to_empty << " seconds";
    }
    daemon->OnLowBattery(info.battery_time_to_empty,
                         info.battery_time_to_full,
                         info.battery_percentage);
  }
}

void Daemon::IdleEventNotify(int64 threshold) {
  dbus_int64_t threshold_int =
      static_cast<dbus_int64_t>(threshold);

  chromeos::dbus::Proxy proxy(chromeos::dbus::GetSystemBusConnection(),
                              kPowerManagerServicePath,
                              kPowerManagerInterface);
  DBusMessage* signal = dbus_message_new_signal(
      kPowerManagerServicePath,
      kPowerManagerInterface,
      threshold ? kIdleNotifySignal : kActiveNotifySignal);
  CHECK(signal);
  dbus_message_append_args(signal,
                           DBUS_TYPE_INT64, &threshold_int,
                           DBUS_TYPE_INVALID);
  dbus_g_proxy_send(proxy.gproxy(), signal, NULL);
  dbus_message_unref(signal);
}

void Daemon::BrightenScreenIfOff() {
  if (util::IsSessionStarted() && backlight_controller_->IsBacklightActiveOff())
    backlight_controller_->IncreaseBrightness(BRIGHTNESS_CHANGE_AUTOMATED);
}

void Daemon::AdjustKeyboardBrightness(int direction) {
  if (!keyboard_controller_)
    return;

  if (direction > 0) {
    keyboard_controller_->IncreaseBrightness(BRIGHTNESS_CHANGE_USER_INITIATED);
  } else if (direction < 0) {
    keyboard_controller_->DecreaseBrightness(true,
                                             BRIGHTNESS_CHANGE_USER_INITIATED);
  }
}

void Daemon::SendBrightnessChangedSignal(double brightness_percent,
                                         BrightnessChangeCause cause,
                                         const std::string& signal_name) {
  dbus_int32_t brightness_percent_int =
      static_cast<dbus_int32_t>(round(brightness_percent));

  dbus_bool_t user_initiated = FALSE;
  switch (cause) {
    case BRIGHTNESS_CHANGE_AUTOMATED:
      user_initiated = FALSE;
      break;
    case BRIGHTNESS_CHANGE_USER_INITIATED:
      user_initiated = TRUE;
      break;
    default:
      NOTREACHED() << "Unhandled brightness change cause " << cause;
  }

  chromeos::dbus::Proxy proxy(chromeos::dbus::GetSystemBusConnection(),
                              kPowerManagerServicePath,
                              kPowerManagerInterface);
  DBusMessage* signal = dbus_message_new_signal(kPowerManagerServicePath,
                                                kPowerManagerInterface,
                                                signal_name.c_str());
  CHECK(signal);
  dbus_message_append_args(signal,
                           DBUS_TYPE_INT32, &brightness_percent_int,
                           DBUS_TYPE_BOOLEAN, &user_initiated,
                           DBUS_TYPE_INVALID);
  dbus_g_proxy_send(proxy.gproxy(), signal, NULL);
  dbus_message_unref(signal);
}

void Daemon::OnBrightnessChanged(double brightness_percent,
                                 BrightnessChangeCause cause,
                                 BacklightController* source) {
  if (source == backlight_controller_) {
    SendBrightnessChangedSignal(brightness_percent, cause,
                                kBrightnessChangedSignal);
  } else if (source == keyboard_controller_ && keyboard_controller_) {
    SendBrightnessChangedSignal(brightness_percent, cause,
                                kKeyboardBrightnessChangedSignal);
  } else {
    NOTREACHED() << "Received a brightness change callback from an unknown "
                 << "backlight controller";
  }
}

void Daemon::HaltPollPowerSupply() {
  util::RemoveTimeout(&poll_power_supply_timer_id_);
}

void Daemon::ResumePollPowerSupply() {
  ScheduleShortPollPowerSupply();
  EventPollPowerSupply();
}

void Daemon::MarkPowerStatusStale() {
  is_power_status_stale_ = true;
}

void Daemon::PrepareForSuspendAnnouncement() {
  // When going to suspend, notify the backlight controller so it can turn
  // the backlight off and tell the kernel to resume the current level
  // after resuming.  This must occur before Chrome is told that the system
  // is going to suspend (Chrome turns the display back on while leaving
  // the backlight off).
  SetPowerState(BACKLIGHT_SUSPENDED);
}

void Daemon::HandleCanceledSuspendAnnouncement() {
  // Undo the earlier BACKLIGHT_SUSPENDED call.
  SetPowerState(BACKLIGHT_ACTIVE);
}

void Daemon::PrepareForSuspend() {
#ifdef SUSPEND_LOCK_VT
  // Do not let suspend change the console terminal.
  util::RunSetuidHelper("lock_vt", "", true);
#endif

  power_supply_.SetSuspendState(true);
  HaltPollPowerSupply();
  MarkPowerStatusStale();
  file_tagger_.HandleSuspendEvent();
}

void Daemon::HandleResume(bool suspend_was_successful,
                          int num_suspend_retries,
                          int max_suspend_retries) {
  // Undo the earlier BACKLIGHT_SUSPENDED call.
  SetPowerState(BACKLIGHT_ACTIVE);

#ifdef SUSPEND_LOCK_VT
  // Allow virtual terminal switching again.
  util::RunSetuidHelper("unlock_vt", "", true);
#endif

  // The battery could've drained while the system was suspended; ensure
  // that we don't leave the reported percentage pinned at 100% or slowly
  // tapering down to the actual level: http://crosbug.com/38834
  battery_report_state_ = BATTERY_REPORT_ADJUSTED;

  time_to_empty_average_.Clear();
  time_to_full_average_.Clear();
  ResumePollPowerSupply();
  file_tagger_.HandleResumeEvent();
  power_supply_.SetSuspendState(false);
  state_controller_->HandleResume();
  if (suspend_was_successful)
    GenerateRetrySuspendMetric(num_suspend_retries, max_suspend_retries);
}

void Daemon::HandleLidClosed() {
  if (state_controller_initialized_) {
    state_controller_->HandleLidStateChange(LID_CLOSED);
  }
}

void Daemon::HandleLidOpened() {
  suspender_.HandleLidOpened();
  if (state_controller_initialized_)
    state_controller_->HandleLidStateChange(LID_OPEN);
}

void Daemon::EnsureBacklightIsOn() {
  // If the user manually set the brightness to 0, increase it a bit:
  // http://crosbug.com/32570
  if (backlight_controller_->IsBacklightActiveOff()) {
    backlight_controller_->IncreaseBrightness(
        BRIGHTNESS_CHANGE_USER_INITIATED);
  }
}

void Daemon::SendPowerButtonMetric(bool down, base::TimeTicks timestamp) {
  // Just keep track of the time when the button was pressed.
  if (down) {
    if (!last_power_button_down_timestamp_.is_null())
      LOG(ERROR) << "Got power-button-down event while button was already down";
    last_power_button_down_timestamp_ = timestamp;
    return;
  }

  // Metrics are sent after the button is released.
  if (last_power_button_down_timestamp_.is_null()) {
    LOG(ERROR) << "Got power-button-up event while button was already up";
    return;
  }
  base::TimeDelta delta = timestamp - last_power_button_down_timestamp_;
  if (delta.InMilliseconds() < 0) {
    LOG(ERROR) << "Negative duration between power button events";
    return;
  }
  last_power_button_down_timestamp_ = base::TimeTicks();
  if (!SendMetric(kMetricPowerButtonDownTimeName,
                  delta.InMilliseconds(),
                  kMetricPowerButtonDownTimeMin,
                  kMetricPowerButtonDownTimeMax,
                  kMetricPowerButtonDownTimeBuckets)) {
    LOG(ERROR) << "Could not send " << kMetricPowerButtonDownTimeName;
  }
}

void Daemon::OnAudioActivity(base::TimeTicks last_activity_time) {
  state_controller_->HandleAudioActivity();
}

gboolean Daemon::UdevEventHandler(GIOChannel* /* source */,
                                  GIOCondition /* condition */,
                                  gpointer data) {
  Daemon* daemon = static_cast<Daemon*>(data);

  struct udev_device* dev = udev_monitor_receive_device(daemon->udev_monitor_);
  if (dev) {
    LOG(INFO) << "Event on (" << udev_device_get_subsystem(dev) << ") Action "
              << udev_device_get_action(dev);
    CHECK(std::string(udev_device_get_subsystem(dev)) ==
          kPowerSupplyUdevSubsystem);
    udev_device_unref(dev);

    // Rescheduling the timer to fire 5s from now to make sure that it doesn't
    // get a bogus value from being too close to this event.
    daemon->ResumePollPowerSupply();
  } else {
    LOG(ERROR) << "Can't get receive_device()";
    return FALSE;
  }
  return TRUE;
}

void Daemon::RegisterUdevEventHandler() {
  // Create the udev object.
  udev_ = udev_new();
  if (!udev_)
    LOG(ERROR) << "Can't create udev object.";

  // Create the udev monitor structure.
  udev_monitor_ = udev_monitor_new_from_netlink(udev_, "udev");
  if (!udev_monitor_) {
    LOG(ERROR) << "Can't create udev monitor.";
    udev_unref(udev_);
  }
  udev_monitor_filter_add_match_subsystem_devtype(udev_monitor_,
                                                  kPowerSupplyUdevSubsystem,
                                                  NULL);
  udev_monitor_enable_receiving(udev_monitor_);

  int fd = udev_monitor_get_fd(udev_monitor_);

  GIOChannel* channel = g_io_channel_unix_new(fd);
  g_io_add_watch(channel, G_IO_IN, &(Daemon::UdevEventHandler), this);

  LOG(INFO) << "Udev controller waiting for events on subsystem "
            << kPowerSupplyUdevSubsystem;
}

void Daemon::RegisterDBusMessageHandler() {
  util::RequestDBusServiceName(kPowerManagerServiceName);

  dbus_handler_.SetNameOwnerChangedHandler(&Suspender::NameOwnerChangedHandler,
                                           &suspender_);

  dbus_handler_.AddDBusSignalHandler(
      kPowerManagerInterface,
      kCleanShutdown,
      base::Bind(&Daemon::HandleCleanShutdownSignal, base::Unretained(this)));
  dbus_handler_.AddDBusSignalHandler(
      login_manager::kSessionManagerInterface,
      login_manager::kSessionManagerSessionStateChanged,
      base::Bind(&Daemon::HandleSessionManagerSessionStateChangedSignal,
                 base::Unretained(this)));
  dbus_handler_.AddDBusSignalHandler(
      update_engine::kUpdateEngineInterface,
      update_engine::kStatusUpdate,
      base::Bind(&Daemon::HandleUpdateEngineStatusUpdateSignal,
                 base::Unretained(this)));

  dbus_handler_.AddDBusMethodHandler(
      kPowerManagerInterface,
      kRequestShutdownMethod,
      base::Bind(&Daemon::HandleRequestShutdownMethod, base::Unretained(this)));
  dbus_handler_.AddDBusMethodHandler(
      kPowerManagerInterface,
      kRequestRestartMethod,
      base::Bind(&Daemon::HandleRequestRestartMethod, base::Unretained(this)));
  dbus_handler_.AddDBusMethodHandler(
      kPowerManagerInterface,
      kRequestSuspendMethod,
      base::Bind(&Daemon::HandleRequestSuspendMethod, base::Unretained(this)));
  dbus_handler_.AddDBusMethodHandler(
      kPowerManagerInterface,
      kDecreaseScreenBrightness,
      base::Bind(&Daemon::HandleDecreaseScreenBrightnessMethod,
                 base::Unretained(this)));
  dbus_handler_.AddDBusMethodHandler(
      kPowerManagerInterface,
      kIncreaseScreenBrightness,
      base::Bind(&Daemon::HandleIncreaseScreenBrightnessMethod,
                 base::Unretained(this)));
  dbus_handler_.AddDBusMethodHandler(
      kPowerManagerInterface,
      kGetScreenBrightnessPercent,
      base::Bind(&Daemon::HandleGetScreenBrightnessMethod,
                 base::Unretained(this)));
  dbus_handler_.AddDBusMethodHandler(
      kPowerManagerInterface,
      kSetScreenBrightnessPercent,
      base::Bind(&Daemon::HandleSetScreenBrightnessMethod,
                 base::Unretained(this)));
  dbus_handler_.AddDBusMethodHandler(
      kPowerManagerInterface,
      kDecreaseKeyboardBrightness,
      base::Bind(&Daemon::HandleDecreaseKeyboardBrightnessMethod,
                 base::Unretained(this)));
  dbus_handler_.AddDBusMethodHandler(
      kPowerManagerInterface,
      kIncreaseKeyboardBrightness,
      base::Bind(&Daemon::HandleIncreaseKeyboardBrightnessMethod,
                 base::Unretained(this)));
  dbus_handler_.AddDBusMethodHandler(
      kPowerManagerInterface,
      kGetIdleTime,
      base::Bind(&Daemon::HandleGetIdleTimeMethod, base::Unretained(this)));
  dbus_handler_.AddDBusMethodHandler(
      kPowerManagerInterface,
      kRequestIdleNotification,
      base::Bind(&Daemon::HandleRequestIdleNotificationMethod,
                 base::Unretained(this)));
  dbus_handler_.AddDBusMethodHandler(
      kPowerManagerInterface,
      kGetPowerSupplyPropertiesMethod,
      base::Bind(&Daemon::HandleGetPowerSupplyPropertiesMethod,
                 base::Unretained(this)));
  dbus_handler_.AddDBusMethodHandler(
      kPowerManagerInterface,
      kStateOverrideRequest,
      base::Bind(&Daemon::HandleStateOverrideRequestMethod,
                 base::Unretained(this)));
  dbus_handler_.AddDBusMethodHandler(
      kPowerManagerInterface,
      kStateOverrideCancel,
      base::Bind(&Daemon::HandleStateOverrideCancelMethod,
                 base::Unretained(this)));
  dbus_handler_.AddDBusMethodHandler(
      kPowerManagerInterface,
      kHandleVideoActivityMethod,
      base::Bind(&Daemon::HandleVideoActivityMethod, base::Unretained(this)));
  dbus_handler_.AddDBusMethodHandler(
      kPowerManagerInterface,
      kHandleUserActivityMethod,
      base::Bind(&Daemon::HandleUserActivityMethod, base::Unretained(this)));
  dbus_handler_.AddDBusMethodHandler(
      kPowerManagerInterface,
      kSetIsProjectingMethod,
      base::Bind(&Daemon::HandleSetIsProjectingMethod, base::Unretained(this)));
  dbus_handler_.AddDBusMethodHandler(
      kPowerManagerInterface,
      kSetPolicyMethod,
      base::Bind(&Daemon::HandleSetPolicyMethod, base::Unretained(this)));
  dbus_handler_.AddDBusMethodHandler(
      kPowerManagerInterface,
      kRegisterSuspendDelayMethod,
      base::Bind(&Suspender::RegisterSuspendDelay,
                 base::Unretained(&suspender_)));
  dbus_handler_.AddDBusMethodHandler(
      kPowerManagerInterface,
      kUnregisterSuspendDelayMethod,
      base::Bind(&Suspender::UnregisterSuspendDelay,
                 base::Unretained(&suspender_)));
  dbus_handler_.AddDBusMethodHandler(
      kPowerManagerInterface,
      kHandleSuspendReadinessMethod,
      base::Bind(&Suspender::HandleSuspendReadiness,
                 base::Unretained(&suspender_)));

  dbus_handler_.Start();
}

bool Daemon::HandleCleanShutdownSignal(DBusMessage*) {  // NOLINT
  if (clean_shutdown_initiated_) {
    clean_shutdown_initiated_ = false;
    Shutdown();
  } else {
    LOG(WARNING) << "Unrequested " << kCleanShutdown << " signal";
  }
  return true;
}

bool Daemon::HandleSessionManagerSessionStateChangedSignal(
    DBusMessage* message) {
  DBusError error;
  dbus_error_init(&error);
  const char* state = NULL;
  const char* user = NULL;
  if (dbus_message_get_args(message, &error,
                            DBUS_TYPE_STRING, &state,
                            DBUS_TYPE_STRING, &user,
                            DBUS_TYPE_INVALID)) {
    OnSessionStateChange(state ? state : "", user ? user : "");
  } else {
    LOG(WARNING) << "Unable to read "
                 << login_manager::kSessionManagerSessionStateChanged
                 << " args";
    dbus_error_free(&error);
  }
  return false;
}

bool Daemon::HandleUpdateEngineStatusUpdateSignal(DBusMessage* message) {
  DBusError error;
  dbus_error_init(&error);

  dbus_int64_t last_checked_time = 0;
  double progress = 0.0;
  const char* current_operation = NULL;
  const char* new_version = NULL;
  dbus_int64_t new_size = 0;

  if (!dbus_message_get_args(message, &error,
                             DBUS_TYPE_INT64, &last_checked_time,
                             DBUS_TYPE_DOUBLE, &progress,
                             DBUS_TYPE_STRING, &current_operation,
                             DBUS_TYPE_STRING, &new_version,
                             DBUS_TYPE_INT64, &new_size,
                             DBUS_TYPE_INVALID)) {
    LOG(WARNING) << "Unable to read args from " << update_engine::kStatusUpdate
                 << " signal";
    dbus_error_free(&error);
    return false;
  }

  // TODO: Use shared constants instead: http://crosbug.com/39706
  UpdaterState state = UPDATER_IDLE;
  std::string operation = current_operation;
  if (operation == "UPDATE_STATUS_DOWNLOADING" ||
      operation == "UPDATE_STATUS_VERIFYING" ||
      operation == "UPDATE_STATUS_FINALIZING") {
    state = UPDATER_UPDATING;
  } else if (operation == "UPDATE_STATUS_UPDATED_NEED_REBOOT") {
    state = UPDATER_UPDATED;
  }
  state_controller_->HandleUpdaterStateChange(state);

  return false;
}

DBusMessage* Daemon::HandleRequestShutdownMethod(DBusMessage* message) {
  shutdown_reason_ = kShutdownReasonUserRequest;
  OnRequestShutdown();
  return NULL;
}

DBusMessage* Daemon::HandleRequestRestartMethod(DBusMessage* message) {
  OnRequestRestart();
  return NULL;
}

DBusMessage* Daemon::HandleRequestSuspendMethod(DBusMessage* message) {
  Suspend();
  return NULL;
}

DBusMessage* Daemon::HandleDecreaseScreenBrightnessMethod(
    DBusMessage* message) {
  dbus_bool_t allow_off = false;
  DBusError error;
  dbus_error_init(&error);
  if (dbus_message_get_args(message, &error,
                            DBUS_TYPE_BOOLEAN, &allow_off,
                            DBUS_TYPE_INVALID) == FALSE) {
    LOG(WARNING) << "Unable to read " << kDecreaseScreenBrightness << " args";
    dbus_error_free(&error);
  }
  bool changed = backlight_controller_->DecreaseBrightness(
      allow_off, BRIGHTNESS_CHANGE_USER_INITIATED);
  SendEnumMetricWithPowerState(kMetricBrightnessAdjust,
                               BRIGHTNESS_ADJUST_DOWN,
                               BRIGHTNESS_ADJUST_MAX);
  if (!changed) {
    SendBrightnessChangedSignal(
        backlight_controller_->GetTargetBrightnessPercent(),
        BRIGHTNESS_CHANGE_USER_INITIATED,
        kBrightnessChangedSignal);
  }
  return NULL;
}

DBusMessage* Daemon::HandleIncreaseScreenBrightnessMethod(
    DBusMessage* message) {
  bool changed = backlight_controller_->IncreaseBrightness(
      BRIGHTNESS_CHANGE_USER_INITIATED);
  SendEnumMetricWithPowerState(kMetricBrightnessAdjust,
                               BRIGHTNESS_ADJUST_UP,
                               BRIGHTNESS_ADJUST_MAX);
  if (!changed) {
    SendBrightnessChangedSignal(
        backlight_controller_->GetTargetBrightnessPercent(),
        BRIGHTNESS_CHANGE_USER_INITIATED,
        kBrightnessChangedSignal);
  }
  return NULL;
}

DBusMessage* Daemon::HandleSetScreenBrightnessMethod(DBusMessage* message) {
  double percent;
  int dbus_style;
  DBusError error;
  dbus_error_init(&error);
  if (!dbus_message_get_args(message, &error,
                             DBUS_TYPE_DOUBLE, &percent,
                             DBUS_TYPE_INT32, &dbus_style,
                             DBUS_TYPE_INVALID)) {
    LOG(WARNING) << kSetScreenBrightnessPercent
                << ": Error reading args: " << error.message;
    dbus_error_free(&error);
    return util::CreateDBusInvalidArgsErrorReply(message);
  }
  TransitionStyle style = TRANSITION_FAST;
  switch (dbus_style) {
    case kBrightnessTransitionGradual:
      style = TRANSITION_FAST;
      break;
    case kBrightnessTransitionInstant:
      style = TRANSITION_INSTANT;
      break;
    default:
      LOG(WARNING) << "Invalid transition style passed ( " << dbus_style
                  << " ).  Using default fast transition";
  }
  backlight_controller_->SetCurrentBrightnessPercent(
      percent, BRIGHTNESS_CHANGE_USER_INITIATED, style);
  SendEnumMetricWithPowerState(kMetricBrightnessAdjust,
                               BRIGHTNESS_ADJUST_ABSOLUTE,
                               BRIGHTNESS_ADJUST_MAX);
  return NULL;
}

DBusMessage* Daemon::HandleGetScreenBrightnessMethod(DBusMessage* message) {
  double percent;
  if (!backlight_controller_->GetCurrentBrightnessPercent(&percent)) {
    return util::CreateDBusErrorReply(
        message, "Could not fetch Screen Brightness");
  }
  DBusMessage* reply = util::CreateEmptyDBusReply(message);
  CHECK(reply);
  dbus_message_append_args(reply,
                           DBUS_TYPE_DOUBLE, &percent,
                           DBUS_TYPE_INVALID);
  return reply;
}

DBusMessage* Daemon::HandleDecreaseKeyboardBrightnessMethod(
    DBusMessage* message) {
  AdjustKeyboardBrightness(-1);
  // TODO(dianders): metric?
  return NULL;
}

DBusMessage* Daemon::HandleIncreaseKeyboardBrightnessMethod(
    DBusMessage* message) {
  AdjustKeyboardBrightness(1);
  // TODO(dianders): metric?
  return NULL;
}

DBusMessage* Daemon::HandleGetIdleTimeMethod(DBusMessage* message) {
  int64 idle_time_ms = 0;
  base::TimeDelta interval =
      base::TimeTicks::Now() - state_controller_->last_user_activity_time();
  idle_time_ms = interval.InMilliseconds();

  DBusMessage* reply = util::CreateEmptyDBusReply(message);
  CHECK(reply);
  dbus_message_append_args(reply,
                           DBUS_TYPE_INT64, &idle_time_ms,
                           DBUS_TYPE_INVALID);
  return reply;
}

DBusMessage* Daemon::HandleRequestIdleNotificationMethod(DBusMessage* message) {
  DBusError error;
  dbus_error_init(&error);
  int64 threshold = 0;
  if (dbus_message_get_args(message, &error,
                            DBUS_TYPE_INT64, &threshold,
                            DBUS_TYPE_INVALID)) {
    state_controller_->AddIdleNotification(
        base::TimeDelta::FromMilliseconds(threshold));
  } else {
    LOG(WARNING) << "Unable to read " << kRequestIdleNotification << " args";
    dbus_error_free(&error);
  }
  return NULL;
}

DBusMessage* Daemon::HandleGetPowerSupplyPropertiesMethod(
    DBusMessage* message) {
  if (is_power_status_stale_) {
    // Poll the power supply for status, but don't clear the stale bit. This
    // case is an exceptional one, so we can't guarantee we want to start
    // polling again yet from this context. The stale bit should only be set
    // near the beginning of a session or around Suspend/Resume, so we are
    // assuming that the battery time is untrustworthy, hence the
    // |is_calculating| is true.
    power_supply_.GetPowerStatus(&power_status_, true);
    HandlePollPowerSupply();
    is_power_status_stale_ = true;
  }

  PowerSupplyProperties protobuf;
  PowerStatus* status = &power_status_;

  protobuf.set_line_power_on(status->line_power_on);
  protobuf.set_battery_energy(status->battery_energy);
  protobuf.set_battery_energy_rate(status->battery_energy_rate);
  protobuf.set_battery_voltage(status->battery_voltage);
  protobuf.set_battery_time_to_empty(status->battery_time_to_empty);
  protobuf.set_battery_time_to_full(status->battery_time_to_full);
  UpdateBatteryReportState();
  protobuf.set_battery_percentage(GetDisplayBatteryPercent());
  protobuf.set_battery_is_present(status->battery_is_present);
  protobuf.set_battery_is_charged(status->battery_state ==
                                  BATTERY_STATE_FULLY_CHARGED);
  protobuf.set_is_calculating_battery_time(status->is_calculating_battery_time);
  protobuf.set_averaged_battery_time_to_empty(
      status->averaged_battery_time_to_empty);
  protobuf.set_averaged_battery_time_to_full(
      status->averaged_battery_time_to_full);

  return util::CreateDBusProtocolBufferReply(message, protobuf);
}

DBusMessage* Daemon::HandleStateOverrideRequestMethod(DBusMessage* message) {
  PowerStateControl protobuf;
  int return_value = 0;
  if (util::ParseProtocolBufferFromDBusMessage(message, &protobuf) &&
      state_control_->StateOverrideRequest(protobuf, &return_value)) {
    state_controller_->HandleOverrideChange(
        state_control_->IsStateDisabled(STATE_CONTROL_IDLE_DIM),
        state_control_->IsStateDisabled(STATE_CONTROL_IDLE_BLANK),
        state_control_->IsStateDisabled(STATE_CONTROL_IDLE_SUSPEND),
        state_control_->IsStateDisabled(STATE_CONTROL_LID_SUSPEND));
    DBusMessage* reply = util::CreateEmptyDBusReply(message);
    dbus_message_append_args(reply,
                             DBUS_TYPE_INT32, &return_value,
                             DBUS_TYPE_INVALID);
    return reply;
  }
  return util::CreateDBusErrorReply(message, "Failed processing request");
}

DBusMessage* Daemon::HandleStateOverrideCancelMethod(DBusMessage* message) {
  DBusError error;
  dbus_error_init(&error);
  int request_id;
  if (dbus_message_get_args(message, &error,
                            DBUS_TYPE_INT32, &request_id,
                            DBUS_TYPE_INVALID)) {
    state_control_->RemoveOverrideAndUpdate(request_id);
  } else {
    LOG(WARNING) << kStateOverrideCancel << ": Error reading args: "
                 << error.message;
    dbus_error_free(&error);
    return util::CreateDBusInvalidArgsErrorReply(message);
  }
  return NULL;
}

DBusMessage* Daemon::HandleVideoActivityMethod(DBusMessage* message) {
  VideoActivityUpdate protobuf;
  if (!util::ParseProtocolBufferFromDBusMessage(message, &protobuf))
    return util::CreateDBusInvalidArgsErrorReply(message);

  if (keyboard_controller_) {
    keyboard_controller_->HandleVideoActivity(
        base::TimeTicks::FromInternalValue(protobuf.last_activity_time()),
        protobuf.is_fullscreen());
  }
  state_controller_->HandleVideoActivity();
  return NULL;
}

DBusMessage* Daemon::HandleUserActivityMethod(DBusMessage* message) {
  DBusError error;
  dbus_error_init(&error);
  int64 last_activity_time_internal = 0;
  if (!dbus_message_get_args(message, &error,
                             DBUS_TYPE_INT64, &last_activity_time_internal,
                             DBUS_TYPE_INVALID)) {
    LOG(WARNING) << kHandleUserActivityMethod
                << ": Error reading args: " << error.message;
    dbus_error_free(&error);
    return util::CreateDBusInvalidArgsErrorReply(message);
  }
  suspender_.HandleUserActivity();
  state_controller_->HandleUserActivity();
  return NULL;
}

DBusMessage* Daemon::HandleSetIsProjectingMethod(DBusMessage* message) {
  DBusMessage* reply = NULL;
  DBusError error;
  dbus_error_init(&error);
  bool is_projecting = false;
  dbus_bool_t args_ok =
      dbus_message_get_args(message, &error,
                            DBUS_TYPE_BOOLEAN, &is_projecting,
                            DBUS_TYPE_INVALID);
  if (args_ok) {
    state_controller_->HandleDisplayModeChange(
        is_projecting ? DISPLAY_PRESENTATION : DISPLAY_NORMAL);
  } else {
    // The message was malformed so log this and return an error.
    LOG(WARNING) << kSetIsProjectingMethod << ": Error reading args: "
                 << error.message;
    reply = util::CreateDBusInvalidArgsErrorReply(message);
  }
  return reply;
}

DBusMessage* Daemon::HandleSetPolicyMethod(DBusMessage* message) {
  PowerManagementPolicy policy;
  if (!util::ParseProtocolBufferFromDBusMessage(message, &policy)) {
    LOG(WARNING) << "Unable to parse " << kSetPolicyMethod << " request";
    return util::CreateDBusInvalidArgsErrorReply(message);
  }
  state_controller_->HandlePolicyChange(policy);
  return NULL;
}

void Daemon::ScheduleShortPollPowerSupply() {
  HaltPollPowerSupply();
  poll_power_supply_timer_id_ = g_timeout_add(battery_poll_short_interval_ms_,
                                              ShortPollPowerSupplyThunk,
                                              this);
}

void Daemon::SchedulePollPowerSupply() {
  HaltPollPowerSupply();
  poll_power_supply_timer_id_ = g_timeout_add(battery_poll_interval_ms_,
                                              PollPowerSupplyThunk,
                                              this);
}

gboolean Daemon::EventPollPowerSupply() {
  power_supply_.GetPowerStatus(&power_status_, true);
  return HandlePollPowerSupply();
}

gboolean Daemon::ShortPollPowerSupply() {
  SchedulePollPowerSupply();
  power_supply_.GetPowerStatus(&power_status_, false);
  HandlePollPowerSupply();
  return false;
}

gboolean Daemon::PollPowerSupply() {
  power_supply_.GetPowerStatus(&power_status_, false);
  return HandlePollPowerSupply();
}

gboolean Daemon::HandlePollPowerSupply() {
  OnPowerEvent(this, power_status_);
  if (!UpdateAveragedTimes(&time_to_empty_average_,
                           &time_to_full_average_)) {
    LOG(ERROR) << "Unable to get averaged times!";
    ScheduleShortPollPowerSupply();
    return FALSE;
  }

  // Send a signal once the power supply status has been obtained.
  DBusMessage* message = dbus_message_new_signal(kPowerManagerServicePath,
                                                 kPowerManagerInterface,
                                                 kPowerSupplyPollSignal);
  CHECK(message != NULL);
  DBusConnection* connection = dbus_g_connection_get_connection(
      chromeos::dbus::GetSystemBusConnection().g_connection());
  if (!dbus_connection_send(connection, message, NULL))
    LOG(WARNING) << "Sending battery poll signal failed.";
  dbus_message_unref(message);
  is_power_status_stale_ = false;
  // Always repeat polling.
  return TRUE;
}

bool Daemon::UpdateAveragedTimes(RollingAverage* empty_average,
                                 RollingAverage* full_average) {
  SendEnumMetric(kMetricBatteryInfoSampleName,
                 BATTERY_INFO_READ,
                 BATTERY_INFO_MAX);
  // Some devices give us bogus values for battery information right after boot
  // or a power event. We attempt to avoid to do sampling at these times, but
  // this guard is to save us when we do sample a bad value. After working out
  // the time values, if we have a negative we know something is bad. If the
  // time we are interested in (to empty or full) is beyond a day then we have a
  // bad value since it is too high. For some devices the value for the
  // uninteresting time, that we are not using, might be bizarre, so we cannot
  // just check both times for overly high values.
  if (power_status_.battery_time_to_empty < 0 ||
      power_status_.battery_time_to_full < 0 ||
      (power_status_.battery_time_to_empty > kBatteryTimeMaxValidSec &&
       !power_status_.line_power_on) ||
      (power_status_.battery_time_to_full > kBatteryTimeMaxValidSec &&
       power_status_.line_power_on)) {
    LOG(ERROR) << "Invalid raw times, time to empty = "
               << power_status_.battery_time_to_empty << ", and time to full = "
               << power_status_.battery_time_to_full;
    power_status_.averaged_battery_time_to_empty = 0;
    power_status_.averaged_battery_time_to_full = 0;
    power_status_.is_calculating_battery_time = true;
    SendEnumMetric(kMetricBatteryInfoSampleName,
                   BATTERY_INFO_BAD,
                   BATTERY_INFO_MAX);
    return false;
  }
  SendEnumMetric(kMetricBatteryInfoSampleName,
                 BATTERY_INFO_GOOD,
                 BATTERY_INFO_MAX);

  int64 battery_time = 0;
  if (power_status_.line_power_on) {
    battery_time = power_status_.battery_time_to_full;
    if (!power_status_.is_calculating_battery_time)
      full_average->AddSample(battery_time);
    empty_average->Clear();
  } else {
    // If the time threshold is set use it, otherwise determine the time
    // equivalent of the percentage threshold
    int64 time_threshold_s = low_battery_shutdown_time_s_ ?
        low_battery_shutdown_time_s_ :
        power_status_.battery_time_to_empty
        * (low_battery_shutdown_percent_
           / power_status_.battery_percentage);
    battery_time = power_status_.battery_time_to_empty - time_threshold_s;
    LOG_IF(WARNING, battery_time < 0)
        << "Calculated invalid negative time to empty value, trimming to 0!";
    battery_time = std::max(static_cast<int64>(0), battery_time);
    if (!power_status_.is_calculating_battery_time)
      empty_average->AddSample(battery_time);
    full_average->Clear();
  }

  if (!power_status_.is_calculating_battery_time) {
    if (!power_status_.line_power_on)
      AdjustWindowSize(battery_time,
                       empty_average,
                       full_average);
    else
      empty_average->ChangeWindowSize(sample_window_max_);
  }
  power_status_.averaged_battery_time_to_full =
      full_average->GetAverage();
  power_status_.averaged_battery_time_to_empty =
      empty_average->GetAverage();
  return true;
}

// For the rolling averages we want the window size to taper off in a linear
// fashion from |sample_window_max| to |sample_window_min| on the battery time
// remaining interval from |taper_time_max_s_| to |taper_time_min_s_|. The two
// point equation for the line is:
//   (x - x0)/(x1 - x0) = (t - t0)/(t1 - t0)
// which solved for x is:
//   x = (t - t0)*(x1 - x0)/(t1 - t0) + x0
// We let x be the size of the window and t be the battery time
// remaining.
void Daemon::AdjustWindowSize(int64 battery_time,
                              RollingAverage* empty_average,
                              RollingAverage* full_average) {
  unsigned int window_size = sample_window_max_;
  if (battery_time >= taper_time_max_s_) {
    window_size = sample_window_max_;
  } else if (battery_time <= taper_time_min_s_) {
    window_size = sample_window_min_;
  } else {
    window_size = (battery_time - taper_time_min_s_);
    window_size *= sample_window_diff_;
    window_size /= taper_time_diff_s_;
    window_size += sample_window_min_;
  }
  empty_average->ChangeWindowSize(window_size);
}

void Daemon::OnLowBattery(int64 time_remaining_s,
                          int64 time_full_s,
                          double battery_percentage) {
  if (!low_battery_shutdown_time_s_ && !low_battery_shutdown_percent_) {
    LOG(INFO) << "Battery time remaining : "
              << time_remaining_s << " seconds";
    low_battery_ = false;
    return;
  }

  // TODO(derat): Figure out what's causing http://crosbug.com/38912.
  if (battery_percentage < 0.001) {
    LOG(WARNING) << "Ignoring probably-bogus zero battery percentage "
                 << "(time-to-empty is " << time_remaining_s << " sec, "
                 << "time-to-full is " << time_full_s << " sec)";
    return;
  }

  if (PLUGGED_STATE_DISCONNECTED == plugged_state_ && !low_battery_ &&
      ((time_remaining_s <= low_battery_shutdown_time_s_ &&
        time_remaining_s > 0) ||
       (battery_percentage <= low_battery_shutdown_percent_ &&
        battery_percentage >= 0))) {
    // Shut the system down when low battery condition is encountered.
    LOG(INFO) << "Time remaining: " << time_remaining_s << " seconds.";
    LOG(INFO) << "Percent remaining: " << battery_percentage << "%.";
    LOG(INFO) << "Low battery condition detected. Shutting down immediately.";
    low_battery_ = true;
    file_tagger_.HandleLowBatteryEvent();
    shutdown_reason_ = kShutdownReasonLowBattery;
    OnRequestShutdown();
  } else if (time_remaining_s < 0) {
    LOG(INFO) << "Battery is at " << time_remaining_s << " seconds remaining, "
              << "may not be fully initialized yet.";
  } else if (PLUGGED_STATE_CONNECTED == plugged_state_ ||
             time_remaining_s > low_battery_shutdown_time_s_) {
    if (PLUGGED_STATE_CONNECTED == plugged_state_) {
      LOG(INFO) << "Battery condition is safe (" << battery_percentage
                << "%).  AC is plugged.  "
                << time_full_s << " seconds to full charge.";
    } else {
      LOG(INFO) << "Battery condition is safe (" << battery_percentage
                << "%).  AC is unplugged.  "
                << time_remaining_s << " seconds remaining.";
    }
    low_battery_ = false;
    file_tagger_.HandleSafeBatteryEvent();
  } else if (time_remaining_s == 0) {
    LOG(INFO) << "Battery is at 0 seconds remaining, either we are charging or "
              << "not fully initialized yet.";
  } else {
    // Either a spurious reading after we have requested suspend, or the user
    // has woken the system up intentionally without rectifying the battery
    // situation (ie. user woke the system without attaching AC.)
    // User is on his own from here until the system dies. We will not try to
    // resuspend.
    LOG(INFO) << "Spurious low battery condition, or user living on the edge.";
    file_tagger_.HandleLowBatteryEvent();
  }
}

gboolean Daemon::CleanShutdownTimedOut() {
  if (clean_shutdown_initiated_) {
    clean_shutdown_initiated_ = false;
    LOG(INFO) << "Timed out waiting for clean shutdown/restart.";
    Shutdown();
  } else {
    LOG(INFO) << "Shutdown already handled. clean_shutdown_initiated_ == false";
  }
  clean_shutdown_timeout_id_ = 0;
  return FALSE;
}

void Daemon::OnSessionStateChange(const std::string& state_str,
                                  const std::string& user) {
  SessionState state = (state_str == kSessionStarted) ?
      SESSION_STARTED : SESSION_STOPPED;

  if (state == SESSION_STARTED) {
    DLOG(INFO) << "Session started for "
               << (user.empty() ? "guest" : "non-guest user");

    // We always want to take action even if we were already "started", since we
    // want to record when the current session started.  If this warning is
    // appearing it means either we are querying the state of Session Manager
    // when already know it to be "started" or we missed a "stopped"
    // signal. Both of these cases are bad and should be investigated.
    LOG_IF(WARNING, (current_session_state_ == state))
        << "Received message saying session started, when we were already in "
        << "the started state!";

    LOG_IF(ERROR,
           (!GenerateBatteryRemainingAtStartOfSessionMetric(power_status_)))
        << "Start Started: Unable to generate battery remaining metric!";

    if (plugged_state_ == PLUGGED_STATE_DISCONNECTED)
      metrics_store_.IncrementNumOfSessionsPerChargeMetric();

    current_user_ = user;
    session_start_ = base::TimeTicks::Now();

    // Sending up the PowerSupply information, so that the display gets it as
    // soon as possible
    ResumePollPowerSupply();
  } else if (current_session_state_ != state) {
    DLOG(INFO) << "Session " << state_str;
    // For states other then "started" we only want to take action if we have
    // actually changed state, since the code we are calling assumes that we are
    // actually transitioning between states.
    current_user_.clear();
    if (state_str == kSessionStopped) {
      // Don't generate metrics for intermediate states, e.g. "stopping".
      GenerateEndOfSessionMetrics(power_status_,
                                  *backlight_controller_,
                                  base::TimeTicks::Now(),
                                  session_start_);
    }
  }

  current_session_state_ = state;

  if (state_controller_initialized_)
    state_controller_->HandleSessionStateChange(state);

  // If the backlight was manually turned off by the user, turn it back on.
  EnsureBacklightIsOn();
}

void Daemon::Shutdown() {
  if (shutdown_state_ == SHUTDOWN_STATE_POWER_OFF) {
    LOG(INFO) << "Shutting down, reason: " << shutdown_reason_;
    util::RunSetuidHelper(
        "shutdown", "--shutdown_reason=" + shutdown_reason_, false);
  } else if (shutdown_state_ == SHUTDOWN_STATE_RESTARTING) {
    LOG(INFO) << "Restarting";
    util::RunSetuidHelper("reboot", "", false);
  } else {
    LOG(ERROR) << "Shutdown : Improper System State!";
  }
}

void Daemon::Suspend() {
  if (clean_shutdown_initiated_) {
    LOG(INFO) << "Ignoring request for suspend with outstanding shutdown.";
    return;
  }
  suspender_.RequestSuspend();
}

void Daemon::RetrieveSessionState() {
  std::string state;
  std::string user;
  if (!util::GetSessionState(&state, &user))
    return;
  LOG(INFO) << "Retrieved session state of " << state;
  OnSessionStateChange(state, user);
}

void Daemon::SetPowerState(PowerState state) {
  backlight_controller_->SetPowerState(state);
  if (keyboard_controller_)
    keyboard_controller_->SetPowerState(state);
}

void Daemon::UpdateBatteryReportState() {
  switch (power_status_.battery_state) {
    case BATTERY_STATE_FULLY_CHARGED:
      battery_report_state_ = BATTERY_REPORT_FULL;
      break;
    case BATTERY_STATE_DISCHARGING:
      switch (battery_report_state_) {
        case BATTERY_REPORT_FULL:
          battery_report_state_ = BATTERY_REPORT_PINNED;
          battery_report_pinned_start_ = base::TimeTicks::Now();
          break;
        case BATTERY_REPORT_TAPERED:
          {
            int64 tapered_delta_ms =
                (base::TimeTicks::Now() -
                 battery_report_tapered_start_).InMilliseconds();
            if (tapered_delta_ms >= kBatteryPercentTaperMs)
              battery_report_state_ = BATTERY_REPORT_ADJUSTED;
          }
          break;
        case BATTERY_REPORT_PINNED:
          if ((base::TimeTicks::Now() -
               battery_report_pinned_start_).InMilliseconds()
              >= kBatteryPercentPinMs) {
            battery_report_state_ = BATTERY_REPORT_TAPERED;
            battery_report_tapered_start_ = base::TimeTicks::Now();
          }
          break;
        default:
          break;
      }
      break;
    default:
      battery_report_state_ = BATTERY_REPORT_ADJUSTED;
      break;
  }
}

double Daemon::GetDisplayBatteryPercent() const {
  double battery_percentage = GetUsableBatteryPercent();
  switch (power_status_.battery_state) {
    case BATTERY_STATE_FULLY_CHARGED:
      battery_percentage = 100.0;
      break;
    case BATTERY_STATE_DISCHARGING:
      switch (battery_report_state_) {
        case BATTERY_REPORT_FULL:
        case BATTERY_REPORT_PINNED:
          battery_percentage = 100.0;
          break;
        case BATTERY_REPORT_TAPERED:
          {
            int64 tapered_delta_ms =
                (base::TimeTicks::Now() -
                 battery_report_tapered_start_).InMilliseconds();
            double elapsed_fraction =
                std::min(1.0, (static_cast<double>(tapered_delta_ms) /
                               kBatteryPercentTaperMs));
            battery_percentage = battery_percentage + (1.0 - elapsed_fraction) *
                (100.0 - battery_percentage);
          }
          break;
        default:
          break;
      }
      break;
    default:
      break;
  }
  return battery_percentage;
}

double Daemon::GetUsableBatteryPercent() const {
  // If we are using a percentage based threshold adjust the reported percentage
  // to account for the bit being trimmed off. If we are using a time-based
  // threshold don't adjust the reported percentage. Adjusting the percentage
  // due to a time threshold might break the monoticity of percentages since the
  // time to empty/full is not guaranteed to be monotonic.
  if (power_status_.battery_percentage <= low_battery_shutdown_percent_) {
    return  0.0;
  } else if (power_status_.battery_percentage > 100.0) {
    LOG(WARNING) << "Before adjustment battery percentage was over 100%";
    return 100.0;
  } else if (low_battery_shutdown_time_s_) {
    return power_status_.battery_percentage;
  } else {  // Using percentage threshold
    // x = current percentage
    // y = adjusted percentage
    // t = threshold percentage
    // y = 100 *(x-t)/(100 - t)
    double battery_percentage = 100.0 * (power_status_.battery_percentage
                                         - low_battery_shutdown_percent_);
    battery_percentage /= 100.0 - low_battery_shutdown_percent_;
    return battery_percentage;
  }
}

}  // namespace power_manager
