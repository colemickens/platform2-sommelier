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
//         user is idle. Currently, this daemon just adjusts the backlight,
//         but in future it will handle other tasks such as turning off
//         the screen and suspending to RAM.

Daemon::Daemon(BacklightController* ctl, PowerPrefs* prefs,
               MetricsLibraryInterface* metrics_lib)
    : ctl_(ctl),
      prefs_(prefs),
      metrics_lib_(metrics_lib),
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
  key_brightness_up_ = XKeysymToKeycode(GDK_DISPLAY(), XF86XK_MonBrightnessUp);
  key_brightness_down_ = XKeysymToKeycode(GDK_DISPLAY(),
                                          XF86XK_MonBrightnessDown);
  key_power_off_ = XKeysymToKeycode(GDK_DISPLAY(), XF86XK_PowerOff);
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
  if (key_power_off_ != 0) {
    GrabKey(key_power_off_, 0);
  } else {
    LOG(ERROR) << "No power off keycode found.";
  }
  gdk_window_add_filter(NULL, gdk_event_filter, this);
  locker_.Init(use_xscreensaver_);
  RegisterDBusMessageHandler();
  CHECK(chromeos::MonitorPowerStatus(&OnPowerEvent, this));
}

void Daemon::ReadSettings() {
  int64 use_xscreensaver;
  int64 disable_idle_suspend;
  CHECK(prefs_->ReadSetting("plugged_dim_ms", &plugged_dim_ms_));
  CHECK(prefs_->ReadSetting("plugged_off_ms", &plugged_off_ms_));
  CHECK(prefs_->ReadSetting("plugged_suspend_ms", &plugged_suspend_ms_));
  CHECK(prefs_->ReadSetting("unplugged_dim_ms", &unplugged_dim_ms_));
  CHECK(prefs_->ReadSetting("unplugged_off_ms", &unplugged_off_ms_));
  CHECK(prefs_->ReadSetting("unplugged_suspend_ms", &unplugged_suspend_ms_));
  CHECK(prefs_->ReadSetting("lock_ms", &lock_ms_));
  CHECK(prefs_->ReadSetting("use_xscreensaver", &use_xscreensaver));
  if (prefs_->ReadSetting("disable_idle_suspend", &disable_idle_suspend) &&
      disable_idle_suspend) {
    LOG(INFO) << "Idle suspend feature disabled";
    plugged_suspend_ms_ = INT64_MAX;
    unplugged_suspend_ms_ = INT64_MAX;
  }
  use_xscreensaver_ = use_xscreensaver;

  // Check that timeouts are sane
  CHECK(kMetricIdleMin >= kFuzzMS);
  CHECK(plugged_dim_ms_ >= kReactMS);
  CHECK(plugged_off_ms_ >= plugged_dim_ms_ + kReactMS);
  CHECK(plugged_suspend_ms_ >= plugged_off_ms_ + kReactMS);
  CHECK(unplugged_dim_ms_ >= kReactMS);
  CHECK(unplugged_off_ms_ >= unplugged_dim_ms_ + kReactMS);
  CHECK(unplugged_suspend_ms_ >= unplugged_off_ms_ + kReactMS);
  CHECK(lock_ms_ >= unplugged_off_ms_ + kReactMS);
  CHECK(lock_ms_ >= plugged_off_ms_ + kReactMS);

  CHECK(idle_.Init(this));
}

void Daemon::Run() {
  GMainLoop* loop = g_main_loop_new(NULL, false);
  g_main_loop_run(loop);
}

void Daemon::SetPlugged(bool plugged) {
  if (plugged != plugged_state_) {
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

  // Protect against overflow
  dim_ms_ = max(dim_ms_ + offset_ms, dim_ms_);
  off_ms_ = max(off_ms_ + offset_ms, off_ms_);
  suspend_ms_ = max(suspend_ms_ + offset_ms, suspend_ms_);

  // Make sure that the screen turns off before it locks, and dims before
  // it turns off. This ensures the user gets a warning before we lock the
  // screen.
  off_ms_ = min(off_ms_, lock_ms_ - kReactMS);
  dim_ms_ = min(dim_ms_, lock_ms_ - 2 * kReactMS);

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
  GenerateMetricsOnIdleEvent(is_idle, idle_time_ms);
  SetIdleState(idle_time_ms);
  if (!is_idle && offset_ms_ != 0)
    SetIdleOffset(0);
}

void Daemon::SetIdleState(int64 idle_time_ms) {
  if (idle_time_ms >= suspend_ms_) {
    LOG(INFO) << "state = kIdleSuspend";
    idle_state_ = kIdleSuspend;
    suspender_.RequestSuspend();
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
}

GdkFilterReturn Daemon::gdk_event_filter(GdkXEvent* gxevent, GdkEvent*,
                                         gpointer data) {
  Daemon* daemon = static_cast<Daemon*>(data);
  XEvent* xevent = static_cast<XEvent*>(gxevent);
  if (xevent->type == KeyPress) {
    int keycode = xevent->xkey.keycode;
    if (keycode == daemon->key_power_off_ &&
        (daemon->idle_state_ == kIdleNormal ||
         daemon->idle_state_ == kIdleDim)) {
      // TODO(davidjames): Switch to using the reboot.2 system call when we
      // have CAP_SYS_BOOT permissions.
      util::Launch("sudo shutdown -P now");
    } else if (keycode == daemon->key_brightness_up_ ||
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
  Window root = DefaultRootWindow(GDK_DISPLAY());
  XGrabKey(GDK_DISPLAY(), key, mask, root, True, GrabModeAsync, GrabModeAsync);
  XGrabKey(GDK_DISPLAY(), key, mask | LockMask, root, True,
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
    // TODO(davidjames): Check lid state
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
    daemon->suspender_.RequestSuspend();
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

}  // namespace power_manager
