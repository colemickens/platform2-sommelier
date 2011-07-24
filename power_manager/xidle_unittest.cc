// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <X11/extensions/XTest.h>

#include <cstdio>

#include "base/command_line.h"
#include "base/logging.h"
#include "power_manager/xidle.h"
#include "power_manager/xidle_observer.h"

namespace power_manager {

class XIdleTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    ASSERT_TRUE(gdk_init_check(NULL, NULL));
  }
  virtual void TearDown() {
    idle_.ClearTimeouts();
  }
  XIdle idle_;
};

static void FakeMotionEvent(Display* display) {
  XTestFakeRelativeMotionEvent(display, 0, 0, 0);
  XSync(display, false);
}

class XIdleObserverTest : public XIdleObserver {
 public:
  explicit XIdleObserverTest(GMainLoop* loop) : count_(0), loop_(loop) {}

  // Overridden from XIdleObsever:
  virtual void OnIdleEvent(bool is_idle, int64 idle_time_ms) {
    if (count_ & 1) {
      ASSERT_FALSE(is_idle);
      ASSERT_LT(idle_time_ms, 50);
    } else {
      ASSERT_TRUE(is_idle);
      ASSERT_GT(idle_time_ms, 49);
      ASSERT_LT(idle_time_ms, 500);
      FakeMotionEvent(GDK_DISPLAY());
    }
    count_++;
  }

  static gboolean Quit(gpointer data) {
    XIdleObserverTest* observer = static_cast<XIdleObserverTest*>(data);
    g_main_loop_quit(observer->loop_);
    return false;
  }

  int count_;

 private:
  GMainLoop* loop_;
};

TEST_F(XIdleTest, GetIdleTimeTest) {
  ASSERT_TRUE(idle_.Init(NULL));
  int64 idle_time = kint64max;
  ASSERT_TRUE(idle_.GetIdleTime(&idle_time));
  ASSERT_NE(idle_time, kint64max);
}

TEST_F(XIdleTest, GetIdleTimeWhenNonIdleTest) {
  ASSERT_TRUE(idle_.Init(NULL));
  int64 idle_time = kint64max;
  FakeMotionEvent(GDK_DISPLAY());
  ASSERT_TRUE(idle_.GetIdleTime(&idle_time));
  ASSERT_LT(idle_time, 50);
}

TEST_F(XIdleTest, MonitorTest) {
  FakeMotionEvent(GDK_DISPLAY());
  GMainLoop* loop = g_main_loop_new(NULL, false);
  XIdleObserverTest observer(loop);
  ASSERT_TRUE(idle_.Init(&observer));
  idle_.AddIdleTimeout(50);
  XIdle idle;
  g_timeout_add(1000, XIdleObserverTest::Quit, &observer);
  g_main_loop_run(loop);
  ASSERT_GT(observer.count_, 1);
  ASSERT_LT(observer.count_, 41);
}

}  // namespace power_manager
