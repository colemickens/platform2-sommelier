// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A simple daemon to detect, mount, and eject removable storage devices.

#include <base/file_util.h>
#include <base/logging.h>
#include <base/string_util.h>
#include <dbus-c++/glib-integration.h>
#include <dbus-c++/util.h>
#include <gflags/gflags.h>
#include <glib-object.h>
#include <glib.h>
#include <metrics/metrics_library.h>

int main(int argc, char** argv) {
  ::g_type_init();
  g_thread_init(NULL);
  google::ParseCommandLineFlags(&argc, &argv, true);

  LOG(INFO) << "Creating a GMainLoop";
  GMainLoop* loop = g_main_loop_new(g_main_context_default(), FALSE);

  LOG(INFO) << "Initializing the metrics library";
  MetricsLibrary metrics_lib;
  metrics_lib.Init();

  // Run the main loop until exit time:
  // TODO(rtc): daemonize this
  g_main_loop_run(loop);

  return 0;
}
