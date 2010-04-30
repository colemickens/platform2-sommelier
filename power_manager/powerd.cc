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
#include "power_manager/xidle.h"

using std::string;

// powerd: Main power manager. Adjusts device status based on whether the
//         user is idle. Currently, this daemon just adjusts the backlight,
//         but in future it will handle other tasks such as turning off
//         the screen and suspending to RAM.

DEFINE_int64(dim_ms, 0,
             "Number of milliseconds before dimming screen.");
DEFINE_int64(off_ms, 0,
             "Number of milliseconds before turning the screen off.");
DEFINE_int64(lock_ms, 0,
             "Number of milliseconds before locking the screen.");
DEFINE_int64(suspend_ms, 0,
             "Number of milliseconds before suspending to RAM.");
DEFINE_int64(plugged_brightness, -1,
             "Brightness when plugged in.");
DEFINE_int64(unplugged_brightness, -1,
             "Brightness when unplugged.");

static void OnPlugEvent(void* object, const chromeos::PowerStatus& info) {
  power_manager::BacklightController* plug =
      static_cast<power_manager::BacklightController*>(object);
  plug->OnPlugEvent(info.line_power_on);
}

class IdleMonitor : public power_manager::XIdleMonitor {
 public:
  IdleMonitor(power_manager::BacklightController* ctl) :
      ctl_(ctl) { }
  void OnIdleEvent(bool is_idle, int64 idle_time_ms);
 private:
  power_manager::BacklightController* ctl_;
};

void IdleMonitor::OnIdleEvent(bool is_idle, int64 idle_time_ms) {
  if (!is_idle) {
    ctl_->SetBacklightState(power_manager::BACKLIGHT_ACTIVE);
  } else if (idle_time_ms >= FLAGS_suspend_ms) {
    LOG(INFO) << "TODO: Suspend computer";
  } else if (idle_time_ms >= FLAGS_lock_ms) {
    LOG(INFO) << "TODO: Lock computer";
  } else if (idle_time_ms >= FLAGS_off_ms) {
    LOG(INFO) << "TODO: Turn off backlight";
  } else {
    ctl_->SetBacklightState(power_manager::BACKLIGHT_DIM);
  }
}

int main(int argc, char* argv[]) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  CHECK(FLAGS_dim_ms) << "--dim_ms is required";
  CHECK(FLAGS_off_ms) << "--off_ms is required";
  CHECK(FLAGS_lock_ms) << "--lock_ms is required";
  CHECK(FLAGS_suspend_ms) << "--suspend_ms is required";
  CHECK(FLAGS_off_ms > FLAGS_dim_ms);
  CHECK(FLAGS_lock_ms > FLAGS_off_ms);
  CHECK(FLAGS_suspend_ms > FLAGS_off_ms);
  CHECK(FLAGS_plugged_brightness >= 0)
      << "--plugged_brightness is required";
  CHECK(FLAGS_unplugged_brightness >= 0)
      << "--unplugged_brightness is required";
  CHECK(argc == 1) << "Unexpected arguments. Try --help";
  string err;
  CHECK(chromeos::LoadLibcros(chromeos::kCrosDefaultPath, err))
      << "LoadLibcros('" << chromeos::kCrosDefaultPath << "') failed: " << err;
  gdk_init(&argc, &argv);
  power_manager::Backlight backlight;
  CHECK(backlight.Init()) << "Cannot initialize backlight";
  power_manager::BacklightController backlight_ctl(&backlight);
  CHECK(backlight_ctl.Init()) << "Cannot initialize backlight controller";
  power_manager::XIdle idle;
  backlight_ctl.set_plugged_brightness_offset(FLAGS_plugged_brightness);
  backlight_ctl.set_unplugged_brightness_offset(FLAGS_unplugged_brightness);
  chromeos::PowerStatusConnection connection =
      chromeos::MonitorPowerStatus(&OnPlugEvent, &backlight_ctl);
  CHECK(connection) << "Cannot establish power status connection";
  IdleMonitor monitor(&backlight_ctl);
  CHECK(idle.Init(&monitor)) << "Cannot initialize XIdle";
  CHECK(idle.AddIdleTimeout(FLAGS_dim_ms)) << "Cannot add idle timeout";
  CHECK(idle.AddIdleTimeout(FLAGS_off_ms)) << "Cannot add idle timeout";
  CHECK(idle.AddIdleTimeout(FLAGS_lock_ms)) << "Cannot add idle timeout";
  CHECK(idle.AddIdleTimeout(FLAGS_suspend_ms)) << "Cannot add idle timeout";
  GMainLoop* loop = g_main_loop_new(NULL, false);
  g_main_loop_run(loop);
  return 0;
}
