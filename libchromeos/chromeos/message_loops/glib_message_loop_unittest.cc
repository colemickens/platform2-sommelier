// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <chromeos/message_loops/glib_message_loop.h>

#include <memory>

#include <base/bind.h>
#include <base/location.h>
#include <gtest/gtest.h>

#include <chromeos/bind_lambda.h>
#include <chromeos/message_loops/message_loop.h>
#include <chromeos/message_loops/message_loop_utils.h>

using base::Bind;
using base::TimeDelta;

namespace chromeos {

using TaskId = MessageLoop::TaskId;

class GlibMessageLoopTest : public ::testing::Test {
 protected:
  void SetUp() override {
    loop_.reset(new GlibMessageLoop());
    EXPECT_TRUE(loop_.get());
  }

  std::unique_ptr<GlibMessageLoop> loop_;
};

TEST_F(GlibMessageLoopTest, CancelTaskInvalidValuesTest) {
  EXPECT_FALSE(loop_->CancelTask(MessageLoop::kTaskIdNull));
  EXPECT_FALSE(loop_->CancelTask(1234));
}

TEST_F(GlibMessageLoopTest, PostTaskTest) {
  bool called = false;
  TaskId task_id = loop_->PostTask(FROM_HERE,
                                   Bind([&called]() { called = true; }));
  EXPECT_NE(MessageLoop::kTaskIdNull, task_id);
  MessageLoopRunMaxIterations(loop_.get(), 100);
  EXPECT_TRUE(called);
}

// Tests that we can cancel tasks right after we schedule them.
TEST_F(GlibMessageLoopTest, PostTaskCancelledTest) {
  bool called = false;
  TaskId task_id = loop_->PostTask(FROM_HERE,
                                   Bind([&called]() { called = true; }));
  EXPECT_TRUE(loop_->CancelTask(task_id));
  MessageLoopRunMaxIterations(loop_.get(), 100);
  EXPECT_FALSE(called);
  // Can't remove a task you already removed.
  EXPECT_FALSE(loop_->CancelTask(task_id));
}

TEST_F(GlibMessageLoopTest, PostDelayedTaskRunsEventuallyTest) {
  bool called = false;
  TaskId task_id = loop_->PostDelayedTask(FROM_HERE,
                                          Bind([&called]() { called = true; }),
                                          TimeDelta::FromMilliseconds(100));
  EXPECT_NE(MessageLoop::kTaskIdNull, task_id);
  MessageLoopRunUntil(loop_.get(),
                      TimeDelta::FromSeconds(10),
                      Bind([&called]() { return called; }));
  // Check that the main loop finished before the 10 seconds timeout, so it
  // finished due to the callback being called and not due to the timeout.
  EXPECT_TRUE(called);
}

// Test that you can call the overloaded version of PostDelayedTask from
// MessageLoop. This is important because only one of the two methods is
// virtual, so you need to unhide the other when overriding the virtual one.
TEST_F(GlibMessageLoopTest, PostDelayedTaskWithoutLocation) {
  loop_->PostDelayedTask(Bind(&base::DoNothing), TimeDelta());
  EXPECT_EQ(1, MessageLoopRunMaxIterations(loop_.get(), 100));
}

// Test that we can cancel the task we are running, and should just fail.
TEST_F(GlibMessageLoopTest, DeleteTaskFromSelf) {
  bool cancel_result = true;  // We would expect this to be false.
  GlibMessageLoop* loop_ptr = loop_.get();
  TaskId task_id;
  task_id = loop_->PostTask(
      FROM_HERE,
      Bind([&cancel_result, loop_ptr, &task_id]() {
        cancel_result = loop_ptr->CancelTask(task_id);
      }));
  EXPECT_EQ(1, MessageLoopRunMaxIterations(loop_.get(), 100));
  EXPECT_FALSE(cancel_result);
}

}  // namespace chromeos
