// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gflags/gflags.h>
#include <iostream>

#include "glinterface.h"
#include "xlib_window.h"

#include "synccontroltest.h"

namespace {

const double kMinVsyncHz = 15.0;
const double kMaxVsyncHz = 150.0;

}  // namespace

DEFINE_double(vsync, 60.0,
             "Rate in Hz that vetrical refreshes occur on the screen. The "
              "default value is 60.0 Hz.");

static bool ValidateVsync(const char* flagname, double rate) {
  if (rate <= 0.0) {
    std::cout << "Attempted to use vsync rate of 0 or negative HZ, vsync="
              << rate << ". That is not a sane value!\n";
    return false;
  }

  if (rate <= kMinVsyncHz) {
    std::cout << "Attempted to use vsync rate of less then " << kMinVsyncHz
              << ", vsync=" << rate << "which is suspect. Please check if your "
              << "screen actually runs this low and update this test if it "
              << "does.\n";
    return false;
  }

  if (rate > kMaxVsyncHz) {
    std::cout << "Attempted to use vsync rate of greater then " << kMaxVsyncHz
              << ", vsync=" << rate << "which is suspect. Please check if your "
              << "screen actually runs this high and update this test if it "
              << "does.\n";
    return false;
  }

  return true;
}

namespace {

static const bool vsync_dummy = google::RegisterFlagValidator(&FLAGS_vsync,
                                                              &ValidateVsync);
}  // namespace


int main(int argc, char* argv[]) {
  g_main_gl_interface.reset(GLInterface::Create());
  google::ParseCommandLineFlags(&argc, &argv, true);

  int vsync_interval = 1000000 * (1.0 / FLAGS_vsync);

  if (!g_main_gl_interface->Init()) {
    std::cout << "Failed to initialize GL interface.\n";
    return 1;
  }

  SyncControlTest* test_controller = SyncControlTest::Create();

  // Resizing the window to be fullscreen, since swapbuffers of non-fullscreen
  // buffers is handled via the CPU and thus will not update the counters that
  // we are interested in testing.
  XWindowAttributes saved_attr;
  XGetWindowAttributes(g_xlib_display, g_xlib_window, &saved_attr);
  XWindowAttributes root_attr;
  XGetWindowAttributes(g_xlib_display,
                       DefaultRootWindow(g_xlib_display),
                       &root_attr);
  XMoveResizeWindow(g_xlib_display, g_xlib_window,
                    0, 0, root_attr.width, root_attr.height);

  test_controller->Init();
  bool ret_val = true;
  for (unsigned int i = 0; i < FLAGS_vsync; i++)
    ret_val &= test_controller->Loop(vsync_interval);
  delete test_controller;

  // Resetting the window to how it was before the test.
  XMoveResizeWindow(g_xlib_display, g_xlib_window,
                    saved_attr.x, saved_attr.y,
                    saved_attr.width, saved_attr.height);
  g_main_gl_interface->Cleanup();

  return ret_val ? 0 : 1;
};
