// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gflags/gflags.h>
#include <inttypes.h>

#include <cmath>
#include <cstdio>

#include "base/logging.h"
#include "base/time.h"
#include "power_manager/common/power_constants.h"
#include "power_manager/powerd/system/internal_backlight.h"

DEFINE_bool(get_brightness, false, "Get current brightness level.");
DEFINE_bool(get_brightness_percent, false, "Get current brightness percent.");
DEFINE_bool(get_max_brightness, false, "Get max brightness level.");
DEFINE_int64(set_brightness, -1, "Set brightness level.");
DEFINE_double(set_brightness_percent, -1.0,
              "Set brightness as a percent in [0.0, 100.0].");
DEFINE_int64(set_resume_brightness, -1, "Set brightness level on resume.");
DEFINE_double(set_resume_brightness_percent, -1.0,
              "Set resume brightness as a percent in [0.0, 100.0].");

namespace {

int64 PercentToLevel(power_manager::system::BacklightInterface& backlight,
                     double percent) {
  percent = std::min(percent, 100.0);
  int64 max_level = 0;
  CHECK(backlight.GetMaxBrightnessLevel(&max_level));
  return static_cast<int64>(roundl(percent * max_level / 100.0));
}

}  // namespace

// A simple tool to get and set the brightness level of the display backlight.
int main(int argc, char* argv[]) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  CHECK_EQ(argc, 1) << "Unexpected arguments. Try --help";
  CHECK((FLAGS_get_brightness + FLAGS_get_max_brightness +
         FLAGS_get_brightness_percent) < 2)
      << "-get_brightness and -get_brightness_percent and -get_max_brightness "
      << "are mutually exclusive";
  CHECK(FLAGS_set_brightness < 0 || FLAGS_set_brightness_percent < 0)
      << "-set_brightness and -set_brightness_percent are mutually exclusive";

  power_manager::system::InternalBacklight backlight;
  CHECK(backlight.Init(base::FilePath(power_manager::kInternalBacklightPath),
                       power_manager::kInternalBacklightPattern));
  if (FLAGS_get_brightness) {
    int64 level = 0;
    CHECK(backlight.GetCurrentBrightnessLevel(&level));
    printf("%" PRIi64 "\n", level);
  }
  if (FLAGS_get_max_brightness) {
    int64 max_level = 0;
    CHECK(backlight.GetMaxBrightnessLevel(&max_level));
    printf("%" PRIi64 "\n", max_level);
  }
  if (FLAGS_get_brightness_percent) {
    int64 level = 0;
    CHECK(backlight.GetCurrentBrightnessLevel(&level));
    int64 max_level = 0;
    CHECK(backlight.GetMaxBrightnessLevel(&max_level));
    printf("%" PRIi64 "\n", static_cast<int64>((level + 0.5) * 100 /
                                               max_level));
  }
  if (FLAGS_set_brightness >= 0) {
    CHECK(backlight.SetBrightnessLevel(FLAGS_set_brightness,
                                       base::TimeDelta()));
  }
  if (FLAGS_set_brightness_percent >= 0.0) {
    int64 new_level = PercentToLevel(backlight, FLAGS_set_brightness_percent);
    CHECK(backlight.SetBrightnessLevel(new_level, base::TimeDelta()));
  }
  if (FLAGS_set_resume_brightness >= 0 || FLAGS_set_resume_brightness == -1) {
    CHECK(backlight.SetResumeBrightnessLevel(FLAGS_set_resume_brightness));
  }
  if (FLAGS_set_resume_brightness_percent >= 0.0) {
    int64 new_level = PercentToLevel(backlight,
                                     FLAGS_set_resume_brightness_percent);
    CHECK(backlight.SetResumeBrightnessLevel(new_level));
  }
  return 0;
}
