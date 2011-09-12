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
#include "power_manager/mock_xsync.h"
#include "power_manager/xidle.h"
#include "power_manager/xidle_observer.h"

namespace power_manager {

class XIdleTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    // Mock out XIdle's XSync by replacing it with MockXSync.  Save the old
    // XSync pointer, because it needs to be freed later.
    xsync_ = new MockXSync();
    CHECK(xsync_);
    idle_.xsync_.reset(xsync_);
  }
  virtual void TearDown() {
    idle_.ClearTimeouts();
  }
  XIdle idle_;
  MockXSync* xsync_;
};

// Simulate the effect of user motion.
// This used to be a simple function returning void, but by making it a glib
// timeout callback, it can be scheduled to be called in the future as well.
static gboolean FakeMotionEvent(gpointer xsync) {
  reinterpret_cast<MockXSync*>(xsync)->FakeRelativeMotionEvent(0, 0, 0);
  return false;
}

class XIdleObserverTest : public XIdleObserver {
 public:
  explicit XIdleObserverTest(GMainLoop* loop, MockXSync* xsync)
      : count_(0),
        loop_(loop),
        xsync_(xsync) {}

  // Overridden from XIdleObsever:
  virtual void OnIdleEvent(bool is_idle, int64 idle_time_ms) {
    // The following setup results in alternating between going into idle and
    // coming out of idle.
    if ((count_ % 2) == 0) {
      // When the idle event count so far (excluding current event) is even,
      // check to make sure that the system is idle.
      ASSERT_TRUE(is_idle);
      ASSERT_GT(idle_time_ms, 49);
      ASSERT_LT(idle_time_ms, 500);
      // Get out of idle by invoking a fake motion event.  This should be
      // scheduled instead of called directly, because count__ needs to be
      // incremented before it is called.
      g_timeout_add(0, FakeMotionEvent, xsync_);
    } else {
      // When the idle event count so far (excluding current event) is odd,
      // check to make sure that the previous event had invoked the fake motion
      // event, taking the system out of idle.
      ASSERT_FALSE(is_idle);
      ASSERT_LT(idle_time_ms, 50);
    }
    ++count_;
  }

  static gboolean Quit(gpointer data) {
    XIdleObserverTest* observer = static_cast<XIdleObserverTest*>(data);
    g_main_loop_quit(observer->loop_);
    return false;
  }

  int count_;

 private:
  GMainLoop* loop_;
  MockXSync* xsync_;
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
  FakeMotionEvent(xsync_);
  ASSERT_TRUE(idle_.GetIdleTime(&idle_time));
  ASSERT_LT(idle_time, 50);
}

TEST_F(XIdleTest, MonitorTest) {
  FakeMotionEvent(xsync_);
  GMainLoop* loop = g_main_loop_new(NULL, false);
  XIdleObserverTest observer(loop, xsync_);
  ASSERT_TRUE(idle_.Init(&observer));
  g_timeout_add(1000, XIdleObserverTest::Quit, &observer);
  idle_.AddIdleTimeout(50);
  g_main_loop_run(loop);
  ASSERT_GT(observer.count_, 1);
  ASSERT_LT(observer.count_, 41);
}

}  // namespace power_manager
