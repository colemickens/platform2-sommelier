// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <inttypes.h>
#include <cstdio>
#include "power_manager/xidle.h"

int main(int argc, char *argv[]) {
  power_manager::XIdle idle;
  idle.Init();
  idle.AddIdleTimeout(2000);
  idle.AddIdleTimeout(5000);
  bool is_idle;
  int64 idle_time;
  while (idle.Monitor(&is_idle, &idle_time)) {
    if (is_idle)
      printf("User has been idle for %" PRIi64 " milliseconds\n", idle_time);
    else
      printf("User is active\n");
  }
  return 0;
}
