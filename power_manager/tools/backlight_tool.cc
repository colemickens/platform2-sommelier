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

#ifdef IS_DESKTOP
#include "power_manager/powerm/external_backlight.h"
#else
#include "power_manager/powerm/internal_backlight.h"
#endif

DEFINE_bool(get_brightness, false, "Get current brightness level.");
DEFINE_bool(get_max_brightness, false, "Get max brightness level.");
DEFINE_int64(set_brightness, -1, "Set brightness level.");
DEFINE_double(set_brightness_percent, -1.0,
              "Set brightness as a percent in [0.0, 100.0].");

// A simple tool to get and set the brightness level of the display backlight.
int main(int argc, char* argv[]) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  CHECK_EQ(argc, 1) << "Unexpected arguments. Try --help";
  CHECK(!FLAGS_get_brightness || !FLAGS_get_max_brightness)
      << "-get_brightness and -get_max_brightness are mutually exclusive";
  CHECK(FLAGS_set_brightness < 0 || FLAGS_set_brightness_percent < 0)
      << "-set_brightness and -set_brightness_percent are mutually exclusive";

#ifdef IS_DESKTOP
  power_manager::ExternalBacklight backlight;
  CHECK(backlight.Init());
#else
  power_manager::InternalBacklight backlight;
  CHECK(backlight.Init(FilePath(power_manager::kInternalBacklightPath),
                       power_manager::kInternalBacklightPattern));
#endif
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
  if (FLAGS_set_brightness >= 0) {
    CHECK(backlight.SetBrightnessLevel(FLAGS_set_brightness,
                                       base::TimeDelta()));
  }
  if (FLAGS_set_brightness_percent >= 0.0) {
    FLAGS_set_brightness_percent =
        std::min(FLAGS_set_brightness_percent, 100.0);
    int64 max_level = 0;
    CHECK(backlight.GetMaxBrightnessLevel(&max_level));
    int64 new_level = static_cast<int64>(
        roundl(FLAGS_set_brightness_percent * max_level / 100.0));
    CHECK(backlight.SetBrightnessLevel(new_level, base::TimeDelta()));
  }
  return 0;
}
