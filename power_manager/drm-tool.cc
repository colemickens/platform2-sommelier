// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gdk/gdk.h>
#include <gflags/gflags.h>
#include <glib.h>
#include <inttypes.h>

#include <cstdio>

#include "base/logging.h"
#include "power_manager/udev_controller.h"

// drm-tool: A simple tool to monitor drm hotplug events with udev.

class DrmCallback : public power_manager::UdevDelegate {
 public:
  void Run(GIOChannel* /* source */, GIOCondition /* condition */) {
    printf("Udev drm callback\n");
  }
};

int main(int argc, char* argv[]) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  DrmCallback callback;
  power_manager::UdevController drm_controller(&callback, "drm");
  if (!drm_controller.Init())
    LOG(WARNING) << "Cannot initialize drm controller";
  GMainLoop* loop = g_main_loop_new(NULL, false);
  g_main_loop_run(loop);
  return 0;
}
