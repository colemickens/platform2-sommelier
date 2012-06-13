// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd.h"

#include <glib-object.h>
#include <stdint.h>
#include <sys/inotify.h>
#include <X11/Xlib.h>
#include <X11/extensions/dpms.h>

#include <algorithm>
#include <cmath>
#include <set>
#include <vector>

#include "base/file_util.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "chromeos/dbus/dbus.h"
#include "chromeos/dbus/service_constants.h"
#include "chromeos/glib/object.h"
#include "cros/chromeos_wm_ipc_enums.h"
#include "power_manager/activity_detector_interface.h"
#include "power_manager/backlight_controller.h"
#include "power_manager/metrics_constants.h"
#include "power_manager/monitor_reconfigure.h"
#include "power_manager/power_constants.h"
#include "power_manager/power_supply.h"
#include "power_manager/state_control.h"
#include "power_manager/util.h"
#include "power_manager/util_dbus.h"
#include "power_supply_properties.pb.h"

using std::map;
using std::max;
using std::min;
using std::string;
using std::vector;

namespace {

// Path for storing FileTagger files.
const char kTaggedFilePath[] = "/var/lib/power_manager";

// Path to power supply info.
const char kPowerStatusPath[] = "/sys/class/power_supply";

// Power supply subsystem for udev events.
const char kPowerSupplyUdevSubsystem[] = "power_supply";

// Time between battery polls, in milliseconds.
const int64 kBatteryPollIntervalMs = 30000;

// Time between battery event and next poll, in milliseconds.
const int64 kBatteryPollShortIntervalMs = 5000;

// How frequently audio should be checked before suspending.
const int64 kAudioCheckIntervalMs = 1000;

// Valid string values for the state value of Session Manager
const std::string kValidStateStrings[] = { "started", "stopping", "stopped" };

// Set of valid state strings for easy sanity testing
std::set<std::string> kValidStates(
    kValidStateStrings,
    kValidStateStrings  + sizeof(kValidStateStrings) / sizeof(std::string));

// Minimum time a user must be idle to have returned from idle
const int64 kMinTimeForIdle = 10;

// The current normal sample rate is 0.5 Hz, so this gives us a window of ~5
// minutes.
const unsigned int kRollingAverageSampleWindow = 10;

}  // namespace

namespace power_manager {

// Timeouts are multiplied by this factor when projecting to external display.
static const int64 kProjectionTimeoutFactor = 2;

// Constants for brightness adjustment metric reporting.
enum {
  kBrightnessDown,
  kBrightnessUp,
  kBrightnessAbsolute,
  kBrightnessEnumMax ,
};

// Daemon: Main power manager. Adjusts device status based on whether the
//         user is idle and on video activity indicator from Chrome.
//         This daemon is responsible for dimming of the backlight, turning
//         the screen off, and suspending to RAM. The daemon also has the
//         capability of shutting the system down.

Daemon::Daemon(BacklightController* backlight_controller,
               PowerPrefs* prefs,
               MetricsLibraryInterface* metrics_lib,
               ActivityDetectorInterface* video_detector,
               ActivityDetectorInterface* audio_detector,
               IdleDetector* idle,
               MonitorReconfigure* monitor_reconfigure,
               BacklightInterface* keyboard_backlight,
               const FilePath& run_dir)
    : backlight_controller_(backlight_controller),
      prefs_(prefs),
      metrics_lib_(metrics_lib),
      video_detector_(video_detector),
      audio_detector_(audio_detector),
      idle_(idle),
      monitor_reconfigure_(monitor_reconfigure),
      keyboard_backlight_(keyboard_backlight),
      low_battery_suspend_percent_(0),
      clean_shutdown_initiated_(false),
      low_battery_(false),
      enforce_lock_(false),
      use_xscreensaver_(false),
      plugged_state_(kPowerUnknown),
      file_tagger_(FilePath(kTaggedFilePath)),
      shutdown_state_(kShutdownNone),
      suspender_(&locker_, &file_tagger_),
      run_dir_(run_dir),
      power_supply_(FilePath(kPowerStatusPath)),
      battery_discharge_rate_metric_last_(0),
      current_session_state_("stopped"),
      udev_(NULL),
      state_control_(new StateControl(this)),
      poll_power_supply_timer_id_(0) {}

Daemon::~Daemon() {
  if (udev_)
    udev_unref(udev_);
}

void Daemon::Init() {
  ReadSettings();
  idle_->Init(this);
  prefs_->StartPrefWatching(&(PrefChangeHandler), this);
  MetricInit();
  LOG_IF(ERROR, (!metrics_store_.Init()))
      << "Unable to initialize metrics store, so we are going to drop number of"
      << " sessions per charge data";

  Display* display = util::GetDisplay();
  if (!DPMSCapable(display)) {
    LOG(WARNING) << "X Server not DPMS capable";
  } else {
    CHECK(DPMSEnable(display));
    CHECK(DPMSSetTimeouts(display, 0, 0, 0));
  }
  CHECK(XSetScreenSaver(display,
                        0,                 // 0 display timeout
                        0,                 // 0 alteration timeout
                        DefaultBlanking,
                        DefaultExposures));
  locker_.Init(use_xscreensaver_, lock_on_idle_suspend_);
  RegisterUdevEventHandler();
  RegisterDBusMessageHandler();
  RetrieveSessionState();
  suspender_.Init(run_dir_);
  time_to_empty_average_.Init(kRollingAverageSampleWindow);
  time_to_full_average_.Init(kRollingAverageSampleWindow);
  power_supply_.Init();
  power_supply_.GetPowerStatus(&power_status_, false);
  OnPowerEvent(this, power_status_);
  UpdateAveragedTimes(&power_status_,
                      &time_to_empty_average_,
                      &time_to_full_average_);
  file_tagger_.Init();
  backlight_controller_->SetObserver(this);
  monitor_reconfigure_->SetProjectionCallback(
      &AdjustIdleTimeoutsForProjectionThunk, this);
}

void Daemon::ReadSettings() {
  int64 use_xscreensaver, enforce_lock;
  int64 disable_idle_suspend;
  int64 low_battery_suspend_percent;
  CHECK(prefs_->GetInt64(kLowBatterySuspendPercentPref,
                         &low_battery_suspend_percent));
  CHECK(prefs_->GetInt64(kCleanShutdownTimeoutMsPref,
                         &clean_shutdown_timeout_ms_));
  CHECK(prefs_->GetInt64(kPluggedDimMsPref, &plugged_dim_ms_));
  CHECK(prefs_->GetInt64(kPluggedOffMsPref, &plugged_off_ms_));
  CHECK(prefs_->GetInt64(kPluggedSuspendMsPref, &plugged_suspend_ms_));
  CHECK(prefs_->GetInt64(kUnpluggedDimMsPref, &unplugged_dim_ms_));
  CHECK(prefs_->GetInt64(kUnpluggedOffMsPref, &unplugged_off_ms_));
  CHECK(prefs_->GetInt64(kUnpluggedSuspendMsPref, &unplugged_suspend_ms_));
  CHECK(prefs_->GetInt64(kReactMsPref, &react_ms_));
  CHECK(prefs_->GetInt64(kFuzzMsPref, &fuzz_ms_));
  CHECK(prefs_->GetInt64(kEnforceLockPref, &enforce_lock));
  CHECK(prefs_->GetInt64(kUseXScreenSaverPref, &use_xscreensaver));
  if (prefs_->GetInt64(kDisableIdleSuspendPref, &disable_idle_suspend) &&
      disable_idle_suspend) {
    LOG(INFO) << "Idle suspend feature disabled";
    plugged_suspend_ms_ = INT64_MAX;
    unplugged_suspend_ms_ = INT64_MAX;
  }
  ReadLockScreenSettings();
  if (low_battery_suspend_percent >= 0 && low_battery_suspend_percent <= 100) {
    low_battery_suspend_percent_ = low_battery_suspend_percent;
  } else {
    LOG(INFO) << "Unreasonable low battery suspend percent threshold:"
              << low_battery_suspend_percent;
    LOG(INFO) << "Disabling low battery suspend.";
    low_battery_suspend_percent_ = 0;
  }
  lock_ms_ = default_lock_ms_;
  enforce_lock_ = enforce_lock;
  use_xscreensaver_ = use_xscreensaver;

  // Check that timeouts are sane.
  CHECK(kMetricIdleMin >= fuzz_ms_);
  CHECK(plugged_dim_ms_ >= react_ms_);
  CHECK(plugged_off_ms_ >= plugged_dim_ms_ + react_ms_);
  CHECK(plugged_suspend_ms_ >= plugged_off_ms_ + react_ms_);
  CHECK(unplugged_dim_ms_ >= react_ms_);
  CHECK(unplugged_off_ms_ >= unplugged_dim_ms_ + react_ms_);
  CHECK(unplugged_suspend_ms_ >= unplugged_off_ms_ + react_ms_);
  CHECK(default_lock_ms_ >= unplugged_off_ms_ + react_ms_);
  CHECK(default_lock_ms_ >= plugged_off_ms_ + react_ms_);

  // Store unmodified timeout values for switching between projecting and non-
  // projecting timeouts.
  base_timeout_values_[kPluggedDimMsPref]       = plugged_dim_ms_;
  base_timeout_values_[kPluggedOffMsPref]       = plugged_off_ms_;
  base_timeout_values_[kPluggedSuspendMsPref]   = plugged_suspend_ms_;
  base_timeout_values_[kUnpluggedDimMsPref]     = unplugged_dim_ms_;
  base_timeout_values_[kUnpluggedOffMsPref]     = unplugged_off_ms_;
  base_timeout_values_[kUnpluggedSuspendMsPref] = unplugged_suspend_ms_;

  // Initialize from prefs as might be used before AC plug status evaluated.
  dim_ms_ = unplugged_dim_ms_;
  off_ms_ = unplugged_off_ms_;

  if (monitor_reconfigure_->is_projecting())
    AdjustIdleTimeoutsForProjection();

  state_control_->ReadSettings(prefs_);
}

void Daemon::ReadLockScreenSettings() {
  int64 lock_on_idle_suspend = 0;
  if (prefs_->GetInt64(kLockOnIdleSuspendPref, &lock_on_idle_suspend) &&
      lock_on_idle_suspend) {
    LOG(INFO) << "Enabling screen lock on idle and suspend";
    CHECK(prefs_->GetInt64(kLockMsPref, &default_lock_ms_));
  } else {
    LOG(INFO) << "Disabling screen lock on idle and suspend";
    default_lock_ms_ = INT64_MAX;
  }
  base_timeout_values_[kLockMsPref] = default_lock_ms_;
  lock_on_idle_suspend_ = lock_on_idle_suspend;
}

void Daemon::Run() {
  GMainLoop* loop = g_main_loop_new(NULL, false);
  SchedulePollPowerSupply();
  g_main_loop_run(loop);
}

void Daemon::UpdateIdleStates() {
  LOG(INFO) << "Daemon : UpdateIdleStates";
  SetIdleState(idle_->GetIdleTimeMs());
}

void Daemon::SetPlugged(bool plugged) {
  if (plugged == plugged_state_)
    return;

  HandleNumOfSessionsPerChargeOnSetPlugged(&metrics_store_,
                                           plugged ?
                                           kPowerConnected :
                                           kPowerDisconnected);

  // If we are moving from kPowerUknown then we don't know how long the device
  // has been on AC for and thus our metric would not tell us anything about the
  // battery state when the user decided to charge.
  if (plugged_state_ != kPowerUnknown) {
    GenerateBatteryRemainingWhenChargeStartsMetric(
        plugged ? kPowerConnected : kPowerDisconnected,
        power_status_);
  }

  LOG(INFO) << "Daemon : SetPlugged = " << plugged;
  plugged_state_ = plugged ? kPowerConnected : kPowerDisconnected;
  int64 idle_time_ms = idle_->GetIdleTimeMs();
  // If the screen is on, and the user plugged or unplugged the computer,
  // we should wait a bit before turning off the screen.
  // If the screen is off, don't immediately suspend, wait another
  // suspend timeout.
  // If the state is uninitialized, this is the powerd startup condition, so
  // we ignore any idle time from before powerd starts.
  if (backlight_controller_->GetPowerState() == BACKLIGHT_ACTIVE ||
      backlight_controller_->GetPowerState() == BACKLIGHT_DIM)
    SetIdleOffset(idle_time_ms, kIdleNormal);
  else if (backlight_controller_->GetPowerState() == BACKLIGHT_IDLE_OFF)
    SetIdleOffset(idle_time_ms, kIdleSuspend);
  else if (backlight_controller_->GetPowerState() == BACKLIGHT_UNINITIALIZED)
    SetIdleOffset(idle_time_ms, kIdleNormal);
  else
    SetIdleOffset(0, kIdleNormal);

  backlight_controller_->OnPlugEvent(plugged);
  SetIdleState(idle_time_ms);
}

void Daemon::OnRequestRestart() {
  if (shutdown_state_ == kShutdownNone) {
    shutdown_state_ = kShutdownRestarting;
    StartCleanShutdown();
  }
}

void Daemon::OnRequestShutdown() {
  if (shutdown_state_ == kShutdownNone) {
    shutdown_state_ = kShutdownPowerOff;
    StartCleanShutdown();
  }
}

void Daemon::StartCleanShutdown() {
  clean_shutdown_initiated_ = true;
  // Cancel any outstanding suspend in flight.
  suspender_.CancelSuspend();
  util::SendSignalToPowerM(kRequestCleanShutdown);
  g_timeout_add(clean_shutdown_timeout_ms_, CleanShutdownTimedOutThunk, this);
}

void Daemon::SetIdleOffset(int64 offset_ms, IdleState state) {
  AdjustIdleTimeoutsForProjection();
  int64 prev_dim_ms = dim_ms_;
  int64 prev_off_ms = off_ms_;
  LOG(INFO) << "offset_ms_ = " << offset_ms;
  offset_ms_ = offset_ms;
  if (plugged_state_ == kPowerConnected) {
    dim_ms_ = plugged_dim_ms_;
    off_ms_ = plugged_off_ms_;
    suspend_ms_ = plugged_suspend_ms_;
  } else {
    CHECK(plugged_state_ == kPowerDisconnected);
    dim_ms_ = unplugged_dim_ms_;
    off_ms_ = unplugged_off_ms_;
    suspend_ms_ = unplugged_suspend_ms_;
  }
  lock_ms_ = default_lock_ms_;

  // Protect against overflow
  dim_ms_ = max(dim_ms_ + offset_ms, dim_ms_);
  off_ms_ = max(off_ms_ + offset_ms, off_ms_);
  suspend_ms_ = max(suspend_ms_ + offset_ms, suspend_ms_);

  if (enforce_lock_) {
    // Make sure that the screen turns off before it locks, and dims before
    // it turns off. This ensures the user gets a warning before we lock the
    // screen.
    off_ms_ = min(off_ms_, lock_ms_ - react_ms_);
    dim_ms_ = min(dim_ms_, lock_ms_ - 2 * react_ms_);
  } else {
    lock_ms_ = max(lock_ms_ + offset_ms, lock_ms_);
  }

  // Only offset timeouts for states starting with idle state provided.
  switch (state) {
    case kIdleSuspend:
      off_ms_ = prev_off_ms;
    case kIdleScreenOff:
      dim_ms_ = prev_dim_ms;
    case kIdleDim:
    case kIdleNormal:
      break;
    case kIdleUnknown:
    default: {
      LOG(ERROR) << "SetIdleOffset : Improper Idle State";
      break;
    }
  }

  // Sync up idle state with new settings.
  idle_->ClearTimeouts();
  if (offset_ms > fuzz_ms_)
    idle_->AddIdleTimeout(fuzz_ms_);
  if (kMetricIdleMin <= dim_ms_ - fuzz_ms_)
    idle_->AddIdleTimeout(kMetricIdleMin);
  // XIdle timeout events for dimming and idle-off.
  idle_->AddIdleTimeout(dim_ms_);
  idle_->AddIdleTimeout(off_ms_);
  // This is to start polling audio before a suspend.
  // |suspend_ms_| must be >= |off_ms_| + |react_ms_|, so if the following
  // condition is false, then they must be equal.  In that case, the idle
  // timeout at |off_ms_| would be equivalent, and the following timeout would
  // be redundant.
  if (suspend_ms_ - react_ms_ > off_ms_)
    idle_->AddIdleTimeout(suspend_ms_ - react_ms_);
  // XIdle timeout events for lock and/or suspend.
  if (lock_ms_ < suspend_ms_ - fuzz_ms_ || lock_ms_ - fuzz_ms_ > suspend_ms_) {
    idle_->AddIdleTimeout(lock_ms_);
    idle_->AddIdleTimeout(suspend_ms_);
  } else {
    idle_->AddIdleTimeout(max(lock_ms_, suspend_ms_));
  }
  // XIdle timeout events for idle notify status
  for (IdleThresholds::iterator iter = thresholds_.begin();
       iter != thresholds_.end();
       ++iter) {
    if (*iter == 0) {
      idle_->AddIdleTimeout(kMinTimeForIdle);
    } else  if (*iter > 0) {
      idle_->AddIdleTimeout(*iter);
    }
  }
}

// SetActive will transition to Normal state. Used for transitioning on events
// that do not result in activity monitored by X, i.e. lid open.
void Daemon::SetActive() {
  int64 idle_time_ms = idle_->GetIdleTimeMs();
  SetIdleOffset(idle_time_ms, kIdleNormal);
  SetIdleState(idle_time_ms);
}

void Daemon::SetIdleState(int64 idle_time_ms) {
  PowerState old_state = backlight_controller_->GetPowerState();
  if (idle_time_ms >= suspend_ms_ &&
      !state_control_->IsStateDisabled(kIdleSuspendDisabled)) {
    // Note: currently this state doesn't do anything.  But it can be possibly
    // useful in future development.  For example, if we want to implement
    // fade from suspend, we would want to have this state to make sure the
    // backlight is set to zero when suspended.
    backlight_controller_->SetPowerState(BACKLIGHT_SUSPENDED);
    audio_detector_->Disable();
    Suspend();
  } else if (idle_time_ms >= off_ms_ &&
             !state_control_->IsStateDisabled(kIdleBlankDisabled)) {
    if (util::IsUserLoggedIn())
      backlight_controller_->SetPowerState(BACKLIGHT_IDLE_OFF);
  } else if (idle_time_ms >= dim_ms_ &&
             !state_control_->IsStateDisabled(kIdleDimDisabled)) {
    backlight_controller_->SetPowerState(BACKLIGHT_DIM);
  } else if (backlight_controller_->GetPowerState() != BACKLIGHT_ACTIVE) {
    if (backlight_controller_->SetPowerState(BACKLIGHT_ACTIVE)) {
      if (backlight_controller_->GetPowerState() == BACKLIGHT_SUSPENDED) {
        util::CreateStatusFile(FilePath(run_dir_).Append(kUserActiveFile));
        suspender_.CancelSuspend();
      }
    }
    audio_detector_->Disable();
  } else if (idle_time_ms < react_ms_ && locker_.is_locked()) {
    BrightenScreenIfOff();
  }
  if (idle_time_ms >= lock_ms_ && util::IsUserLoggedIn() &&
      backlight_controller_->GetPowerState() != BACKLIGHT_SUSPENDED) {
    locker_.LockScreen();
  }
  if (old_state != backlight_controller_->GetPowerState())
    idle_transition_timestamps_[backlight_controller_->GetPowerState()] =
        base::TimeTicks::Now();
}

void Daemon::OnPowerEvent(void* object, const PowerStatus& info) {
  Daemon* daemon = static_cast<Daemon*>(object);
  daemon->SetPlugged(info.line_power_on);
  daemon->GenerateMetricsOnPowerEvent(info);
  // Do not emergency suspend if no battery exists.
  if (info.battery_is_present)
    daemon->OnLowBattery(info.battery_percentage);
}

void Daemon::AddIdleThreshold(int64 threshold) {
  idle_->AddIdleTimeout((threshold == 0) ? kMinTimeForIdle : threshold);
  thresholds_.push_back(threshold);
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
  if (util::IsUserLoggedIn() && backlight_controller_->IsBacklightActiveOff())
    backlight_controller_->IncreaseBrightness(BRIGHTNESS_CHANGE_AUTOMATED);
}

void Daemon::OnIdleEvent(bool is_idle, int64 idle_time_ms) {
  CHECK(plugged_state_ != kPowerUnknown);
  if (is_idle && backlight_controller_->GetPowerState() == BACKLIGHT_ACTIVE &&
      dim_ms_ <= idle_time_ms && !locker_.is_locked()) {
    int64 video_time_ms = 0;
    bool video_is_playing = false;
    int64 dim_timeout = kPowerConnected == plugged_state_ ? plugged_dim_ms_ :
                                                            unplugged_dim_ms_;
    CHECK(video_detector_->GetActivity(dim_timeout,
                                       &video_time_ms,
                                       &video_is_playing));
    if (video_is_playing)
      SetIdleOffset(idle_time_ms - video_time_ms, kIdleNormal);
  }
  if (is_idle && backlight_controller_->GetPowerState() == BACKLIGHT_DIM &&
      !util::OOBECompleted()) {
    LOG(INFO) << "OOBE not complete. Delaying screenoff until done.";
    SetIdleOffset(idle_time_ms, kIdleScreenOff);
  }
  if (is_idle &&
      backlight_controller_->GetPowerState() != BACKLIGHT_SUSPENDED &&
      idle_time_ms >= suspend_ms_ - react_ms_) {
    // Before suspending, make sure there is no audio playing for a period of
    // time, so start polling for audio detection early.
    audio_detector_->Enable();
  }
  if (is_idle &&
      backlight_controller_->GetPowerState() != BACKLIGHT_SUSPENDED &&
      idle_time_ms >= suspend_ms_) {
    int64 audio_time_ms = 0;
    bool audio_is_playing = false;
    CHECK(audio_detector_->GetActivity(kAudioCheckIntervalMs,
                                       &audio_time_ms,
                                       &audio_is_playing));
    if (audio_is_playing) {
      LOG(INFO) << "Delaying suspend because audio is playing.";
      // Increase the suspend offset by the react time.  Since the offset is
      // calculated relative to the ORIGINAL [un]plugged_suspend_ms_ value, we
      // need to use that here.
      int64 base_suspend_ms = (plugged_state_ == kPowerConnected) ?
                               plugged_suspend_ms_ : unplugged_suspend_ms_;
      SetIdleOffset(suspend_ms_ - base_suspend_ms + react_ms_, kIdleSuspend);
      // This is the tricky part.  Since the audio detection happens |react_ms_|
      // ms before suspend time, and suspend timeout gets offset by |react_ms_|
      // ms each time there is audio, there is no time to disable and reenable
      // audio detection using an idle timeout.  So audio detection should stay
      // on until either the system goes to suspend out the user comes out of
      // idle.
    }
  }

  if (is_idle) {
    last_idle_event_timestamp_ = base::TimeTicks::Now();
    last_idle_timedelta_ = base::TimeDelta::FromMilliseconds(idle_time_ms);
  } else if (!last_idle_event_timestamp_.is_null() &&
             idle_time_ms < last_idle_timedelta_.InMilliseconds()) {
    GenerateMetricsOnIdleEvent(is_idle, idle_time_ms);
  }
  SetIdleState(idle_time_ms);
  if (!is_idle && offset_ms_ != 0)
    SetIdleOffset(0, kIdleNormal);

  // Notify once for each threshold.
  IdleThresholds::iterator iter = thresholds_.begin();
  while (iter != thresholds_.end()) {
    // If we're idle and past a threshold, notify and erase the threshold.
    if (is_idle && *iter != 0 && idle_time_ms >= *iter) {
      IdleEventNotify(*iter);
      iter = thresholds_.erase(iter);
    // Else, if we just went active and the threshold is a check for active.
    } else if (!is_idle && *iter == 0) {
      IdleEventNotify(0);
      iter = thresholds_.erase(iter);
    } else {
      ++iter;
    }
  }
}

void Daemon::AdjustKeyboardBrightness(int direction) {
  if (!keyboard_backlight_)
    return;

  // TODO(dianders): Implement the equivalent of backlight_controller for
  // keyboard.  This function is a bit hacky until then.
  const int64 kNumKeylightLevels = 16;
  int64 level, max_level;

  if (!keyboard_backlight_->GetMaxBrightnessLevel(&max_level) ||
      !keyboard_backlight_->GetCurrentBrightnessLevel(&level)) {
    LOG(WARNING) << "Failed to get keyboard backlight brightness";
    return;
  }

  // Try to move by 1-step, handling corner cases:
  // 1. kNumKeylightLevels > max_level
  // 2. Step would take us less than 0 or more than max.
  int64 step_size = max(static_cast<int64>(1),
                        (max_level + 1) / kNumKeylightLevels);
  level = level + (direction * step_size);
  level = max(static_cast<int64>(0), min(max_level, level));

  if (!keyboard_backlight_->SetBrightnessLevel(level)) {
    LOG(WARNING) << "Failed to set keyboard backlight brightness";
    return;
  }

  OnKeyboardBrightnessChanged((100.0 * level) / max_level,
                              BRIGHTNESS_CHANGE_USER_INITIATED);
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

void Daemon::OnScreenBrightnessChanged(double brightness_percent,
                                       BrightnessChangeCause cause) {
  SendBrightnessChangedSignal(brightness_percent, cause,
                              kBrightnessChangedSignal);
}

void Daemon::OnKeyboardBrightnessChanged(double brightness_percent,
                                         BrightnessChangeCause cause) {
  SendBrightnessChangedSignal(brightness_percent, cause,
                              kKeyboardBrightnessChangedSignal);
}

gboolean Daemon::UdevEventHandler(GIOChannel* /* source */,
                                  GIOCondition /* condition */,
                                  gpointer data) {
  Daemon* daemon = static_cast<Daemon*>(data);

  struct udev_device* dev = udev_monitor_receive_device(daemon->udev_monitor_);
  if (dev) {
    LOG(INFO) << "Event on ("
              << udev_device_get_subsystem(dev)
              << ") Action "
              << udev_device_get_action(dev);
    CHECK(string(udev_device_get_subsystem(dev)) == kPowerSupplyUdevSubsystem);
    udev_device_unref(dev);

    // Rescheduling the timer to fire 5s from now to make sure that it doesn't
    // get a bogus value from being too close to this event.
    daemon->ScheduleShortPollPowerSupply();
    daemon->EventPollPowerSupply();
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

DBusHandlerResult Daemon::DBusMessageHandler(DBusConnection* connection,
                                             DBusMessage* message,
                                             void* data) {
  Daemon* daemon = static_cast<Daemon*>(data);
  CHECK(daemon);

  // Filter out messages other than signals and method calls.
  int type = dbus_message_get_type(message);
  if (type != DBUS_MESSAGE_TYPE_METHOD_CALL &&
      type != DBUS_MESSAGE_TYPE_SIGNAL) {
    if (type == DBUS_MESSAGE_TYPE_ERROR)
      util::LogDBusError(message);
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
  }

  // Look up and call the corresponding dbus message handler.
  std::string interface = dbus_message_get_interface(message);
  std::string member = dbus_message_get_member(message);
  DBusInterfaceMemberPair dbus_message_pair = std::make_pair(interface, member);

  if (type == DBUS_MESSAGE_TYPE_METHOD_CALL) {
    DBusMethodHandlerTable::iterator iter =
        daemon->dbus_method_handler_table_.find(dbus_message_pair);
    if (iter == daemon->dbus_method_handler_table_.end()) {
      LOG(ERROR) << "Could not find handler for " << interface << ":" << member
                 << " in method handler table.";
      return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    LOG(INFO) << "Got " << member << " method call";
    DBusMethodHandler callback = iter->second;
    DBusMessage* reply = (daemon->*callback)(message);

    // Must send a reply if it is a message.
    if (!reply)
      reply = util::CreateEmptyDBusReply(message);
    // If the send reply fails, it is due to lack of memory.
    CHECK(dbus_connection_send(connection, reply, NULL));
    dbus_message_unref(reply);
    return DBUS_HANDLER_RESULT_HANDLED;
  } else if (type == DBUS_MESSAGE_TYPE_SIGNAL) {
    DBusSignalHandlerTable::iterator iter =
        daemon->dbus_signal_handler_table_.find(dbus_message_pair);
    if (iter == daemon->dbus_signal_handler_table_.end()) {
      LOG(ERROR) << "Could not find handler for " << interface << ":" << member
                 << " in signal handler table.";
      return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }
    LOG(INFO) << "Got " << member << " signal";
    DBusSignalHandler callback = iter->second;
    if ((daemon->*callback)(message))
      return DBUS_HANDLER_RESULT_HANDLED;
    // Do not send a reply if it is a signal.
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
  }
  NOTREACHED();  // All valid cases should be handled and returned.
  return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

void Daemon::RegisterDBusMessageHandler() {
  DBusConnection* connection = dbus_g_connection_get_connection(
      chromeos::dbus::GetSystemBusConnection().g_connection());
  CHECK(connection);

  DBusError error;
  dbus_error_init(&error);
  dbus_bus_request_name(connection,
                        power_manager::kPowerManagerServiceName,
                        0,
                        &error);
  if (dbus_error_is_set(&error)) {
    LOG(DFATAL) << "Failed to register name \""
                << power_manager::kPowerManagerServiceName << "\": "
                << error.message;
    dbus_error_free(&error);
  }

  AddDBusSignalHandler(kPowerManagerInterface, kRequestSuspendSignal,
                       &Daemon::HandleRequestSuspendSignal);
  AddDBusSignalHandler(kPowerManagerInterface, kLidClosed,
                       &Daemon::HandleLidClosedSignal);
  AddDBusSignalHandler(kPowerManagerInterface, kLidOpened,
                       &Daemon::HandleLidOpenedSignal);
  AddDBusSignalHandler(kPowerManagerInterface, kButtonEventSignal,
                       &Daemon::HandleButtonEventSignal);
  AddDBusSignalHandler(kPowerManagerInterface, kCleanShutdown,
                       &Daemon::HandleCleanShutdownSignal);
  AddDBusSignalHandler(kPowerManagerInterface, kPowerStateChangedSignal,
                       &Daemon::HandlePowerStateChangedSignal);
  AddDBusSignalHandler(login_manager::kSessionManagerInterface,
                       login_manager::kSessionManagerSessionStateChanged,
                       &Daemon::HandleSessionManagerSessionStateChangedSignal);
  AddDBusSignalHandler(kPowerManagerInterface, kStateOverrideCancel,
                       &Daemon::HandleStateOverrideCancelSignal);

  AddDBusMethodHandler(kPowerManagerInterface, kRequestLockScreenMethod,
                       &Daemon::HandleRequestLockScreenMethod);
  AddDBusMethodHandler(kPowerManagerInterface, kRequestUnlockScreenMethod,
                       &Daemon::HandleRequestUnlockScreenMethod);
  AddDBusMethodHandler(kPowerManagerInterface, kScreenIsLockedMethod,
                       &Daemon::HandleScreenIsLockedMethod);
  AddDBusMethodHandler(kPowerManagerInterface, kScreenIsUnlockedMethod,
                       &Daemon::HandleScreenIsUnlockedMethod);
  AddDBusMethodHandler(kPowerManagerInterface, kRequestShutdownMethod,
                       &Daemon::HandleRequestShutdownMethod);
  AddDBusMethodHandler(kPowerManagerInterface, kRequestRestartMethod,
                       &Daemon::HandleRequestRestartMethod);
  AddDBusMethodHandler(kPowerManagerInterface, kDecreaseScreenBrightness,
                       &Daemon::HandleDecreaseScreenBrightnessMethod);
  AddDBusMethodHandler(kPowerManagerInterface, kIncreaseScreenBrightness,
                       &Daemon::HandleIncreaseScreenBrightnessMethod);
  AddDBusMethodHandler(kPowerManagerInterface, kGetScreenBrightnessPercent,
                       &Daemon::HandleGetScreenBrightnessMethod);
  AddDBusMethodHandler(kPowerManagerInterface, kSetScreenBrightnessPercent,
                       &Daemon::HandleSetScreenBrightnessMethod);
  AddDBusMethodHandler(kPowerManagerInterface, kDecreaseKeyboardBrightness,
                       &Daemon::HandleDecreaseKeyboardBrightnessMethod);
  AddDBusMethodHandler(kPowerManagerInterface, kIncreaseKeyboardBrightness,
                       &Daemon::HandleIncreaseKeyboardBrightnessMethod);
  AddDBusMethodHandler(kPowerManagerInterface, kGetIdleTime,
                       &Daemon::HandleGetIdleTimeMethod);
  AddDBusMethodHandler(kPowerManagerInterface, kRequestIdleNotification,
                       &Daemon::HandleRequestIdleNotificationMethod);
  AddDBusMethodHandler(kPowerManagerInterface, kGetPowerSupplyPropertiesMethod,
                       &Daemon::HandleGetPowerSupplyPropertiesMethod);
  AddDBusMethodHandler(kPowerManagerInterface, kStateOverrideRequest,
                       &Daemon::HandleStateOverrideRequestMethod);
  AddDBusMethodHandler(kPowerManagerInterface, kHandleVideoActivityMethod,
                       &Daemon::HandleVideoActivityMethod);
  AddDBusMethodHandler(kPowerManagerInterface, kHandleUserActivityMethod,
                       &Daemon::HandleUserActivityMethod);
  AddDBusMethodHandler(kPowerManagerInterface, kSetIsProjectingMethod,
                       &Daemon::HandleSetIsProjectingMethod);

  CHECK(dbus_connection_add_filter(connection, &DBusMessageHandler, this,
                                   NULL));
  LOG(INFO) << "D-Bus monitoring started";
}

bool Daemon::HandleRequestSuspendSignal(DBusMessage*) {  // NOLINT
  Suspend();
  return true;
}

bool Daemon::HandleLidClosedSignal(DBusMessage*) {  // NOLINT
  SetActive();
  Suspend();
  return true;
}

bool Daemon::HandleLidOpenedSignal(DBusMessage*) {  // NOLINT
  SetActive();
  suspender_.CancelSuspend();
  return true;
}

bool Daemon::HandleButtonEventSignal(DBusMessage* message) {
  DBusError error;
  dbus_error_init(&error);
  const char* button_name = NULL;
  dbus_bool_t down = FALSE;
  dbus_int64_t timestamp = 0;
  if (!dbus_message_get_args(message, &error,
                             DBUS_TYPE_STRING, &button_name,
                             DBUS_TYPE_BOOLEAN, &down,
                             DBUS_TYPE_INT64, &timestamp,
                             DBUS_TYPE_INVALID)) {
    LOG(ERROR) << "Unable to process button event: "
               << error.name << " (" << error.message << ")";
    dbus_error_free(&error);
    return true;
  }

  // TODO:  Stop sending key events to powerd.
  if (strcmp(button_name, kPowerButtonName) == 0) {
    // TODO: Use |timestamp| instead if libbase/libchrome ever gets updated to a
    // recent-enough version that base::TimeTicks::FromInternalValue() is
    // available: http://crosbug.com/16623
    SendPowerButtonMetric(down, base::TimeTicks::Now());
  } else if (strcmp(button_name, kLockButtonName) == 0) {
    // Chrome handles lock button presses directly.
  } else if (strcmp(button_name, power_manager::kKeyLeftCtrl) == 0 ||
             strcmp(button_name, power_manager::kKeyRightCtrl) == 0) {
    // Do nothing - this will acknowledge the event until we can remove it.
  } else if (strcmp(button_name, power_manager::kKeyLeftShift) == 0 ||
             strcmp(button_name, power_manager::kKeyRightShift) == 0) {
    // Do nothing - this will acknowledge the event until we can remove it.
  } else if (strcmp(button_name, power_manager::kKeyLeftAlt) == 0 ||
             strcmp(button_name, power_manager::kKeyRightAlt) == 0) {
    // Do nothing - this will acknowledge the event until we can remove it.
  } else if (strcmp(button_name, kKeyF4) == 0) {
    // Do nothing - this will acknowledge the event until we can remove it.
  } else {
    NOTREACHED() << "Unhandled button '" << button_name << "'";
  }
  return true;
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

bool Daemon::HandlePowerStateChangedSignal(DBusMessage* message) {
  const char* state = '\0';
  DBusError error;
  dbus_error_init(&error);
  if (dbus_message_get_args(message, &error,
                            DBUS_TYPE_STRING, &state,
                            DBUS_TYPE_INVALID)) {
    OnPowerStateChange(state);
  } else {
    LOG(WARNING) << "Unable to read " << kPowerStateChanged << " args";
    dbus_error_free(&error);
  }
  return false;
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
    OnSessionStateChange(state, user);
  } else {
    LOG(WARNING) << "Unable to read "
                 << login_manager::kSessionManagerSessionStateChanged
                 << " args";
    dbus_error_free(&error);
  }
  return false;
}

bool Daemon::HandleStateOverrideCancelSignal(DBusMessage* message) {
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
  }
  return true;
}

DBusMessage* Daemon::HandleRequestLockScreenMethod(DBusMessage* message) {
  locker_.LockScreen();
  return NULL;
}

DBusMessage* Daemon::HandleRequestUnlockScreenMethod(DBusMessage* message) {
  util::SendSignalToSessionManager("UnlockScreen");
  return NULL;
}

DBusMessage* Daemon::HandleScreenIsLockedMethod(DBusMessage* message) {
  locker_.set_locked(true);
  suspender_.CheckSuspend();
  return NULL;
}

DBusMessage* Daemon::HandleScreenIsUnlockedMethod(DBusMessage* message) {
  locker_.set_locked(false);
  return NULL;
}

DBusMessage* Daemon::HandleRequestShutdownMethod(DBusMessage* message) {
  OnRequestShutdown();
  return NULL;
}

DBusMessage* Daemon::HandleRequestRestartMethod(DBusMessage* message) {
  OnRequestRestart();
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
                               kBrightnessDown,
                               kBrightnessEnumMax);
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
                               kBrightnessUp,
                               kBrightnessEnumMax);
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
    return util::CreateDBusErrorReply(message, DBUS_ERROR_INVALID_ARGS,
                                      "Invalid arguments passed to method");
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
  SendEnumMetricWithPowerState(kMetricBrightnessAdjust, kBrightnessAbsolute,
                               kBrightnessEnumMax);
  return NULL;
}

DBusMessage* Daemon::HandleGetScreenBrightnessMethod(DBusMessage* message) {
  double percent;
  if (!backlight_controller_->GetCurrentBrightnessPercent(&percent)) {
    return util::CreateDBusErrorReply(message, DBUS_ERROR_FAILED,
                                      "Could not fetch Screen Brightness");
  }
  DBusMessage* reply = dbus_message_new_method_return(message);
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
  int64 idle_time_ms = idle_->GetIdleTimeMs();
  DBusMessage* reply = dbus_message_new_method_return(message);
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
    AddIdleThreshold(threshold);
  } else {
    LOG(WARNING) << "Unable to read " << kRequestIdleNotification << " args";
    dbus_error_free(&error);
  }
  return NULL;
}

DBusMessage* Daemon::HandleGetPowerSupplyPropertiesMethod(
    DBusMessage* message) {
  PowerSupplyProperties protobuf;
  PowerStatus* status = &power_status_;

  protobuf.set_line_power_on(status->line_power_on);
  protobuf.set_battery_energy(status->battery_energy);
  protobuf.set_battery_energy_rate(status->battery_energy_rate);
  protobuf.set_battery_voltage(status->battery_voltage);
  protobuf.set_battery_time_to_empty(status->battery_time_to_empty);
  protobuf.set_battery_time_to_full(status->battery_time_to_full);
  protobuf.set_battery_percentage(status->battery_percentage);
  protobuf.set_battery_is_present(status->battery_is_present);
  protobuf.set_battery_is_charged(status->battery_state ==
                                  BATTERY_STATE_FULLY_CHARGED);
  protobuf.set_is_calculating_battery_time(status->is_calculating_battery_time);
  protobuf.set_averaged_battery_time_to_empty(
      status->averaged_battery_time_to_empty);
  protobuf.set_averaged_battery_time_to_full(
      status->averaged_battery_time_to_full);

  DBusMessage* reply = dbus_message_new_method_return(message);
  CHECK(reply);
  std::string serialized_proto;
  CHECK(protobuf.SerializeToString(&serialized_proto));
  // For array arguments, D-Bus wants the array typecode, the element
  // typecode, the array address, and the number of elements (as opposed to
  // the usual typecode-followed-by-address ordering).
  const char* serial_data = serialized_proto.data();
  dbus_message_append_args(reply,
                           DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE,
                           &serial_data, serialized_proto.size(),
                           DBUS_TYPE_INVALID);
  return reply;
}

DBusMessage* Daemon::HandleStateOverrideRequestMethod(DBusMessage* message) {
  DBusError error;
  dbus_error_init(&error);
  char* data;
  int size;
  if (dbus_message_get_args(message, &error,
                            DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE,
                            &data, &size,
                            DBUS_TYPE_INVALID)) {
    int return_value;
    bool success = state_control_->StateOverrideRequest(
         data, size, &return_value);
    if (success) {
      DBusMessage* reply = dbus_message_new_method_return(message);
      CHECK(reply);
      dbus_message_append_args(reply,
                               DBUS_TYPE_INT32, &return_value,
                               DBUS_TYPE_INVALID);
      return reply;
    }
    return util::CreateDBusErrorReply(message, DBUS_ERROR_FAILED,
                                      "Failed processing request");
  }
  LOG(WARNING) << kStateOverrideRequest << ": Error reading args: " <<
    error.message;
  return util::CreateDBusErrorReply(message, DBUS_ERROR_INVALID_ARGS,
                                    "Invalid arguments passed to method");
}

DBusMessage* Daemon::HandleVideoActivityMethod(DBusMessage* message) {
  DBusError error;
  dbus_error_init(&error);
  int64 last_activity_time_internal = 0;
  if (!dbus_message_get_args(message, &error,
                             DBUS_TYPE_INT64, &last_activity_time_internal,
                             DBUS_TYPE_INVALID)) {
    LOG(WARNING) << kHandleVideoActivityMethod
                << ": Error reading args: " << error.message;
    dbus_error_free(&error);
    return util::CreateDBusErrorReply(message, DBUS_ERROR_INVALID_ARGS,
                                      "Invalid arguments passed to method");
  }
  video_detector_->HandleActivity(
      base::TimeTicks::FromInternalValue(last_activity_time_internal));
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
    return util::CreateDBusErrorReply(message, DBUS_ERROR_INVALID_ARGS,
                                      "Invalid arguments passed to method");
  }
  idle_->HandleUserActivity(
      base::TimeTicks::FromInternalValue(last_activity_time_internal));
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
    monitor_reconfigure_->SetIsProjecting(is_projecting);
  } else {
    // The message was malformed so log this and return an error.
    LOG(WARNING) << kSetIsProjectingMethod << ": Error reading args: "
                 << error.message;
    reply = util::CreateDBusErrorReply(message, DBUS_ERROR_INVALID_ARGS,
                                       "Invalid arguments passed to method");
  }
  return reply;
}

void Daemon::AddDBusSignalHandler(const std::string& interface,
                                  const std::string& member,
                                  DBusSignalHandler handler) {
  DBusConnection* connection = dbus_g_connection_get_connection(
      chromeos::dbus::GetSystemBusConnection().g_connection());
  CHECK(connection);
  util::AddDBusSignalMatch(connection, interface, member);
  dbus_signal_handler_table_[std::make_pair(interface, member)] = handler;
}

void Daemon::AddDBusMethodHandler(const std::string& interface,
                                  const std::string& member,
                                  DBusMethodHandler handler) {
  DBusConnection* connection = dbus_g_connection_get_connection(
      chromeos::dbus::GetSystemBusConnection().g_connection());
  CHECK(connection);
  util::AddDBusMethodMatch(
      connection, interface, kPowerManagerServicePath, member);
  dbus_method_handler_table_[std::make_pair(interface, member)] = handler;
}

void Daemon::ScheduleShortPollPowerSupply() {
  if (poll_power_supply_timer_id_ > 0)
    g_source_remove(poll_power_supply_timer_id_);
  poll_power_supply_timer_id_ = g_timeout_add(kBatteryPollShortIntervalMs,
                                              ShortPollPowerSupplyThunk,
                                              this);
}

void Daemon::SchedulePollPowerSupply() {
  if (poll_power_supply_timer_id_ > 0)
    g_source_remove(poll_power_supply_timer_id_);
  poll_power_supply_timer_id_ = g_timeout_add(kBatteryPollIntervalMs,
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
  UpdateAveragedTimes(&power_status_,
                      &time_to_empty_average_,
                      &time_to_full_average_);
  // Send a signal once the power supply status has been obtained.
  DBusMessage* message = dbus_message_new_signal(kPowerManagerServicePath,
                                                 kPowerManagerInterface,
                                                 kPowerSupplyPollSignal);
  CHECK(message != NULL);
  DBusConnection* connection = dbus_g_connection_get_connection(
      chromeos::dbus::GetSystemBusConnection().g_connection());
  if (!dbus_connection_send(connection, message, NULL))
    LOG(WARNING) << "Sending battery poll signal failed.";
  // Always repeat polling.
  return TRUE;
}

void Daemon::UpdateAveragedTimes(PowerStatus* status,
                                 RollingAverage* empty_average,
                                 RollingAverage* full_average) {
  if (status->line_power_on) {
    if (!status->is_calculating_battery_time)
      full_average->AddSample(status->battery_time_to_full);
    empty_average->Clear();
  } else {
    if (!status->is_calculating_battery_time)
      empty_average->AddSample(status->battery_time_to_empty);
    full_average->Clear();
  }

  status->averaged_battery_time_to_full = full_average->GetAverage();
  status->averaged_battery_time_to_empty = empty_average->GetAverage();
}

void Daemon::OnLowBattery(double battery_percentage) {
  if (!low_battery_suspend_percent_) {
    LOG(INFO) << "Battery percent : "
              << battery_percentage << "%";
    low_battery_ = false;
    return;
  }
  if (kPowerDisconnected == plugged_state_ && !low_battery_ &&
      battery_percentage <= low_battery_suspend_percent_ &&
      battery_percentage >= 0) {
    // Shut the system down when low battery condition is encountered.
    LOG(INFO) << "Low battery condition detected. Shutting down immediately.";
    low_battery_ = true;
    file_tagger_.HandleLowBatteryEvent();
    OnRequestShutdown();
  } else if (battery_percentage < 0) {
    LOG(INFO) << "Battery is at " << battery_percentage << "%, may not be "
              << "fully initialized yet.";
  } else if (kPowerConnected == plugged_state_ ||
             battery_percentage > low_battery_suspend_percent_ ) {
    LOG(INFO) << "Battery condition is safe (plugged in or not low) : "
              << battery_percentage << "%";
    low_battery_ = false;
    file_tagger_.HandleSafeBatteryEvent();
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
  return FALSE;
}

void Daemon::OnPowerStateChange(const char* state) {
  // on == resume via powerd_suspend
  if (g_str_equal(state, "on") == TRUE) {
    LOG(INFO) << "Resuming has commenced";
    HandleResume();
    SetActive();
  } else {
    DLOG(INFO) << "Saw arg:" << state << " for PowerStateChange";
  }
}

void Daemon::OnSessionStateChange(const char* state, const char* user) {
  if (!state || !user) {
    LOG(DFATAL) << "Got session state change with missing state or user";
    return;
  }

  std::string state_string(state);

  if (kValidStates.find(state_string) == kValidStates.end()) {
    LOG(WARNING) << "Changing to unknown session state: " << state;
    return;
  }

  if (state_string == "started") {
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

    if (plugged_state_ == kPowerDisconnected)
      metrics_store_.IncrementNumOfSessionsPerChargeMetric();

    current_user_ = user;
    session_start_ = base::Time::Now();
    DLOG(INFO) << "Session started for "
               << (current_user_.empty() ? "guest" : "non-guest user");
  } else if (current_session_state_ != state) {
    DLOG(INFO) << "Session " << state;
    // For states other then "started" we only want to take action if we have
    // actually changed state, since the code we are calling assumes that we are
    // actually transitioning between states.
    current_user_.clear();
    if (current_session_state_ == "stopped")
      GenerateEndOfSessionMetrics(power_status_,
                                  *backlight_controller_,
                                  base::Time::Now(),
                                  session_start_);
  }
  current_session_state_ = state;
}

void Daemon::SendPowerButtonMetric(bool down,
                                   const base::TimeTicks& timestamp) {
  if (down) {
    if (!last_power_button_down_timestamp_.is_null())
      LOG(ERROR) << "Got power-button-down event while button was already down";
    last_power_button_down_timestamp_ = timestamp;
  } else {
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
}

void Daemon::Shutdown() {
  if (shutdown_state_ == kShutdownPowerOff) {
    LOG(INFO) << "Shutting down";
    util::SendSignalToPowerM(kShutdownSignal);
  } else if (shutdown_state_ == kShutdownRestarting) {
    LOG(INFO) << "Restarting";
    util::SendSignalToPowerM(kRestartSignal);
  } else {
    LOG(ERROR) << "Shutdown : Improper System State!";
  }
}

void Daemon::Suspend() {
  if (clean_shutdown_initiated_) {
    LOG(INFO) << "Ignoring request for suspend with outstanding shutdown.";
    return;
  }
  if (util::IsUserLoggedIn()) {
    power_supply_.SetSuspendState(true);
    suspender_.RequestSuspend();
    // When going to suspend, notify the backlight controller so it will know to
    // set the backlight correctly upon resume.
    backlight_controller_->SetPowerState(BACKLIGHT_SUSPENDED);
  } else {
    LOG(INFO) << "Not logged in. Suspend Request -> Shutting down.";
    OnRequestShutdown();
  }
}

gboolean Daemon::PrefChangeHandler(const char* name,
                                   int,               // watch handle
                                   unsigned int,      // mask
                                   gpointer data) {
  Daemon* daemon = static_cast<Daemon*>(data);
  if (!strcmp(name, "lock_on_idle_suspend")) {
    daemon->ReadLockScreenSettings();
    daemon->locker_.Init(daemon->use_xscreensaver_,
                         daemon->lock_on_idle_suspend_);
    daemon->SetIdleOffset(0, kIdleNormal);
  }
  return TRUE;
}

void Daemon::HandleResume() {
  file_tagger_.HandleResumeEvent();
  power_supply_.SetSuspendState(false);
}

void Daemon::RetrieveSessionState() {
  DBusGConnection* connection =
      chromeos::dbus::GetSystemBusConnection().g_connection();
  CHECK(connection);

  DBusGProxy* proxy = dbus_g_proxy_new_for_name(
      connection,
      login_manager::kSessionManagerServiceName,
      login_manager::kSessionManagerServicePath,
      login_manager::kSessionManagerInterface);

  GError* error = NULL;
  gchar* state = NULL;
  gchar* user = NULL;
  if (dbus_g_proxy_call(proxy,
                        login_manager::kSessionManagerRetrieveSessionState,
                        &error,
                        G_TYPE_INVALID,
                        G_TYPE_STRING,
                        &state,
                        G_TYPE_STRING,
                        &user,
                        G_TYPE_INVALID)) {
    LOG(INFO) << "Retrieved session state of " << state;
    OnSessionStateChange(state, user);
    g_free(state);
    g_free(user);
  } else {
    LOG(ERROR) << "Unable to retrieve session state from session manager: "
               << error->message;
    g_error_free(error);
  }
  g_object_unref(proxy);
}

void Daemon::AdjustIdleTimeoutsForProjection() {
  plugged_dim_ms_       = base_timeout_values_[kPluggedDimMsPref];
  plugged_off_ms_       = base_timeout_values_[kPluggedOffMsPref];
  plugged_suspend_ms_   = base_timeout_values_[kPluggedSuspendMsPref];
  unplugged_dim_ms_     = base_timeout_values_[kUnpluggedDimMsPref];
  unplugged_off_ms_     = base_timeout_values_[kUnpluggedOffMsPref];
  unplugged_suspend_ms_ = base_timeout_values_[kUnpluggedSuspendMsPref];
  default_lock_ms_      = base_timeout_values_[kLockMsPref];

  if (monitor_reconfigure_->is_projecting()) {
    LOG(INFO) << "External display projection: multiplying idle times by "
              << kProjectionTimeoutFactor;
    plugged_dim_ms_ *= kProjectionTimeoutFactor;
    plugged_off_ms_ *= kProjectionTimeoutFactor;
    if (plugged_suspend_ms_ != INT64_MAX)
      plugged_suspend_ms_ *= kProjectionTimeoutFactor;
    unplugged_dim_ms_ *= kProjectionTimeoutFactor;
    unplugged_off_ms_ *= kProjectionTimeoutFactor;
    if (unplugged_suspend_ms_ != INT64_MAX)
      unplugged_suspend_ms_ *= kProjectionTimeoutFactor;
    if (default_lock_ms_ != INT64_MAX)
      default_lock_ms_ *= kProjectionTimeoutFactor;
  }
}

}  // namespace power_manager
