// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd.h"

#include <dbus/dbus-glib-lowlevel.h>
#include <gdk/gdkx.h>
#include <X11/extensions/dpms.h>

#include <algorithm>

#include "base/logging.h"
#include "base/string_util.h"
#include "chromeos/dbus/dbus.h"
#include "chromeos/dbus/service_constants.h"

using std::max;
using std::min;

namespace power_manager {

// The minimum delta between timers to avoid timer precision issues
static const int64 kFuzzMS = 100;

// The minimum delta between timers when we want to give a user time to react
static const int64 kReactMS = 30000;

// Lock the screen
static void SendSignalToSessionManager(const char *signal) {
  DBusGProxy *proxy = dbus_g_proxy_new_for_name(
      chromeos::dbus::GetSystemBusConnection().g_connection(),
      login_manager::kSessionManagerServiceName,
      login_manager::kSessionManagerServicePath,
      login_manager::kSessionManagerInterface);
  CHECK(proxy);
  GError *error = NULL;
  if (!dbus_g_proxy_call(proxy, signal, &error, G_TYPE_INVALID,
                         G_TYPE_INVALID)) {
    LOG(ERROR) << "Error sending signal: " << error->message;
  }
  g_object_unref(proxy);
}

// A message filter to receive signals.
static DBusHandlerResult DBusMessageHandler(DBusConnection*,
                                            DBusMessage* message,
                                            void*) {
  if (dbus_message_is_signal(message, kPowerManagerInterface,
                             kRequestLockScreenSignal)) {
    LOG(INFO) << "RequestLockScreen event";
    SendSignalToSessionManager("LockScreen");
  } else if (dbus_message_is_signal(message, kPowerManagerInterface,
                                    kRequestUnlockScreenSignal)) {
    LOG(INFO) << "RequestUnlockScreen event";
    SendSignalToSessionManager("UnlockScreen");
  } else if (dbus_message_is_signal(message, kPowerManagerInterface,
                                    kScreenIsLockedSignal)) {
    // TODO: Record screen lock/unlock state
    LOG(INFO) << "ScreenIsLocked event";
  } else if (dbus_message_is_signal(message, kPowerManagerInterface,
                                    kScreenIsUnlockedSignal)) {
    // TODO: Record screen lock/unlock state
    LOG(INFO) << "ScreenIsUnlocked event";
  } else {
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
  }
  return DBUS_HANDLER_RESULT_HANDLED;
}

static void RegisterDBusMessageHandler(Daemon *object) {
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
    CHECK(dbus_connection_add_filter(connection, &DBusMessageHandler, object,
                                     NULL));
    LOG(INFO) << "DBus monitoring started";
  }
}

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
      battery_discharge_rate_metric_last_(0),
      battery_remaining_charge_metric_last_(0),
      battery_time_to_empty_metric_last_(0) {}

void Daemon::Init() {
  ReadSettings();
  DbusInit();
  MetricInit();
  if (!DPMSCapable(GDK_DISPLAY())) {
    LOG(WARNING) << "X Server not DPMS capable";
  } else {
    CHECK(DPMSEnable(GDK_DISPLAY()));
    CHECK(DPMSSetTimeouts(GDK_DISPLAY(), 0, 0, 0));
  }
}

void Daemon::ReadSettings() {
  CHECK(prefs_->ReadSetting("plugged_dim_ms", &plugged_dim_ms_));
  CHECK(prefs_->ReadSetting("plugged_off_ms", &plugged_off_ms_));
  CHECK(prefs_->ReadSetting("plugged_suspend_ms", &plugged_suspend_ms_));
  CHECK(prefs_->ReadSetting("unplugged_dim_ms", &unplugged_dim_ms_));
  CHECK(prefs_->ReadSetting("unplugged_off_ms", &unplugged_off_ms_));
  CHECK(prefs_->ReadSetting("unplugged_suspend_ms", &unplugged_suspend_ms_));
  CHECK(prefs_->ReadSetting("lock_ms", &lock_ms_));

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

void Daemon::DbusInit() {
  RegisterDBusMessageHandler(this);
  CHECK(chromeos::MonitorPowerStatus(&OnPowerEvent, this));
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
    dim_ms_ = plugged_dim_ms_ + offset_ms;
    off_ms_ = plugged_off_ms_ + offset_ms;
    suspend_ms_ = plugged_suspend_ms_ + offset_ms;
  } else {
    CHECK(plugged_state_ == kPowerDisconnected);
    dim_ms_ = unplugged_dim_ms_ + offset_ms;
    off_ms_ = unplugged_off_ms_ + offset_ms;
    suspend_ms_ = unplugged_suspend_ms_ + offset_ms;
  }

  // Make sure that the screen turns off before it locks, and dims before
  // it turns off. This ensures the user gets a warning before we lock the
  // screen.
  off_ms_ = min(off_ms_, lock_ms_ - kReactMS);
  dim_ms_ = min(dim_ms_, lock_ms_ - 2 * kReactMS);

  // Sync up idle state with new settings
  CHECK(idle_.ClearTimeouts());
  if (offset_ms > kFuzzMS)
    CHECK(idle_.AddIdleTimeout(kFuzzMS));
  if (kMetricIdleMin + kFuzzMS <= dim_ms_)
    CHECK(idle_.AddIdleTimeout(kMetricIdleMin));
  CHECK(idle_.AddIdleTimeout(dim_ms_));
  CHECK(idle_.AddIdleTimeout(off_ms_));
  if (lock_ms_ + kFuzzMS < suspend_ms_ || lock_ms_ > suspend_ms_ + kFuzzMS) {
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
    LOG(INFO) << "TODO: Suspend computer";
    idle_state_ = kIdleSuspend;
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

  if (idle_time_ms >= lock_ms_) {
    LOG(INFO) << "TODO: Lock computer";
  }
}

void Daemon::OnPowerEvent(void* object, const chromeos::PowerStatus& info) {
  Daemon* daemon = static_cast<Daemon*>(object);
  daemon->SetPlugged(info.line_power_on);
  daemon->GenerateMetricsOnPowerEvent(info);
}

}  // namespace power_manager
