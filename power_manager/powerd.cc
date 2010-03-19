// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <inttypes.h>
#include <algorithm>
#include <cstdio>
#include "base/logging.h"
#include "base/string_util.h"
#include "power_manager/backlight.h"
#include "power_manager/xidle.h"

int main(int argc, char* argv[]) {
  int64 dim_ms, dim_brightness, level, max_level, dim_diff = 0;
  if (argc != 3 ||
      !StringToInt64(argv[1], &dim_ms) ||
      !StringToInt64(argv[2], &dim_brightness)) {
    printf("Usage: %s dim_ms dim_brightness\n");
    return 1;
  }
  power_manager::XIdle idle;
  Display* display = XOpenDisplay(NULL);
  CHECK(display) << "Cannot open display";
  bool ok = idle.Init(display);
  CHECK(ok) << "Cannot initialize XIdle";
  ok = idle.AddIdleTimeout(dim_ms);
  CHECK(ok) << "Cannot add idle timeout";
  power_manager::Backlight backlight;
  ok = backlight.Init();
  CHECK(ok) << "Cannot initialize backlight class";
  bool is_idle;
  int64 idle_time;
  while (idle.Monitor(&is_idle, &idle_time)) {
    if (is_idle) {
      if (!backlight.GetBrightness(&level, &max_level)) {
        LOG(WARNING) << "Can't get brightness";
      } else if (dim_brightness >= level) {
        LOG(INFO) << "Monitor is already dim. Nothing to do.";
      } else if (!backlight.SetBrightness(dim_brightness)) {
        LOG(WARNING) << "Can't set brightness to " << dim_brightness
                     << " out of " << max_level;
      } else {
        dim_diff = level - dim_brightness;
        LOG(INFO) << "Dim from " << level << " to " << dim_brightness
                  << " (out of " << max_level << ")";
      }
    } else if (dim_diff) {
      if (backlight.GetBrightness(&level, &max_level)) {
        int64 new_level = std::min(level + dim_diff, max_level);
        if (backlight.SetBrightness(new_level)) {
          LOG(INFO) << "Brighten from " << level << " to " << new_level
                    << " (out of " << max_level << ")";
        } else {
          LOG(WARNING) << "Can't brighten from " << level << " to "
                       << new_level << " (out of " << max_level << ")";
        }
        dim_diff = 0;
      } else {
        LOG(WARNING) << "Can't get brightness";
      }
    } else {
      LOG(INFO) << "Monitor is already bright. Nothing to do.";
    }
  }
  XCloseDisplay(display);
  return 0;
}
