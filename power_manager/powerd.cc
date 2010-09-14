// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd.h"

#include <gdk/gdkx.h>
#include <stdint.h>
#include <X11/extensions/dpms.h>
#include <X11/keysym.h>
#include <X11/XF86keysym.h>

#include <algorithm>

#include "base/logging.h"
#include "base/string_util.h"
#include "chromeos/dbus/dbus.h"
#include "chromeos/dbus/service_constants.h"
#include "power_manager/util.h"

using std::max;
using std::min;

namespace power_manager {

// The minimum delta between timers to avoid timer precision issues
static const int64 kFuzzMS = 100;

// The minimum delta between timers when we want to give a user time to react
static const int64 kReactMS = 30000;

// Daemon: Main power manager. Adjusts device status based on whether the
//         user is idle and on video activity indicator from the window manager.
//         This daemon is responsible for dimming of the backlight, turning
//         the screen off, and suspending to RAM. The daemon also has the
//         capability of shutting the system down.

Daemon::Daemon(BacklightController* ctl, PowerPrefs* prefs,
               MetricsLibraryInterface* metrics_lib,
               VideoDetectorInterface* video_detector)
    : ctl_(ctl),
      prefs_(prefs),
      metrics_lib_(metrics_lib),
      video_detector_(video_detector),
      low_battery_suspend_percent_(0),
      clean_shutdown_initiated_(false),
      low_battery_suspended_(false),
      enforce_lock_(false),
      use_xscreensaver_(false),
      plugged_state_(kPowerUnknown),
      idle_state_(kIdleUnknown),
      suspender_(&locker_),
      battery_discharge_rate_metric_last_(0),
      battery_remaining_charge_metric_last_(0),
      battery_time_to_empty_metric_last_(0) {}

void Daemon::Init() {
  ReadSettings();
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
  locker_.Init(use_xscreensaver_);
  RegisterDBusMessageHandler();
  CHECK(chromeos::MonitorPowerStatus(&OnPowerEvent, this));
}

void Daemon::ReadSettings() {
  int64 use_xscreensaver, enforce_lock;
  int64 disable_idle_suspend;
  int64 low_battery_suspend_percent;
  CHECK(prefs_->ReadSetting("low_battery_suspend_percent",
                            &low_battery_suspend_percent));
  CHECK(prefs_->ReadSetting("clean_shutdown_timeout_ms",
                            &clean_shutdown_timeout_ms_));
  CHECK(prefs_->ReadSetting("plugged_dim_ms", &plugged_dim_ms_));
  CHECK(prefs_->ReadSetting("plugged_off_ms", &plugged_off_ms_));
  CHECK(prefs_->ReadSetting("plugged_suspend_ms", &plugged_suspend_ms_));
  CHECK(prefs_->ReadSetting("unplugged_dim_ms", &unplugged_dim_ms_));
  CHECK(prefs_->ReadSetting("unplugged_off_ms", &unplugged_off_ms_));
  CHECK(prefs_->ReadSetting("unplugged_suspend_ms", &unplugged_suspend_ms_));
  CHECK(prefs_->ReadSetting("lock_ms", &default_lock_ms_));
  CHECK(prefs_->ReadSetting("enforce_lock", &enforce_lock));
  CHECK(prefs_->ReadSetting("use_xscreensaver", &use_xscreensaver));
  if (prefs_->ReadSetting("disable_idle_suspend", &disable_idle_suspend) &&
      disable_idle_suspend) {
    LOG(INFO) << "Idle suspend feature disabled";
    plugged_suspend_ms_ = INT64_MAX;
    unplugged_suspend_ms_ = INT64_MAX;
  }
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

  CHECK(idle_.Init(this));
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
    if (idle_state_ == kIdleNormal || idle_state_ == kIdleDim)
      SetIdleOffset(idle_time_ms);
    else
      SetIdleOffset(0);

    ctl_->OnPlugEvent(plugged);
    SetIdleState(idle_time_ms);
  }
}

void Daemon::SetIdleOffset(int64 offset_ms) {
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

void Daemon::OnIdleEvent(bool is_idle, int64 idle_time_ms) {
  CHECK(plugged_state_ != kPowerUnknown);
  if (is_idle && kIdleNormal == idle_state_ && !locker_.is_locked()) {
    int64 video_time_ms = 0;
    bool video_is_playing = false;
    int64 dim_timeout = kPowerConnected == plugged_state_ ? plugged_dim_ms_ :
                                                            unplugged_dim_ms_;
    CHECK(video_detector_->GetVideoActivity(dim_timeout,
                                            &video_time_ms,
                                            &video_is_playing));
    if (video_is_playing)
      SetIdleOffset(idle_time_ms - video_time_ms);
  }
  GenerateMetricsOnIdleEvent(is_idle, idle_time_ms);
  SetIdleState(idle_time_ms);
  if (!is_idle && offset_ms_ != 0)
    SetIdleOffset(0);
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
    idle_state_ = kIdleNormal;
  }

  if (idle_time_ms >= lock_ms_ && util::LoggedIn()) {
    locker_.LockScreen();
  }
}

void Daemon::OnPowerEvent(void* object, const chromeos::PowerStatus& info) {
  Daemon* daemon = static_cast<Daemon*>(object);
  daemon->SetPlugged(info.line_power_on);
  daemon->GenerateMetricsOnPowerEvent(info);
  // Do not emergency suspend if no battery exists.
  if (info.battery_is_present)
    daemon->SuspendOnLowBattery(info.battery_percentage);
}

GdkFilterReturn Daemon::gdk_event_filter(GdkXEvent* gxevent, GdkEvent*,
                                         gpointer data) {
  Daemon* daemon = static_cast<Daemon*>(data);
  XEvent* xevent = static_cast<XEvent*>(gxevent);
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
    } else if (keycode == daemon->key_brightness_down_ ||
               keycode == daemon->key_f6_) {
      if (keycode == daemon->key_brightness_down_) {
        LOG(INFO) << "Key press: Brightness down";
      } else {  // keycode == daemon->key_f6_
        LOG(INFO) << "Key press: F6";
      }
      daemon->ctl_->DecreaseBrightness();
    }
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
                                    kRequestShutdownSignal)) {
    LOG(INFO) << "RequestShutdown event";
    daemon->StartCleanShutdown();
  } else if (dbus_message_is_signal(message, kPowerManagerInterface,
                                    util::kLidClosed)) {
    LOG(INFO) << "Lid Closed event";
    daemon->Suspend();
  } else if (dbus_message_is_signal(message, kPowerManagerInterface,
                                    util::kLidOpened)) {
    LOG(INFO) << "Lid Opened event";
    daemon->suspender_.CancelSuspend();
  } else if (dbus_message_is_signal(message, kPowerManagerInterface,
                                    util::kPowerButtonDown)) {
    LOG(INFO) << "Button Down event";
  } else if (dbus_message_is_signal(message, kPowerManagerInterface,
                                    util::kPowerButtonUp)) {
    LOG(INFO) << "Button Up event";
    if (daemon->idle_state_ == kIdleNormal ||
        daemon->idle_state_ == kIdleDim) {
      LOG(INFO) << "Starting Clean Shutdown";
      daemon->StartCleanShutdown();
    } else {
      LOG(INFO) << "Power button struck while screen off.";
    }
  } else if (dbus_message_is_signal(message, kPowerManagerInterface,
                                    kCleanShutdown)) {
    LOG(INFO) << "Clean shutdown event";
    if (daemon->clean_shutdown_initiated_) {
      daemon->clean_shutdown_initiated_ = false;
      LOG(INFO) << "Shutting down";
      daemon->Shutdown();
    } else {
      LOG(INFO) << "Received clean shutdown signal, but never asked for it.";
    }
  } else {
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
  }
  return DBUS_HANDLER_RESULT_HANDLED;
}

void Daemon::RegisterDBusMessageHandler() {
  const std::string filter = StringPrintf("type='signal', interface='%s'",
                                          kPowerManagerInterface);

  DBusError error;
  dbus_error_init(&error);
  DBusConnection* connection = dbus_g_connection_get_connection(
      chromeos::dbus::GetSystemBusConnection().g_connection());
  dbus_bus_add_match(connection, filter.c_str(), &error);
  if (dbus_error_is_set(&error)) {
    LOG(ERROR) << "Failed to add a filter:" << error.name << ", message="
               << error.message;
    NOTREACHED();
  } else {
    CHECK(dbus_connection_add_filter(connection, &DBusMessageHandler, this,
                                     NULL));
    LOG(INFO) << "DBus monitoring started";
  }
}

void Daemon::SuspendOnLowBattery(double battery_percentage) {
  if (kPowerDisconnected == plugged_state_ && !low_battery_suspended_ &&
      battery_percentage <= low_battery_suspend_percent_) {
    LOG(INFO) << "Low battery condition detected. Suspending immediately.";
    low_battery_suspended_ = true;
    Suspend();
  } else if (kPowerConnected == plugged_state_ ||
             battery_percentage > low_battery_suspend_percent_ ) {
    low_battery_suspended_ = false;
  } else {
    // Either a spurious reading after we have requested suspend, or the user
    // has woken the system up intentionally without rectifying the battery
    // situation (ie. user woke the system without attaching AC.)
    // User is on his own from here until the system dies. We will not try to
    // resuspend.
    LOG(INFO) << "Spurious low battery condition, or user living on the edge.";
  }
}

gboolean Daemon::CleanShutdownTimedOut(gpointer data) {
  Daemon* daemon = static_cast<Daemon*>(data);
  if (daemon->clean_shutdown_initiated_) {
    daemon->clean_shutdown_initiated_ = false;
    LOG(INFO) << "Timed out waiting for clean shutdown. Shutting down.";
    daemon->Shutdown();
  } else {
    LOG(INFO) << "Shutdown already handled. clean_shutdown_initiated_ == false";
  }
  return false;
}

void Daemon::StartCleanShutdown() {
  clean_shutdown_initiated_ = true;
  // Cancel any outstanding suspend in flight.
  suspender_.CancelSuspend();
  util::SendSignalToPowerM(util::kRequestCleanShutdown);
  g_timeout_add(clean_shutdown_timeout_ms_, Daemon::CleanShutdownTimedOut,
                this);
}

void Daemon::Shutdown() {
  util::SendSignalToPowerM(util::kShutdownSignal);
}

void Daemon::Suspend() {
  if (clean_shutdown_initiated_) {
    LOG(INFO) << "Ignoring request for suspend with outstanding shutdown.";
    return;
  }
  if (util::LoggedIn()) {
    suspender_.RequestSuspend();
  } else {
    LOG(INFO) << "Not logged in. Suspend Request -> Shutting down.";
    StartCleanShutdown();
  }
}

}  // namespace power_manager
