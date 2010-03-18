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
  bool ok = backlight.Init() && backlight.GetBrightness(&level, &max);
  CHECK(ok) << "Can't get brightness";
  printf("Current brightness level is %" PRIi64 " out of %" PRIi64 "\n",
         level, max);
  if (argc > 1) {
    int64 new_level;
    ok = StringToInt64(argv[1], &new_level) &&
         backlight.SetBrightness(new_level) &&
         backlight.GetBrightness(&level, &max);
    CHECK(ok) << "Can't set brightness to " << argv[1] << " out of " << max;
    printf("New brightness level is %" PRIi64 " out of %" PRIi64 "\n",
           level, max);
  }
  return 0;
}
