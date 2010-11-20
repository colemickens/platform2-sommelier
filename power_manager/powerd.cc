// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd.h"

#include <gdk/gdkx.h>
#include <stdint.h>
#include <X11/extensions/dpms.h>
#include <X11/keysym.h>
#include <X11/XF86keysym.h>
#include <sys/inotify.h>

#include <algorithm>
#include <string>
#include <vector>

#include "base/file_util.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "chromeos/dbus/dbus.h"
#include "chromeos/dbus/service_constants.h"
#include "power_manager/power_button_handler.h"
#include "power_manager/power_constants.h"
#include "power_manager/util.h"

using std::max;
using std::min;
using std::string;
using std::vector;

namespace power_manager {

// The minimum delta between timers to avoid timer precision issues
static const int64 kFuzzMS = 100;

// The minimum delta between timers when we want to give a user time to react
static const int64 kReactMS = 30000;

static const char kTaggedFilePath[] = "/var/lib/power_manager";

// Daemon: Main power manager. Adjusts device status based on whether the
//         user is idle and on video activity indicator from the window manager.
//         This daemon is responsible for dimming of the backlight, turning
//         the screen off, and suspending to RAM. The daemon also has the
//         capability of shutting the system down.

Daemon::Daemon(BacklightController* ctl, PowerPrefs* prefs,
               MetricsLibraryInterface* metrics_lib,
               VideoDetectorInterface* video_detector,
               const FilePath& run_dir)
    : ctl_(ctl),
      prefs_(prefs),
      metrics_lib_(metrics_lib),
      video_detector_(video_detector),
      low_battery_suspend_percent_(0),
      clean_shutdown_initiated_(false),
      low_battery_(false),
      enforce_lock_(false),
      use_xscreensaver_(false),
      plugged_state_(kPowerUnknown),
      idle_state_(kIdleUnknown),
      system_state_(kSystemOn),
      file_tagger_(FilePath(kTaggedFilePath)),
      suspender_(&locker_, &file_tagger_),
      run_dir_(run_dir),
      power_button_handler_(new PowerButtonHandler(this)),
      battery_discharge_rate_metric_last_(0),
      battery_remaining_charge_metric_last_(0),
      battery_time_to_empty_metric_last_(0) {}

Daemon::~Daemon() {}

void Daemon::Init() {
  ReadSettings();
  CHECK(idle_.Init(this));
  prefs_->StartPrefWatching(&(PrefChangeHandler), this);
  MetricInit();
  if (!DPMSCapable(GDK_DISPLAY())) {
    LOG(WARNING) << "X Server not DPMS capable";
  } else {
    CHECK(DPMSEnable(GDK_DISPLAY()));
    CHECK(DPMSSetTimeouts(GDK_DISPLAY(), 0, 0, 0));
  }
  CHECK(XSetScreenSaver(GDK_DISPLAY(),
                        0,                 // 0 display timeout
                        0,                 // 0 alteration timeout
                        DefaultBlanking,
                        DefaultExposures));
  key_brightness_up_ = XKeysymToKeycode(GDK_DISPLAY(), XF86XK_MonBrightnessUp);
  key_brightness_down_ = XKeysymToKeycode(GDK_DISPLAY(),
                                          XF86XK_MonBrightnessDown);
  key_f6_ = XKeysymToKeycode(GDK_DISPLAY(), XK_F6);
  key_f7_ = XKeysymToKeycode(GDK_DISPLAY(), XK_F7);
  CHECK(key_f6_ != 0);
  CHECK(key_f7_ != 0);
  if (key_brightness_up_ == 0) {
    LOG(ERROR) << "No brightness up keycode found. Guessing instead.";
    key_brightness_up_ = 212;
  }
  if (key_brightness_down_ == 0) {
    LOG(ERROR) << "No brightness down keycode found. Guessing instead.";
    key_brightness_down_ = 101;
  }
  GrabKey(key_brightness_up_, 0);
  GrabKey(key_brightness_down_, 0);
  GrabKey(key_f6_, 0);
  GrabKey(key_f7_, 0);
  gdk_window_add_filter(NULL, gdk_event_filter, this);
  locker_.Init(use_xscreensaver_, lock_on_idle_suspend_);
  RegisterDBusMessageHandler();
  suspender_.Init(run_dir_);
  CHECK(chromeos::MonitorPowerStatus(&OnPowerEvent, this));
  file_tagger_.Init();
}

void Daemon::ReadSettings() {
  int64 use_xscreensaver, enforce_lock;
  int64 disable_idle_suspend;
  int64 low_battery_suspend_percent;
  CHECK(prefs_->GetInt64(kLowBatterySuspendPercent,
                         &low_battery_suspend_percent));
  CHECK(prefs_->GetInt64(kCleanShutdownTimeoutMs,
                         &clean_shutdown_timeout_ms_));
  CHECK(prefs_->GetInt64(kPluggedDimMs, &plugged_dim_ms_));
  CHECK(prefs_->GetInt64(kPluggedOffMs, &plugged_off_ms_));
  CHECK(prefs_->GetInt64(kPluggedSuspendMs, &plugged_suspend_ms_));
  CHECK(prefs_->GetInt64(kUnpluggedDimMs, &unplugged_dim_ms_));
  CHECK(prefs_->GetInt64(kUnpluggedOffMs, &unplugged_off_ms_));
  CHECK(prefs_->GetInt64(kUnpluggedSuspendMs, &unplugged_suspend_ms_));
  CHECK(prefs_->GetInt64(kEnforceLock, &enforce_lock));
  CHECK(prefs_->GetInt64(kUseXScreenSaver, &use_xscreensaver));
  if (prefs_->GetInt64(kDisableIdleSuspend, &disable_idle_suspend) &&
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

  // Check that timeouts are sane
  CHECK(kMetricIdleMin >= kFuzzMS);
  CHECK(plugged_dim_ms_ >= kReactMS);
  CHECK(plugged_off_ms_ >= plugged_dim_ms_ + kReactMS);
  CHECK(plugged_suspend_ms_ >= plugged_off_ms_ + kReactMS);
  CHECK(unplugged_dim_ms_ >= kReactMS);
  CHECK(unplugged_off_ms_ >= unplugged_dim_ms_ + kReactMS);
  CHECK(unplugged_suspend_ms_ >= unplugged_off_ms_ + kReactMS);
  CHECK(default_lock_ms_ >= unplugged_off_ms_ + kReactMS);
  CHECK(default_lock_ms_ >= plugged_off_ms_ + kReactMS);
}

void Daemon::ReadLockScreenSettings() {
  int64 lock_on_idle_suspend;
  if (prefs_->GetInt64(kLockOnIdleSuspend, &lock_on_idle_suspend) &&
      0 == lock_on_idle_suspend) {
    LOG(INFO) << "Disabling screen lock on idle and suspend";
    default_lock_ms_ = INT64_MAX;
  } else {
    CHECK(prefs_->GetInt64(kLockMs, &default_lock_ms_));
    LOG(INFO) << "Enabling screen lock on idle and suspend";
  }
  lock_on_idle_suspend_ = lock_on_idle_suspend;
}

void Daemon::Run() {
  GMainLoop* loop = g_main_loop_new(NULL, false);
  g_main_loop_run(loop);
}

void Daemon::SetPlugged(bool plugged) {
  if (plugged != plugged_state_) {
    LOG(INFO) << "Daemon : SetPlugged = " << plugged;
    plugged_state_ = plugged ? kPowerConnected : kPowerDisconnected;
    int64 idle_time_ms;
    CHECK(idle_.GetIdleTime(&idle_time_ms));
    // If the screen is on, and the user plugged or unplugged the computer,
    // we should wait a bit before turning off the screen.
    // If the screen is off, don't immediately suspend.
    if (idle_state_ == kIdleNormal || idle_state_ == kIdleDim)
      SetIdleOffset(idle_time_ms, kIdleNormal);
    else if (idle_state_ == kIdleScreenOff)
      SetIdleOffset(idle_time_ms, kIdleSuspend);
    else
      SetIdleOffset(0, kIdleNormal);

    ctl_->OnPlugEvent(plugged);
    SetIdleState(idle_time_ms);
  }
}

void Daemon::OnRequestRestart() {
  if (system_state_ == kSystemOn || system_state_ == kSystemSuspend) {
    system_state_ = kSystemRestarting;
    StartCleanShutdown();
  }
}

void Daemon::OnRequestShutdown() {
  if (system_state_ == kSystemOn || system_state_ == kSystemSuspend) {
    system_state_ = kSystemShuttingDown;
    StartCleanShutdown();
  }
}

void Daemon::StartCleanShutdown() {
  clean_shutdown_initiated_ = true;
  // Cancel any outstanding suspend in flight.
  suspender_.CancelSuspend();
  util::SendSignalToPowerM(util::kRequestCleanShutdown);
  g_timeout_add(clean_shutdown_timeout_ms_, Daemon::CleanShutdownTimedOut,
                this);
}

void Daemon::SetIdleOffset(int64 offset_ms, IdleState state) {
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
    off_ms_ = min(off_ms_, lock_ms_ - kReactMS);
    dim_ms_ = min(dim_ms_, lock_ms_ - 2 * kReactMS);
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

  // Sync up idle state with new settings
  CHECK(idle_.ClearTimeouts());
  if (offset_ms > kFuzzMS)
    CHECK(idle_.AddIdleTimeout(kFuzzMS));
  if (kMetricIdleMin <= dim_ms_ - kFuzzMS)
    CHECK(idle_.AddIdleTimeout(kMetricIdleMin));
  CHECK(idle_.AddIdleTimeout(dim_ms_));
  CHECK(idle_.AddIdleTimeout(off_ms_));
  if (lock_ms_ < suspend_ms_ - kFuzzMS || lock_ms_ - kFuzzMS > suspend_ms_) {
    CHECK(idle_.AddIdleTimeout(lock_ms_));
    CHECK(idle_.AddIdleTimeout(suspend_ms_));
  } else {
    CHECK(idle_.AddIdleTimeout(max(lock_ms_, suspend_ms_)));
  }
}

// SetActive will transition to Normal state. Used for transitioning on events
// that do not result in activity monitored by X, i.e. lid open.
void Daemon::SetActive() {
  int64 idle_time_ms;
  CHECK(idle_.GetIdleTime(&idle_time_ms));
  SetIdleOffset(idle_time_ms, kIdleNormal);
  SetIdleState(idle_time_ms);
}

void Daemon::OnIdleEvent(bool is_idle, int64 idle_time_ms) {
  CHECK(plugged_state_ != kPowerUnknown);
  if (is_idle && kIdleNormal == idle_state_ &&
      dim_ms_ <= idle_time_ms && !locker_.is_locked()) {
    int64 video_time_ms = 0;
    bool video_is_playing = false;
    int64 dim_timeout = kPowerConnected == plugged_state_ ? plugged_dim_ms_ :
                                                            unplugged_dim_ms_;
    CHECK(video_detector_->GetVideoActivity(dim_timeout,
                                            &video_time_ms,
                                            &video_is_playing));
    if (video_is_playing)
      SetIdleOffset(idle_time_ms - video_time_ms, kIdleNormal);
  }
  if (is_idle && kIdleDim == idle_state_ && !util::OOBECompleted()) {
    LOG(INFO) << "OOBE not complete. Delaying screenoff until done.";
    SetIdleOffset(idle_time_ms, kIdleScreenOff);
  }
  GenerateMetricsOnIdleEvent(is_idle, idle_time_ms);
  SetIdleState(idle_time_ms);
  if (!is_idle && offset_ms_ != 0)
    SetIdleOffset(0, kIdleNormal);
}

void Daemon::SetIdleState(int64 idle_time_ms) {
  if (idle_time_ms >= suspend_ms_) {
    LOG(INFO) << "state = kIdleSuspend";
    idle_state_ = kIdleSuspend;
    Suspend();
  } else if (idle_time_ms >= off_ms_) {
    LOG(INFO) << "state = kIdleScreenOff";
    ctl_->SetPowerState(BACKLIGHT_OFF);
    idle_state_ = kIdleScreenOff;
  } else if (idle_time_ms >= dim_ms_) {
    LOG(INFO) << "state = kIdleDim";
    ctl_->SetDimState(BACKLIGHT_DIM);
    ctl_->SetPowerState(BACKLIGHT_ON);
    idle_state_ = kIdleDim;
  } else {
    LOG(INFO) << "state = kIdleNormal";
    ctl_->SetDimState(BACKLIGHT_ACTIVE);
    ctl_->SetPowerState(BACKLIGHT_ON);
    if (idle_state_ == kIdleSuspend) {
      util::CreateStatusFile(FilePath(run_dir_).Append(util::kUserActiveFile));
      suspender_.CancelSuspend();
    }
    idle_state_ = kIdleNormal;
  }
  if (idle_time_ms >= lock_ms_ && util::LoggedIn() &&
      idle_state_ != kIdleSuspend) {
    locker_.LockScreen();
  }
}

void Daemon::OnPowerEvent(void* object, const chromeos::PowerStatus& info) {
  Daemon* daemon = static_cast<Daemon*>(object);
  daemon->SetPlugged(info.line_power_on);
  daemon->GenerateMetricsOnPowerEvent(info);
  // Do not emergency suspend if no battery exists.
  if (info.battery_is_present)
    daemon->OnLowBattery(info.battery_percentage);
}

GdkFilterReturn Daemon::gdk_event_filter(GdkXEvent* gxevent, GdkEvent*,
                                         gpointer data) {
  Daemon* daemon = static_cast<Daemon*>(data);
  XEvent* xevent = static_cast<XEvent*>(gxevent);
  bool changed_brightness = false;
  if (xevent->type == KeyPress) {
    int keycode = xevent->xkey.keycode;
    if (keycode == daemon->key_brightness_up_ ||
               keycode == daemon->key_f7_) {
      if (keycode == daemon->key_brightness_up_) {
        LOG(INFO) << "Key press: Brightness up";
      } else {  // keycode == daemon->key_f7_
        LOG(INFO) << "Key press: F7";
      }
      daemon->ctl_->IncreaseBrightness();
      changed_brightness = true;
    } else if (keycode == daemon->key_brightness_down_ ||
               keycode == daemon->key_f6_) {
      if (keycode == daemon->key_brightness_down_) {
        LOG(INFO) << "Key press: Brightness down";
      } else {  // keycode == daemon->key_f6_
        LOG(INFO) << "Key press: F6";
      }
      daemon->ctl_->DecreaseBrightness();
      changed_brightness = true;
    }
  }

  if (changed_brightness) {
    int64 brightness = 0;
    daemon->ctl_->GetBrightness(&brightness);
    daemon->SendBrightnessChangedSignal(static_cast<int>(brightness));
  }

  return GDK_FILTER_CONTINUE;
}

void Daemon::GrabKey(KeyCode key, uint32 mask) {
  uint32 NumLockMask = Mod2Mask;
  uint32 CapsLockMask = LockMask;
  Window root = DefaultRootWindow(GDK_DISPLAY());
  XGrabKey(GDK_DISPLAY(), key, mask, root, True, GrabModeAsync, GrabModeAsync);
  XGrabKey(GDK_DISPLAY(), key, mask | CapsLockMask, root, True,
           GrabModeAsync, GrabModeAsync);
  XGrabKey(GDK_DISPLAY(), key, mask | NumLockMask, root, True,
           GrabModeAsync, GrabModeAsync);
  XGrabKey(GDK_DISPLAY(), key, mask | CapsLockMask | NumLockMask, root, True,
           GrabModeAsync, GrabModeAsync);
}

DBusHandlerResult Daemon::DBusMessageHandler(
    DBusConnection*, DBusMessage* message, void* data) {
  Daemon* daemon = static_cast<Daemon*>(data);
  if (dbus_message_is_signal(message, kPowerManagerInterface,
                             kRequestLockScreenSignal)) {
    LOG(INFO) << "RequestLockScreen event";
    daemon->locker_.LockScreen();
  } else if (dbus_message_is_signal(message, kPowerManagerInterface,
                                    kRequestUnlockScreenSignal)) {
    LOG(INFO) << "RequestUnlockScreen event";
    util::SendSignalToSessionManager("UnlockScreen");
  } else if (dbus_message_is_signal(message, kPowerManagerInterface,
                                    kScreenIsLockedSignal)) {
    LOG(INFO) << "ScreenIsLocked event";
    daemon->locker_.set_locked(true);
    daemon->suspender_.CheckSuspend();
  } else if (dbus_message_is_signal(message, kPowerManagerInterface,
                                    kScreenIsUnlockedSignal)) {
    LOG(INFO) << "ScreenIsUnlocked event";
    daemon->locker_.set_locked(false);
  } else if (dbus_message_is_signal(message, kPowerManagerInterface,
                                    kRequestSuspendSignal)) {
    LOG(INFO) << "RequestSuspend event";
    daemon->Suspend();
  } else if (dbus_message_is_signal(message, kPowerManagerInterface,
                                    kRequestRestartSignal)) {
    LOG(INFO) << "RequestRestart event";
    daemon->OnRequestRestart();
  } else if (dbus_message_is_signal(message, kPowerManagerInterface,
                                    kRequestShutdownSignal)) {
    LOG(INFO) << "RequestShutdown event";
    daemon->OnRequestShutdown();
  } else if (dbus_message_is_signal(message, kPowerManagerInterface,
                                    util::kLidClosed)) {
    LOG(INFO) << "Lid Closed event";
    daemon->SetActive();
    daemon->Suspend();
  } else if (dbus_message_is_signal(message, kPowerManagerInterface,
                                    util::kLidOpened)) {
    LOG(INFO) << "Lid Opened event";
    daemon->SetActive();
    daemon->suspender_.CancelSuspend();
  } else if (dbus_message_is_signal(message, kPowerManagerInterface,
                                    util::kPowerButtonDown)) {
    LOG(INFO) << "Button Down event";
    daemon->power_button_handler_->HandleButtonDown();
  } else if (dbus_message_is_signal(message, kPowerManagerInterface,
                                    util::kPowerButtonUp)) {
    LOG(INFO) << "Button Up event";
    daemon->power_button_handler_->HandleButtonUp();
  } else if (dbus_message_is_signal(message, kPowerManagerInterface,
                                    kCleanShutdown)) {
    LOG(INFO) << "Clean shutdown/restart event";
    if (daemon->clean_shutdown_initiated_) {
      daemon->clean_shutdown_initiated_ = false;
      daemon->Shutdown();
    } else {
      LOG(INFO) << "Received clean shutdown signal, but never asked for it.";
    }
  } else if (dbus_message_is_signal(message, kPowerManagerInterface,
                                    util::kPowerStateChanged)) {
    LOG(INFO) << "Power state change event";
    const char* state = '\0';
    DBusError error;
    dbus_error_init(&error);
    if (dbus_message_get_args(message, &error, DBUS_TYPE_STRING, &state,
                              DBUS_TYPE_INVALID) == FALSE) {
      LOG(WARNING) << "Trouble reading args of PowerStateChange event "
                   << state;
      return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }
    daemon->OnPowerStateChange(state);
    // other dbus clients may be interested in consuming this signal
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
  } else if (dbus_message_is_signal(
                 message,
                 login_manager::kSessionManagerInterface,
                 login_manager::kSessionManagerSessionStateChanged)) {
    DBusError error;
    dbus_error_init(&error);
    const char* state = NULL;
    const char* user = NULL;
    if (dbus_message_get_args(message, &error,
                              DBUS_TYPE_STRING, &state,
                              DBUS_TYPE_STRING, &user,
                              DBUS_TYPE_INVALID)) {
      daemon->OnSessionStateChange(state, user);
    } else {
      LOG(WARNING) << "Unable to read arguments from "
                   << login_manager::kSessionManagerSessionStateChanged
                   << " signal";
    }
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
  } else {
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
  }
  return DBUS_HANDLER_RESULT_HANDLED;
}

void Daemon::RegisterDBusMessageHandler() {
  DBusConnection* connection = dbus_g_connection_get_connection(
      chromeos::dbus::GetSystemBusConnection().g_connection());
  CHECK(connection);

  vector<string> matches;
  matches.push_back(
      StringPrintf("type='signal', interface='%s'", kPowerManagerInterface));
  matches.push_back(
      StringPrintf("type='signal', interface='%s', member='%s'",
                   login_manager::kSessionManagerInterface,
                   login_manager::kSessionManagerSessionStateChanged));

  for (vector<string>::const_iterator it = matches.begin();
       it != matches.end(); ++it) {
    DBusError error;
    dbus_error_init(&error);
    dbus_bus_add_match(connection, it->c_str(), &error);
    if (dbus_error_is_set(&error)) {
      LOG(DFATAL) << "Failed to add match \"" << *it << "\": "
                  << error.name << ", message=" << error.message;
    }
  }

  CHECK(dbus_connection_add_filter(connection, &DBusMessageHandler, this,
                                   NULL));
  LOG(INFO) << "D-Bus monitoring started";
}

void Daemon::OnLowBattery(double battery_percentage) {
  if (kPowerDisconnected == plugged_state_ && !low_battery_ &&
      battery_percentage <= low_battery_suspend_percent_) {
    // Shut the system down when low battery condition is encountered.
    LOG(INFO) << "Low battery condition detected. Shutting down immediately.";
    low_battery_ = true;
    file_tagger_.HandleLowBatteryEvent();
    OnRequestShutdown();
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

gboolean Daemon::CleanShutdownTimedOut(gpointer data) {
  Daemon* daemon = static_cast<Daemon*>(data);
  if (daemon->clean_shutdown_initiated_) {
    daemon->clean_shutdown_initiated_ = false;
    LOG(INFO) << "Timed out waiting for clean shutdown/restart.";
    daemon->Shutdown();
  } else {
    LOG(INFO) << "Shutdown already handled. clean_shutdown_initiated_ == false";
  }
  return false;
}

void Daemon::OnPowerStateChange(const char* state) {
 // on == resume via powerd_suspend
  if (g_str_equal(state, "on") == TRUE) {
    LOG(INFO) << "Resuming has commenced";
    system_state_ = kSystemOn;
    SetActive();
    HandleResume();
  } else {
    DLOG(INFO) << "Saw arg:" << state << " for PowerStateChange";
  }
}

void Daemon::OnSessionStateChange(const char* state, const char* user) {
  if (!state || !user) {
    LOG(DFATAL) << "Got session state change signal with missing state or user";
    return;
  }

  if (strcmp(state, "started") == 0) {
    current_user_ = user;
    DLOG(INFO) << "Session started for "
               << (current_user_.empty() ? "guest" : current_user_.c_str());
  } else if (strcmp(state, "stopped") == 0) {
    current_user_.clear();
    DLOG(INFO) << "Session stopped";
  } else {
    LOG(WARNING) << "Got unexpected state in session state change signal: "
                 << state;
  }
}

void Daemon::Shutdown() {
  if (system_state_ == kSystemShuttingDown) {
    LOG(INFO) << "Shutting down";
    util::SendSignalToPowerM(util::kShutdownSignal);
  } else if (system_state_ == kSystemRestarting) {
    LOG(INFO) << "Restarting";
    util::SendSignalToPowerM(util::kRestartSignal);
  } else {
    LOG(ERROR) << "Shutdown : Improper System State!";
  }
}

void Daemon::Suspend() {
  if (system_state_ == kSystemRestarting || system_state_ == kSystemShuttingDown) {
    LOG(INFO) << "Ignoring request for suspend with outstanding shutdown.";
    return;
  }
  if (util::LoggedIn()) {
    system_state_ = kSystemSuspend;
    suspender_.RequestSuspend();
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
  return true;
}

void Daemon::SendBrightnessChangedSignal(int level) {
  chromeos::dbus::Proxy proxy(chromeos::dbus::GetSystemBusConnection(),
                              "/",
                              kPowerManagerInterface);
  DBusMessage* signal = dbus_message_new_signal("/",
                                                kPowerManagerInterface,
                                                kBrightnessChangedSignal);
  CHECK(signal);
  dbus_message_append_args(signal,
                           DBUS_TYPE_INT32, &level,
                           DBUS_TYPE_INVALID);
  dbus_g_proxy_send(proxy.gproxy(), signal, NULL);
  dbus_message_unref(signal);
}

void Daemon::HandleResume() {
  file_tagger_.HandleResumeEvent();
}

}  // namespace power_manager
