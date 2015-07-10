// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <chromeos/message_loops/glib_message_loop.h>

#include <fcntl.h>
#include <unistd.h>

#include <memory>

#include <base/bind.h>
#include <base/location.h>
#include <base/posix/eintr_wrapper.h>
#include <gtest/gtest.h>

#include <chromeos/bind_lambda.h>
#include <chromeos/message_loops/message_loop.h>
#include <chromeos/message_loops/message_loop_utils.h>

using base::Bind;
using base::TimeDelta;

namespace {
// Helper class to create and close a unidirectional pipe. Used to provide valid
// file descriptors when testing watching for a file descriptor.
class ScopedPipe {
 public:
  ScopedPipe() {
    int fds[2];
    if (pipe(fds) != 0) {
      PLOG(FATAL) << "Creating a pipe()";
    }
    reader = fds[0];
    writer = fds[1];
  }
  ~ScopedPipe() {
    if (reader != -1)
      close(reader);
    if (writer != -1)
      close(writer);
  }

  // The reader and writer end of the pipe.
  int reader{-1};
  int writer{-1};
};
}  // namespace

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

TEST_F(GlibMessageLoopTest, WatchForInvalidFD) {
  bool called = false;
  EXPECT_EQ(MessageLoop::kTaskIdNull, loop_->WatchFileDescriptor(
      FROM_HERE, -1, MessageLoop::kWatchRead, true,
      Bind([&called] { called = true; })));
  EXPECT_EQ(MessageLoop::kTaskIdNull, loop_->WatchFileDescriptor(
      FROM_HERE, -1, MessageLoop::kWatchWrite, true,
      Bind([&called] { called = true; })));
  EXPECT_EQ(0, MessageLoopRunMaxIterations(loop_.get(), 100));
  EXPECT_FALSE(called);
}

TEST_F(GlibMessageLoopTest, CancelWatchedFileDescriptor) {
  ScopedPipe pipe;
  bool called = false;
  TaskId task_id = loop_->WatchFileDescriptor(
      FROM_HERE, pipe.reader, MessageLoop::kWatchRead, true,
      Bind([&called] { called = true; }));
  EXPECT_NE(MessageLoop::kTaskIdNull, task_id);
  // The reader end is blocked because we didn't write anything to the writer
  // end.
  EXPECT_EQ(0, MessageLoopRunMaxIterations(loop_.get(), 100));
  EXPECT_FALSE(called);
  EXPECT_TRUE(loop_->CancelTask(task_id));
}

// When you watch a file descriptor for reading, the guaranties are that a
// blocking call to read() on that file descriptor will not block. This should
// include the case when the other end of a pipe is closed or the file is empty.
TEST_F(GlibMessageLoopTest, WatchFileDescriptorTriggersWhenEmpty) {
  int fd = HANDLE_EINTR(open("/dev/null", O_RDONLY));
  int called = 0;
  TaskId task_id = loop_->WatchFileDescriptor(
      FROM_HERE, fd, MessageLoop::kWatchRead, true,
      Bind([&called] { called++; }));
  EXPECT_NE(MessageLoop::kTaskIdNull, task_id);
  EXPECT_NE(0, MessageLoopRunMaxIterations(loop_.get(), 10));
  EXPECT_LT(2, called);
  EXPECT_TRUE(loop_->CancelTask(task_id));
}

TEST_F(GlibMessageLoopTest, WatchFileDescriptorTriggersWhenPipeClosed) {
  ScopedPipe pipe;
  bool called = false;
  EXPECT_EQ(0, HANDLE_EINTR(close(pipe.writer)));
  pipe.writer = -1;
  TaskId task_id = loop_->WatchFileDescriptor(
      FROM_HERE, pipe.reader, MessageLoop::kWatchRead, true,
      Bind([&called] { called = true; }));
  EXPECT_NE(MessageLoop::kTaskIdNull, task_id);
  // The reader end is not blocked because we closed the writer end so a read on
  // the reader end would return 0 bytes read.
  EXPECT_NE(0, MessageLoopRunMaxIterations(loop_.get(), 10));
  EXPECT_TRUE(called);
  EXPECT_TRUE(loop_->CancelTask(task_id));
}

// Test that an invalid file descriptor triggers the callback.
TEST_F(GlibMessageLoopTest, WatchFileDescriptorTriggersWhenInvalid) {
  int fd = HANDLE_EINTR(open("/dev/zero", O_RDONLY));
  int called = 0;
  TaskId task_id = loop_->WatchFileDescriptor(
      FROM_HERE, fd, MessageLoop::kWatchRead, true,
      Bind([&called, fd] {
        if (!called)
          IGNORE_EINTR(close(fd));
        called++;
      }));
  EXPECT_NE(MessageLoop::kTaskIdNull, task_id);
  EXPECT_NE(0, MessageLoopRunMaxIterations(loop_.get(), 10));
  EXPECT_LT(2, called);
  EXPECT_TRUE(loop_->CancelTask(task_id));
}

// When a WatchFileDescriptor task is scheduled with |persistent| = true, we
// should keep getting a call whenever the file descriptor is ready.
TEST_F(GlibMessageLoopTest, WatchFileDescriptorPersistently) {
  int fd = HANDLE_EINTR(open("/dev/zero", O_RDONLY));
  int called = 0;
  TaskId task_id = loop_->WatchFileDescriptor(
      FROM_HERE, fd, MessageLoop::kWatchRead, true,
      Bind([&called] { called++; }));
  EXPECT_NE(MessageLoop::kTaskIdNull, task_id);
  // We let the main loop run for 20 iterations to give it enough iterations to
  // verify that our callback was called more than one. We only check that our
  // callback is called more than once.
  EXPECT_EQ(20, MessageLoopRunMaxIterations(loop_.get(), 20));
  EXPECT_LT(1, called);
  EXPECT_TRUE(loop_->CancelTask(task_id));
  EXPECT_EQ(0, IGNORE_EINTR(close(fd)));
}

TEST_F(GlibMessageLoopTest, WatchFileDescriptorNonPersistent) {
  int fd = HANDLE_EINTR(open("/dev/zero", O_RDONLY));
  int called = 0;
  TaskId task_id = loop_->WatchFileDescriptor(
      fd, MessageLoop::kWatchRead, false, Bind([&called] { called++; }));
  EXPECT_NE(MessageLoop::kTaskIdNull, task_id);
  // We let the main loop run for 20 iterations but we just expect it to run
  // at least once. The callback should be called exactly once since we
  // scheduled it non-persistently. After it ran, we shouldn't be able to cancel
  // this task.
  EXPECT_LT(0, MessageLoopRunMaxIterations(loop_.get(), 20));
  EXPECT_EQ(1, called);
  EXPECT_FALSE(loop_->CancelTask(task_id));
  EXPECT_EQ(0, IGNORE_EINTR(close(fd)));
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
