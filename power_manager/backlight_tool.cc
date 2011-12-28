// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gflags/gflags.h>
#include <inttypes.h>

#include <cstdio>

#include "base/logging.h"
#include "power_manager/power_constants.h"

#ifdef IS_DESKTOP
#include "power_manager/external_backlight.h"
#else
#include "power_manager/backlight.h"
#endif

DEFINE_bool(get_brightness, false, "Get current brightness level.");
DEFINE_bool(get_max_brightness, false, "Get max brightness level.");
DEFINE_int64(set_brightness, -1, "Set brightness level.");

// A simple tool to get and set the brightness level of the display backlight.
int main(int argc, char* argv[]) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  CHECK(argc == 1) << "Unexpected arguments. Try --help";
  CHECK(!FLAGS_get_brightness || !FLAGS_get_max_brightness) <<
      "-get_brightness and -get_max_brightness are mutually exclusive";
#ifdef IS_DESKTOP
  power_manager::ExternalBacklight backlight;
  CHECK(backlight.Init());
#else
  power_manager::Backlight backlight;
  CHECK(backlight.Init(FilePath(power_manager::kBacklightPath),
                       power_manager::kBacklightPattern));
#endif
  if (FLAGS_get_brightness) {
    int64 level = 0;
    CHECK(backlight.GetCurrentBrightnessLevel(&level));
    printf("%" PRIi64 "\n", level);
  }
  if (FLAGS_get_max_brightness) {
    int64 max = 0;
    CHECK(backlight.GetMaxBrightnessLevel(&max));
    printf("%" PRIi64 "\n", max);
  }
  if (FLAGS_set_brightness >= 0)
    CHECK(backlight.SetBrightnessLevel(FLAGS_set_brightness));
  return 0;
}
