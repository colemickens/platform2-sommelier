// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gdk/gdk.h>
#include <gflags/gflags.h>
#include <algorithm>
#include "base/logging.h"
#include "power_manager/backlight.h"
#include "power_manager/idle_dimmer.h"
#include "power_manager/xidle.h"

// powerd: Main power manager. Adjusts device status based on whether the
//         user is idle. Currently, this daemon just adjusts the backlight,
//         but in future it will handle other tasks such as turning off
//         the screen and suspending to RAM.

DEFINE_int64(dim_ms, 0,
             "Number of milliseconds before dimming screen.");
DEFINE_int64(idle_brightness, -1,
             "Brightness setting to use when user is idle.");

int main(int argc, char* argv[]) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  CHECK(FLAGS_dim_ms) << "--dim_ms is required";
  CHECK(FLAGS_idle_brightness >= 0) << "--idle_brightness is required";
  CHECK(argc == 1) << "Unexpected arguments. Try --help";
  gdk_init(&argc, &argv);
  power_manager::Backlight backlight;
  power_manager::XIdle idle;
  CHECK(backlight.Init()) << "Cannot initialize backlight";
  power_manager::IdleDimmer monitor(FLAGS_idle_brightness, &backlight);
  CHECK(idle.Init(&monitor)) << "Cannot initialize XIdle";
  CHECK(idle.AddIdleTimeout(FLAGS_dim_ms)) << "Cannot add idle timeout";
  GMainLoop* loop = g_main_loop_new(NULL, false);
  g_main_loop_run(loop);
  return 0;
}
