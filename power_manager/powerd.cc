// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gdk/gdk.h>
#include <gflags/gflags.h>
#include <algorithm>
#include "base/logging.h"
#include "power_manager/backlight.h"
#include "power_manager/xidle.h"

DEFINE_int64(dim_ms, 0,
             "Number of milliseconds before dimming screen.");
DEFINE_int64(idle_brightness, -1,
             "Brightness setting to use when user is idle.");

using std::min;

namespace power_manager {

// \brief Dim the screen when the user becomes idle.
class IdleDimmer : public XIdleMonitor {
 public:
  // Constructor. When the user becomes idle, use the provided backlight
  // object to set the brightness to idle_brightness. When the user becomes
  // active again, restore the user's brightness settings to normal.
  explicit IdleDimmer(int64 idle_brightness, BacklightInterface* backlight);

  void OnIdleEvent(bool is_idle, int64 idle_time_ms);

 private:
  // Backlight used for idle dimming. Non-owned.
  BacklightInterface* backlight_;

  // Whether the monitor has been dimmed due to inactivity.
  bool idle_dim_;

  // The brightness level when the user is idle or active.
  int64 idle_brightness_;
  int64 active_brightness_;
};

IdleDimmer::IdleDimmer(int64 idle_brightness, BacklightInterface* backlight)
    : idle_dim_(false),
      idle_brightness_(idle_brightness),
      backlight_(backlight) {
}

void IdleDimmer::OnIdleEvent(bool is_idle, int64) {
  int64 level, max_level, new_level;
  if (!backlight_->GetBrightness(&level, &max_level))
    return;
  if (is_idle) {
    if (idle_brightness_ >= level) {
      LOG(INFO) << "Monitor is already dim. Nothing to do.";
      return;
    }
    if (idle_dim_) {
      LOG(WARNING) << "Ignoring duplicate idle event.";
      return;
    }
    new_level = idle_brightness_;
    active_brightness_ = level;
    idle_dim_ = true;
    LOG(INFO) << "Dim from " << level << " to " << idle_brightness_
              << " (out of " << max_level << ")";
  } else {  // !is_idle
    if (!idle_dim_) {
      LOG(INFO) << "Monitor is already bright. Nothing to do.";
      return;
    }
    int64 diff = level - idle_brightness_;
    new_level = min(active_brightness_ + diff, max_level);
    idle_dim_ = false;
    LOG(INFO) << "Brighten from " << level << " to " << new_level
              << " (out of " << max_level << ")";
  }
  backlight_->SetBrightness(new_level);
}

}  // namespace power_manager

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
