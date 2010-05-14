// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd.h"

#include "base/logging.h"

namespace power_manager {

// Daemon: Main power manager. Adjusts device status based on whether the
//         user is idle. Currently, this daemon just adjusts the backlight,
//         but in future it will handle other tasks such as turning off
//         the screen and suspending to RAM.

Daemon::Daemon(BacklightController* ctl, PowerPrefs* prefs)
    : ctl_(ctl), plugged_state_(kPowerUnknown) {
  CHECK(prefs->ReadSetting("plugged_dim_ms", &plugged_dim_ms_));
  CHECK(prefs->ReadSetting("plugged_off_ms", &plugged_off_ms_));
  CHECK(prefs->ReadSetting("plugged_suspend_ms", &plugged_suspend_ms_));
  CHECK(prefs->ReadSetting("unplugged_dim_ms", &unplugged_dim_ms_));
  CHECK(prefs->ReadSetting("unplugged_off_ms", &unplugged_off_ms_));
  CHECK(prefs->ReadSetting("unplugged_suspend_ms", &unplugged_suspend_ms_));

  // Check that timeouts are sane
  int64 fuzz = 500;
  CHECK(plugged_dim_ms_ > fuzz);
  CHECK(plugged_off_ms_ > plugged_dim_ms_ + fuzz);
  CHECK(plugged_suspend_ms_ > plugged_off_ms_ + fuzz);
  CHECK(unplugged_dim_ms_ > fuzz);
  CHECK(unplugged_off_ms_ > unplugged_dim_ms_ + fuzz);
  CHECK(unplugged_suspend_ms_ > unplugged_off_ms_ + fuzz);
}

void Daemon::Init() {
  CHECK(idle_.Init(this));
}

void Daemon::Run() {
  chromeos::PowerStatusConnection connection =
      chromeos::MonitorPowerStatus(&OnPowerEvent, this);
  CHECK(connection) << "Cannot establish power status connection";
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
    CHECK(idle_.AddIdleTimeout(dim_ms_));
    CHECK(idle_.AddIdleTimeout(off_ms_));
    CHECK(idle_.AddIdleTimeout(suspend_ms_));
    ctl_->OnPlugEvent(plugged);

    // Sync up idle state with new settings
    int64 idle_time_ms;
    CHECK(idle_.GetIdleTime(&idle_time_ms));
    OnIdleEvent(idle_time_ms >= dim_ms_, idle_time_ms);
  }
}

void Daemon::OnIdleEvent(bool is_idle, int64 idle_time_ms) {
  CHECK(plugged_state_ != kPowerUnknown);
  if (!is_idle) {
    LOG(INFO) << "User is active";
    ctl_->SetBacklightState(BACKLIGHT_ACTIVE);
  } else if (idle_time_ms >= dim_ms_) {
    LOG(INFO) << "User is idle";
    ctl_->SetBacklightState(BACKLIGHT_DIM);
    if (idle_time_ms >= off_ms_) {
      LOG(INFO) << "TODO: Turn off backlight";
    }
    if (idle_time_ms >= suspend_ms_) {
      LOG(INFO) << "TODO: Suspend computer";
    }
  }
}

void Daemon::OnPowerEvent(void* object, const chromeos::PowerStatus& info) {
  Daemon* daemon = static_cast<Daemon*>(object);
  daemon->SetPlugged(info.line_power_on);
}

}  // namespace power_manager
