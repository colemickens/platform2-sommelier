// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <glib.h>
#include <glib-object.h>
#include <inttypes.h>

#include <cstdio>

#include "base/logging.h"
#include "power_manager/xidle.h"
#include "power_manager/xidle_observer.h"

// Prints console notifications when the user is and is not idle.
class XIdleObserverExample : public power_manager::XIdleObserver {
 public:
  // Overridden from power_manager::XIdleObserver:
  virtual void OnIdleEvent(bool is_idle, int64 idle_time_ms) {
    if (is_idle)
      printf("User has been idle for %" PRIi64 " ms\n", idle_time_ms);
    else
      printf("User is active\n");
  }
};

int main(int argc, char** argv) {
  g_type_init();
  GMainLoop* loop = g_main_loop_new(NULL, false);
  power_manager::XIdle idle;
  XIdleObserverExample observer;
  CHECK(idle.Init(&observer));
  CHECK(idle.AddIdleTimeout(2000));
  CHECK(idle.AddIdleTimeout(5000));
  g_main_loop_run(loop);
  return 0;
}
