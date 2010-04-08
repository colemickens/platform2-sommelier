// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gflags/gflags.h>
#include <inttypes.h>

#include <cstdio>

#include "base/logging.h"
#include "base/string_util.h"
#include "power_manager/backlight.h"

// backlight-tool: A simple tool to get and set the brightness level of the
//                 display backlight.

DEFINE_bool(get_brightness, false, "Get current brightness level.");
DEFINE_bool(get_max_brightness, false, "Get max brightness level.");
DEFINE_int64(set_brightness, -1, "Set brightness level.");

int main(int argc, char* argv[]) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  CHECK(argc == 1) << "Unexpected arguments. Try --help";
  CHECK(!FLAGS_get_brightness || !FLAGS_get_max_brightness) <<
      "-get_brightness and -get_max_brightness are mutually exclusive";
  power_manager::Backlight backlight;
  CHECK(backlight.Init());
  if (FLAGS_get_brightness || FLAGS_get_max_brightness) {
    int64 level, max;
    CHECK(backlight.GetBrightness(&level, &max));
    if (FLAGS_get_brightness)
      printf("%" PRIi64 "\n", level);
    else if (FLAGS_get_max_brightness)
      printf("%" PRIi64 "\n", max);
  }
  if (FLAGS_set_brightness >= 0) {
    CHECK(backlight.SetBrightness(FLAGS_set_brightness));
  }
  return 0;
}
