// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd.h"

#include <dbus/dbus-glib-lowlevel.h>

#include "base/logging.h"
#include "base/string_util.h"
#include "chromeos/dbus/dbus.h"
#include "chromeos/dbus/service_constants.h"

namespace power_manager {

static const int64 kFuzzMS = 100;

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
  TimerInit();
  DbusInit();
}

void Daemon::TimerInit() {
  CHECK(prefs_->ReadSetting("plugged_dim_ms", &plugged_dim_ms_));
  CHECK(prefs_->ReadSetting("plugged_off_ms", &plugged_off_ms_));
  CHECK(prefs_->ReadSetting("plugged_suspend_ms", &plugged_suspend_ms_));
  CHECK(prefs_->ReadSetting("unplugged_dim_ms", &unplugged_dim_ms_));
  CHECK(prefs_->ReadSetting("unplugged_off_ms", &unplugged_off_ms_));
  CHECK(prefs_->ReadSetting("unplugged_suspend_ms", &unplugged_suspend_ms_));
  CHECK(prefs_->ReadSetting("lock_ms", &lock_ms_));

  // Check that timeouts are sane
  CHECK(kMetricIdleMin >= kFuzzMS);
  CHECK(plugged_dim_ms_ >= kFuzzMS);
  CHECK(plugged_off_ms_ >= plugged_dim_ms_ + kFuzzMS);
  CHECK(plugged_suspend_ms_ >= plugged_off_ms_ + kFuzzMS);
  CHECK(unplugged_dim_ms_ >= kFuzzMS);
  CHECK(unplugged_off_ms_ >= unplugged_dim_ms_ + kFuzzMS);
  CHECK(unplugged_suspend_ms_ >= unplugged_off_ms_ + kFuzzMS);
  CHECK(lock_ms_ >= unplugged_off_ms_ + kFuzzMS);
  CHECK(lock_ms_ >= plugged_off_ms_ + kFuzzMS);

  // If lock_ms_ and suspend_ms_ are close (but not equal), then we run into
  // timer precision problems, so we exclude that case via a CHECK.
  CHECK(lock_ms_ == unplugged_suspend_ms_ ||
        lock_ms_ + kFuzzMS <= unplugged_suspend_ms_ ||
        unplugged_suspend_ms_ + kFuzzMS <= lock_ms_);
  CHECK(lock_ms_ == plugged_suspend_ms_ ||
        lock_ms_ + kFuzzMS <= plugged_suspend_ms_ ||
        plugged_suspend_ms_ + kFuzzMS <= lock_ms_);

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
    if (plugged) {
      plugged_state_ = kPowerConnected;
      dim_ms_ = plugged_dim_ms_;
      off_ms_ = plugged_off_ms_;
      suspend_ms_ = plugged_suspend_ms_;
    } else {
      plugged_state_ = kPowerDisconnected;
      dim_ms_ = unplugged_dim_ms_;
      off_ms_ = unplugged_off_ms_;
      suspend_ms_ = unplugged_suspend_ms_;
    }
    CHECK(idle_.ClearTimeouts());
    if (kMetricIdleMin + kFuzzMS <= dim_ms_)
      CHECK(idle_.AddIdleTimeout(kMetricIdleMin));
    CHECK(idle_.AddIdleTimeout(dim_ms_));
    CHECK(idle_.AddIdleTimeout(off_ms_));
    if (lock_ms_ != suspend_ms_)
      CHECK(idle_.AddIdleTimeout(lock_ms_));
    CHECK(idle_.AddIdleTimeout(suspend_ms_));
    ctl_->OnPlugEvent(plugged);

    // Sync up idle state with new settings
    int64 idle_time_ms;
    CHECK(idle_.GetIdleTime(&idle_time_ms));
    OnIdleEvent(idle_time_ms >= dim_ms_ || idle_time_ms >= kMetricIdleMin,
                idle_time_ms);
  }
}

void Daemon::OnIdleEvent(bool is_idle, int64 idle_time_ms) {
  CHECK(plugged_state_ != kPowerUnknown);
  GenerateMetricsOnIdleEvent(is_idle, idle_time_ms);
  if (idle_time_ms >= suspend_ms_) {
    LOG(INFO) << "TODO: Suspend computer";
    idle_state_ = kIdleSuspend;
  } else if (idle_time_ms >= off_ms_) {
    LOG(INFO) << "TODO: Turn off backlight";
    idle_state_ = kIdleScreenOff;
  } else if (idle_time_ms >= dim_ms_) {
    LOG(INFO) << "TODO: Turn on backlight";
    ctl_->SetBacklightState(BACKLIGHT_DIM);
    idle_state_ = kIdleDim;
  } else {
    LOG(INFO) << "User is active";
    LOG(INFO) << "TODO: Turn on backlight";
    ctl_->SetBacklightState(BACKLIGHT_ACTIVE);
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
