// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>
#include <X11/extensions/XTest.h>

#include <cstdio>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "power_manager/mock_xsync.h"
#include "power_manager/xidle.h"

namespace power_manager {

class XIdleTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    xsync_ = new MockXSync();
    CHECK(xsync_);
    // Mock out XIdle's XSync by replacing it with MockXSync.
    idle_.reset(new XIdle(xsync_));
  }
  virtual void TearDown() {
    idle_->ClearTimeouts();
  }
  scoped_ptr<XIdle> idle_;
  MockXSync* xsync_;
};

class IdleObserverTest : public IdleObserver {
 public:
  explicit IdleObserverTest(MockXSync* xsync)
      : count_(0),
        xsync_(xsync) {}

  // Overridden from XIdleObsever:
  virtual void OnIdleEvent(bool is_idle, int64 idle_time_ms) {
    bool do_fake_motion_event = false;
    // The following setup results in alternating between going into idle and
    // coming out of idle.
    if ((count_ % 2) == 0) {
      // When the idle event count so far (excluding current event) is even,
      // check to make sure that the system is idle.
      ASSERT_TRUE(is_idle);
      ASSERT_GT(idle_time_ms, 49);
      ASSERT_LT(idle_time_ms, 500);
      // Get out of idle by invoking a fake motion event.  This should be done
      // AFTER count_ had been incremented, since it depends on the next value
      // of count_.
      do_fake_motion_event = true;
    } else {
      // When the idle event count so far (excluding current event) is odd,
      // check to make sure that the previous event had invoked the fake motion
      // event, taking the system out of idle.
      ASSERT_FALSE(is_idle);
      ASSERT_EQ(idle_time_ms, 0);
    }
    ++count_;
    if (do_fake_motion_event)
      xsync_->FakeMotionEvent();
  }

  int count_;

 private:
  MockXSync* xsync_;
};

// Test idle time when there is no user input.
TEST_F(XIdleTest, GetIdleTimeTest) {
  ASSERT_TRUE(idle_->Init(NULL));
  int64 idle_time = kint64max;
  ASSERT_TRUE(idle_->GetIdleTime(&idle_time));
  // Initial idle time should be zero.
  ASSERT_EQ(0, idle_time);
  for (int i = 0; i < 10; ++i) {
    xsync_->Run(10, 1);
    ASSERT_TRUE(idle_->GetIdleTime(&idle_time));
    // Idle time should increase when the time simulator runs.
    ASSERT_EQ((i + 1) * 10, idle_time);
  }
}

// Test idle time when there is user input.
TEST_F(XIdleTest, GetIdleTimeWhenNonIdleTest) {
  ASSERT_TRUE(idle_->Init(NULL));
  int64 idle_time = kint64max;
  // Let the time run for a bit.
  xsync_->Run(30, 1);
  ASSERT_TRUE(idle_->GetIdleTime(&idle_time));
  ASSERT_EQ(idle_time, 30);
  // Simulate user input event.
  xsync_->FakeMotionEvent();
  ASSERT_TRUE(idle_->GetIdleTime(&idle_time));
  ASSERT_EQ(idle_time, 0);
}

TEST_F(XIdleTest, MonitorTest) {
  xsync_->FakeMotionEvent();
  // Register observer with XIdle, so it can count XIdle events.
  IdleObserverTest observer(xsync_);
  ASSERT_TRUE(idle_->Init(&observer));
  // Set an idle timeout.
  idle_->AddIdleTimeout(50);
  // Advance the simulated time.
  xsync_->Run(100, 1);
  // There should have have only been one timeout trigger, at t=50.
  // The trigger would have resulted in two observer counts.
  ASSERT_EQ(2, observer.count_);
  // Run for another 100 time units.  There should be two more triggers, at
  // t=100 and t=150.  That increases the observer count by 4.
  xsync_->Run(100, 1);
  ASSERT_EQ(6, observer.count_);
}

}  // namespace power_manager
