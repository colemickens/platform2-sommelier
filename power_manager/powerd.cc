// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gdk/gdk.h>
#include <gflags/gflags.h>
#include <inttypes.h>
#include <algorithm>
#include <cstdio>
#include "base/logging.h"
#include "base/string_util.h"
#include "power_manager/backlight.h"
#include "power_manager/xidle.h"

DEFINE_int64(dim_ms, 0,
             "Number of milliseconds before dimming screen.");
DEFINE_int64(idle_brightness, -1,
             "Brightness setting to use when user is idle.");

class IdleDimmer : public power_manager::XIdleMonitor {
 public:
  explicit IdleDimmer(int64 idle_brightness);
  bool Init();
  void OnIdleEvent(bool is_idle, int64 idle_time_ms);
 private:
  // Backlight used for idle dimming
  power_manager::Backlight backlight_;

  // Whether the monitor has been dimmed due to inactivity
  bool idle_dim_;

  // The brightness level when the user is idle or active
  int64 idle_brightness_;
  int64 active_brightness_;
};

IdleDimmer::IdleDimmer(int64 idle_brightness)
    : idle_dim_(false),
      idle_brightness_(idle_brightness) {
}

bool IdleDimmer::Init() {
  bool ok = backlight_.Init();
  LOG_IF(WARNING, !ok) << "Cannot initialize backlight class";
  return ok;
}

void IdleDimmer::OnIdleEvent(bool is_idle, int64) {
  int64 level, max_level, new_level;
  if (!backlight_.GetBrightness(&level, &max_level))
    return;
  if (is_idle) {
    if (idle_brightness_ >= level) {
      LOG(INFO) << "Monitor is already dim. Nothing to do.";
      return;
    }
    new_level = idle_brightness_;
    active_brightness_ = level;
    idle_dim_ = true;
    LOG(INFO) << "Dim from " << level << " to " << idle_brightness_
              << " (out of " << max_level << ")";
  } else { // !is_idle
    if (!idle_dim_) {
      LOG(INFO) << "Monitor is already bright. Nothing to do.";
      return;
    }
    int64 diff = level - idle_brightness_;
    new_level = std::min(active_brightness_ + diff, max_level);
    idle_dim_ = false;
    LOG(INFO) << "Brighten from " << level << " to " << new_level
              << " (out of " << max_level << ")";
  }
  backlight_.SetBrightness(new_level);
}

int main(int argc, char* argv[]) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  CHECK(FLAGS_dim_ms) << "--dim_ms is required";
  CHECK(FLAGS_idle_brightness >= 0) << "--idle_brightness is required";
  CHECK(argc == 1) << "Unexpected arguments";
  gdk_init(&argc, &argv);
  power_manager::XIdle idle;
  IdleDimmer monitor(FLAGS_idle_brightness);
  CHECK(monitor.Init()) << "Cannot initialize monitor";
  CHECK(idle.Init(&monitor)) << "Cannot initialize XIdle";
  CHECK(idle.AddIdleTimeout(FLAGS_dim_ms)) << "Cannot add idle timeout";
  GMainLoop* loop = g_main_loop_new(NULL, false);
  g_main_loop_run(loop);
  return 0;
}
