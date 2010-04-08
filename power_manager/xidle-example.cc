// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gdk/gdk.h>
#include <inttypes.h>

#include <cstdio>

#include "base/logging.h"
#include "power_manager/xidle.h"

// xidle-example: Prints console notifications when the user is and is not
//                idle.

class IdleMonitorExample : public power_manager::XIdleMonitor {
 public:
  void OnIdleEvent(bool is_idle, int64 idle_time_ms) {
    if (is_idle)
      printf("User has been idle for %" PRIi64 " ms\n", idle_time_ms);
    else
      printf("User is active\n");
  }
};

int main(int argc, char* argv[]) {
  gdk_init(&argc, &argv);
  GMainLoop* loop = g_main_loop_new(NULL, false);
  power_manager::XIdle idle;
  IdleMonitorExample monitor;
  CHECK(idle.Init(&monitor));
  CHECK(idle.AddIdleTimeout(2000));
  CHECK(idle.AddIdleTimeout(5000));
  g_main_loop_run(loop);
  return 0;
}
