// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gflags/gflags.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <string>

#include "base/logging.h"
#include "cros/chromeos_cros_api.h"
#include "cros/chromeos_power.h"
#include "power_manager/backlight.h"
#include "power_manager/backlight_controller.h"
#include "power_manager/power_prefs.h"
#include "power_manager/xidle.h"

using std::string;

// powerd: Main power manager. Adjusts device status based on whether the
//         user is idle. Currently, this daemon just adjusts the backlight,
//         but in future it will handle other tasks such as turning off
//         the screen and suspending to RAM.

DEFINE_string(prefs_dir, "",
              "Directory to store logs and settings.");

class IdleMonitor : public power_manager::XIdleMonitor {
 public:
  IdleMonitor(power_manager::BacklightController* ctl,
              power_manager::PowerPrefs* prefs);
  void Init();
  void OnIdleEvent(bool is_idle, int64 idle_time_ms);
  void SetPlugged(bool plugged);
 private:
  power_manager::BacklightController* ctl_;
  power_manager::XIdle idle;
  int64 plugged_dim_ms_, plugged_off_ms_, plugged_suspend_ms_,
        unplugged_dim_ms_, unplugged_off_ms_, unplugged_suspend_ms_,
        dim_ms_, off_ms_, suspend_ms_;
  enum { POWER_DISCONNECTED, POWER_CONNECTED, POWER_UNKNOWN } plugged_state_;
};

IdleMonitor::IdleMonitor(power_manager::BacklightController* ctl,
                         power_manager::PowerPrefs* prefs) :
    ctl_(ctl), plugged_state_(POWER_UNKNOWN) {
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

void IdleMonitor::Init() {
  CHECK(idle.Init(this));
}

void IdleMonitor::SetPlugged(bool plugged) {
  if (plugged != plugged_state_) {
    if (plugged) {
      plugged_state_ = POWER_CONNECTED;
      dim_ms_ = plugged_dim_ms_;
      off_ms_ = plugged_off_ms_;
      suspend_ms_ = plugged_suspend_ms_;
    } else {
      plugged_state_ = POWER_DISCONNECTED;
      dim_ms_ = unplugged_dim_ms_;
      off_ms_ = unplugged_off_ms_;
      suspend_ms_ = unplugged_suspend_ms_;
    }
    CHECK(idle.ClearTimeouts());
    CHECK(idle.AddIdleTimeout(dim_ms_));
    CHECK(idle.AddIdleTimeout(off_ms_));
    CHECK(idle.AddIdleTimeout(suspend_ms_));
    ctl_->OnPlugEvent(plugged);

    // Sync up idle state with new settings
    int64 idle_time_ms;
    CHECK(idle.GetIdleTime(&idle_time_ms));
    OnIdleEvent(idle_time_ms >= dim_ms_, idle_time_ms);
  }
}

void IdleMonitor::OnIdleEvent(bool is_idle, int64 idle_time_ms) {
  CHECK(plugged_state_ != POWER_UNKNOWN);
  if (!is_idle) {
    LOG(INFO) << "User is active";
    ctl_->SetBacklightState(power_manager::BACKLIGHT_ACTIVE);
  } else if (idle_time_ms >= dim_ms_) {
    LOG(INFO) << "User is idle";
    ctl_->SetBacklightState(power_manager::BACKLIGHT_DIM);
    if (idle_time_ms >= off_ms_) {
      LOG(INFO) << "TODO: Turn off backlight";
    }
    if (idle_time_ms >= suspend_ms_) {
      LOG(INFO) << "TODO: Suspend computer";
    }
  }
}

static void OnPlugEvent(void* object, const chromeos::PowerStatus& info) {
  IdleMonitor* monitor = static_cast<IdleMonitor*>(object);
  monitor->SetPlugged(info.line_power_on);
}

int main(int argc, char* argv[]) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  CHECK(!FLAGS_prefs_dir.empty()) << "--prefs_dir is required";
  CHECK(argc == 1) << "Unexpected arguments. Try --help";
  FilePath prefs_dir(FLAGS_prefs_dir);
  power_manager::PowerPrefs prefs(prefs_dir);
  string err;
  CHECK(chromeos::LoadLibcros(chromeos::kCrosDefaultPath, err))
      << "LoadLibcros('" << chromeos::kCrosDefaultPath << "') failed: " << err;
  gdk_init(&argc, &argv);
  power_manager::Backlight backlight;
  CHECK(backlight.Init()) << "Cannot initialize backlight";
  power_manager::BacklightController backlight_ctl(&backlight, &prefs);
  CHECK(backlight_ctl.Init()) << "Cannot initialize backlight controller";
  IdleMonitor monitor(&backlight_ctl, &prefs);
  monitor.Init();
  chromeos::PowerStatusConnection connection =
      chromeos::MonitorPowerStatus(&OnPlugEvent, &monitor);
  CHECK(connection) << "Cannot establish power status connection";
  GMainLoop* loop = g_main_loop_new(NULL, false);
  g_main_loop_run(loop);
  return 0;
}
