// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include <base/bind.h>
#include <base/threading/platform_thread.h>
#include <base/threading/thread.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "libhwsec/message_loop_idle.h"

namespace hwsec {

namespace {

// ---------------------------- MessageLoop tests ----------------------------

class MessageLoopTest : public testing::Test {
 public:
  MessageLoopTest() : test_thread_("test_thread"), test_done_(false) {}

  void SetUp() override { test_thread_.StartAndWaitForTesting(); }

  void TearDown() override {
    test_done_ = true;

    // We need to stop the thread first before removing |event_|.
    test_thread_.Stop();
  }

  void StartEvent() {
    event_.reset(new MessageLoopIdleEvent(test_thread_.message_loop()));
  }

 protected:
  // base::Thread used to test MessageLoopIdleEvent.
  base::Thread test_thread_;

  // The MessageLoopIdleEvent that we are testing.
  std::unique_ptr<MessageLoopIdleEvent> event_;

  // Set to true to signal that the testing is down (in tear down stage.).
  bool test_done_;

  void InfiniteSchedule() {
    if (test_done_) {
      return;
    }

    base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(20));
    test_thread_.task_runner()->PostTask(
        FROM_HERE,
        base::Bind(&MessageLoopTest::InfiniteSchedule, base::Unretained(this)));
  }

  void ScheduleNTimes(int n) {
    if (test_done_) {
      return;
    }

    if (n <= 0) {
      return;
    }

    base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(20));
    test_thread_.task_runner()->PostTask(
        FROM_HERE, base::Bind(&MessageLoopTest::ScheduleNTimes,
                              base::Unretained(this), n - 1));
  }

  // The default timeout that we'll use when waiting for an event that we expect
  // will be signaled.
  static constexpr base::TimeDelta kTimeout =
      base::TimeDelta::FromSecondsD(5.0);

  // The default timeout that we'll use when waiting for an event that we don't
  // expect will be signaled. i.e. This time span will always elapse during
  // testing.
  static constexpr base::TimeDelta kVerifyTimespan =
      base::TimeDelta::FromSecondsD(0.5);
};

constexpr base::TimeDelta MessageLoopTest::kTimeout;
constexpr base::TimeDelta MessageLoopTest::kVerifyTimespan;

TEST_F(MessageLoopTest, NoTask) {
  // If there are no task, it should not wait indefinitely.
  StartEvent();
  EXPECT_TRUE(event_->TimedWait(kTimeout));
}

TEST_F(MessageLoopTest, Wait) {
  // If there are no task, it should not wait indefinitely.
  StartEvent();
  event_->Wait();
  // Note that Wait() should return pretty soon. When this test fails, Wait()
  // will block indefinitely.
}

TEST_F(MessageLoopTest, RecurringTask) {
  // If a task keeps on scheduling a version of itself, then the event should
  // always timeout.
  InfiniteSchedule();
  StartEvent();
  EXPECT_FALSE(event_->TimedWait(kVerifyTimespan));
}

TEST_F(MessageLoopTest, Task1Time) {
  // The event will be done eventually, taking ~20ms, so TimedWait() should
  // return true once that happens.
  ScheduleNTimes(1);
  StartEvent();
  EXPECT_TRUE(event_->TimedWait(kTimeout));
}

TEST_F(MessageLoopTest, Task5Times) {
  // The events will be done eventually, taking ~100ms, so TimedWait() should
  // return true once that happens.
  ScheduleNTimes(5);
  StartEvent();
  EXPECT_TRUE(event_->TimedWait(kTimeout));
}

TEST_F(MessageLoopTest, DelayedTask) {
  // If there's a delayed task, it should still be idle.
  test_thread_.task_runner()->PostDelayedTask(
      FROM_HERE, base::Bind([]() { NOTREACHED(); }),
      base::TimeDelta::FromDays(42));
  // One does not simply run a unit test for 42 days.

  StartEvent();
  EXPECT_TRUE(event_->TimedWait(kTimeout));
}

}  // namespace

}  // namespace hwsec
