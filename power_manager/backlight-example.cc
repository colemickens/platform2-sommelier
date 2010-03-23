// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <inttypes.h>
#include <cstdio>
#include "base/logging.h"
#include "base/string_util.h"
#include "power_manager/backlight.h"

int main(int argc, char* argv[]) {
  power_manager::Backlight backlight;
  int64 level, max;
  CHECK(backlight.Init());
  CHECK(backlight.GetBrightness(&level, &max));
  printf("Current brightness level is %" PRIi64 " out of %" PRIi64 "\n",
         level, max);
  if (argc > 1) {
    int64 new_level;
    CHECK(StringToInt64(argv[1], &new_level));
    CHECK(backlight.SetBrightness(new_level));
    CHECK(backlight.GetBrightness(&level, &max));
    printf("New brightness level is %" PRIi64 " out of %" PRIi64 "\n",
           level, max);
  }
  return 0;
}
