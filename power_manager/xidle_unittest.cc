// Copyright (c) 2009-2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>
#include <unistd.h>
#include <X11/extensions/XTest.h>
#include <cstdio>

#include "base/command_line.h"
#include "base/logging.h"
#include "power_manager/xidle.h"

namespace power_manager {

class XIdleTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    display_ = XOpenDisplay(NULL);
    ASSERT_TRUE(idle_.Init(display_));
  }
  virtual void TearDown() {
    idle_.ClearTimeouts();
    XCloseDisplay(display_);
  }
  Display *display_;
  power_manager::XIdle idle_;
};

static void FakeMotionEvent(Display* display) {
  XTestFakeRelativeMotionEvent(display, 0, 0, 0);
  XSync(display, false);
}

TEST_F(XIdleTest, GetIdleTimeTest) {
  int64 idle_time = kint64max;
  ASSERT_TRUE(idle_.GetIdleTime(&idle_time));
  ASSERT_NE(idle_time, kint64max);
}

TEST_F(XIdleTest, GetIdleTimeWhenNonIdleTest) {
  int64 idle_time = kint64max;
  FakeMotionEvent(display_);
  ASSERT_TRUE(idle_.GetIdleTime(&idle_time));
  ASSERT_LT(idle_time, 50);
}

TEST_F(XIdleTest, MonitorTest) {
  int64 idle_time = kint64max;
  bool is_idle;
  FakeMotionEvent(display_);
  idle_.AddIdleTimeout(50);
  idle_.AddIdleTimeout(100);
  for (int i = 0; i < 10; i++) {
    ASSERT_TRUE(idle_.GetIdleTime(&idle_time));
    ASSERT_LT(idle_time, 50);
    ASSERT_TRUE(idle_.Monitor(&is_idle, &idle_time));
    ASSERT_GT(idle_time, 49);
    ASSERT_LT(idle_time, 100);
    ASSERT_TRUE(is_idle);
    FakeMotionEvent(display_);
    ASSERT_TRUE(idle_.Monitor(&is_idle, &idle_time));
    ASSERT_LT(idle_time, 50);
    ASSERT_FALSE(is_idle);
  }
}

}  // namespace power_manager
