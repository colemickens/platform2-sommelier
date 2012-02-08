// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Unit tests for MountTask.

#include "mount_task.h"

#include <base/at_exit.h>
#include <base/logging.h>
#include <base/synchronization/waitable_event.h>
#include <base/threading/platform_thread.h>
#include <base/threading/thread.h>
#include <base/time.h>
#include <chromeos/utility.h>
#include <gtest/gtest.h>

#include "mock_mount.h"

using base::PlatformThread;

namespace cryptohome {
using std::string;
using ::testing::Return;
using ::testing::_;

class MountTaskTest : public ::testing::Test {
 public:
  MountTaskTest()
      : runner_("RunnerThread"),
        event_(true, false),
        mount_(),
        result_() {
    wait_time_ = base::TimeDelta::FromSeconds(180);
  }

  virtual ~MountTaskTest() { }

  void SetUp() {
    runner_.Start();
  }

  void TearDown() {
    if (runner_.IsRunning()) {
      runner_.Stop();
    }
  }

 protected:
  base::Thread runner_;
  base::WaitableEvent event_;
  MockMount mount_;
  MountTaskResult result_;
  base::TimeDelta wait_time_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MountTaskTest);
};

class MountTaskNotifier : public MountTaskObserver {
 public:
  MountTaskNotifier()
      : notified_(false) { }
  virtual ~MountTaskNotifier() { }

  // MountTaskObserver
  virtual void MountTaskObserve(const MountTaskResult& result) {
    notified_ = true;
  }

  bool notified_;
};

TEST_F(MountTaskTest, ResultCopyConstructorTest) {
  MountTaskResult result1;

  result1.set_sequence_id(1337);
  result1.set_return_status(true);
  result1.set_return_code(MOUNT_ERROR_FATAL);

  MountTaskResult result2(result1);

  ASSERT_EQ(result1.sequence_id(), result2.sequence_id());
  ASSERT_EQ(result1.return_status(), result2.return_status());
  ASSERT_EQ(result1.return_code(), result2.return_code());
}

TEST_F(MountTaskTest, ResultEqualsTest) {
  MountTaskResult result1;

  result1.set_sequence_id(1337);
  result1.set_return_status(true);
  result1.set_return_code(MOUNT_ERROR_FATAL);

  MountTaskResult result2;
  result2 = result1;

  ASSERT_EQ(result1.sequence_id(), result2.sequence_id());
  ASSERT_EQ(result1.return_status(), result2.return_status());
  ASSERT_EQ(result1.return_code(), result2.return_code());
}

TEST_F(MountTaskTest, EventTest) {
  ASSERT_FALSE(event_.IsSignaled());
  MountTask* mount_task = new MountTask(NULL, NULL, UsernamePasskey());
  mount_task->set_complete_event(&event_);
  mount_task->set_result(&result_);
  runner_.message_loop()->PostTask(FROM_HERE, mount_task);
  event_.TimedWait(wait_time_);
  ASSERT_TRUE(event_.IsSignaled());
}

TEST_F(MountTaskTest, ObserveTest) {
  MountTaskNotifier notifier;
  MountTask* mount_task = new MountTask(&notifier, NULL, UsernamePasskey());
  mount_task->set_result(&result_);
  runner_.message_loop()->PostTask(FROM_HERE, mount_task);
  for (unsigned int i = 0; i < 64; i++) {
    if (!notifier.notified_) {
      PlatformThread::Sleep(100);
    } else {
      break;
    }
  }
  ASSERT_TRUE(notifier.notified_);
}

TEST_F(MountTaskTest, NopTest) {
  ASSERT_FALSE(event_.IsSignaled());
  MountTask* mount_task = new MountTaskNop(NULL);
  mount_task->set_complete_event(&event_);
  mount_task->set_result(&result_);
  runner_.message_loop()->PostTask(FROM_HERE, mount_task);
  event_.TimedWait(wait_time_);
  ASSERT_TRUE(event_.IsSignaled());
}

TEST_F(MountTaskTest, MountTest) {
  EXPECT_CALL(mount_, MountCryptohome(_, _, _))
      .WillOnce(Return(true));

  ASSERT_FALSE(event_.IsSignaled());
  Mount::MountArgs mount_args;
  MountTask* mount_task = new MountTaskMount(NULL, &mount_, UsernamePasskey(),
                                             mount_args);
  mount_task->set_complete_event(&event_);
  mount_task->set_result(&result_);
  runner_.message_loop()->PostTask(FROM_HERE, mount_task);
  event_.TimedWait(wait_time_);
  ASSERT_TRUE(event_.IsSignaled());
}

TEST_F(MountTaskTest, MountGuestTest) {
  EXPECT_CALL(mount_, MountGuestCryptohome())
      .WillOnce(Return(true));

  ASSERT_FALSE(event_.IsSignaled());
  MountTask* mount_task = new MountTaskMountGuest(NULL, &mount_);
  mount_task->set_complete_event(&event_);
  mount_task->set_result(&result_);
  runner_.message_loop()->PostTask(FROM_HERE, mount_task);
  event_.TimedWait(wait_time_);
  ASSERT_TRUE(event_.IsSignaled());
}

TEST_F(MountTaskTest, MigratePasskeyTest) {
  EXPECT_CALL(mount_, MigratePasskey(_, _))
      .WillOnce(Return(true));

  ASSERT_FALSE(event_.IsSignaled());
  MountTask* mount_task = new MountTaskMigratePasskey(NULL, &mount_,
                                                      UsernamePasskey(), "");
  mount_task->set_complete_event(&event_);
  mount_task->set_result(&result_);
  runner_.message_loop()->PostTask(FROM_HERE, mount_task);
  event_.TimedWait(wait_time_);
  ASSERT_TRUE(event_.IsSignaled());
}

TEST_F(MountTaskTest, UnmountTest) {
  EXPECT_CALL(mount_, UnmountCryptohome())
      .WillOnce(Return(true));

  ASSERT_FALSE(event_.IsSignaled());
  MountTask* mount_task = new MountTaskUnmount(NULL, &mount_);
  mount_task->set_complete_event(&event_);
  mount_task->set_result(&result_);
  runner_.message_loop()->PostTask(FROM_HERE, mount_task);
  event_.TimedWait(wait_time_);
  ASSERT_TRUE(event_.IsSignaled());
}

TEST_F(MountTaskTest, TestCredentailsTest) {
  EXPECT_CALL(mount_, TestCredentials(_))
      .WillOnce(Return(true));

  ASSERT_FALSE(event_.IsSignaled());
  MountTask* mount_task = new MountTaskTestCredentials(NULL, &mount_,
                                                       UsernamePasskey());
  mount_task->set_complete_event(&event_);
  mount_task->set_result(&result_);
  runner_.message_loop()->PostTask(FROM_HERE, mount_task);
  event_.TimedWait(wait_time_);
  ASSERT_TRUE(event_.IsSignaled());
}

TEST_F(MountTaskTest, RemoveTest) {
  EXPECT_CALL(mount_, RemoveCryptohome(_))
      .WillOnce(Return(true));

  ASSERT_FALSE(event_.IsSignaled());
  MountTask* mount_task = new MountTaskRemove(NULL, &mount_, UsernamePasskey());
  mount_task->set_complete_event(&event_);
  mount_task->set_result(&result_);
  runner_.message_loop()->PostTask(FROM_HERE, mount_task);
  event_.TimedWait(wait_time_);
  ASSERT_TRUE(event_.IsSignaled());
}

TEST_F(MountTaskTest, ResetTpmContext) {
  ASSERT_FALSE(event_.IsSignaled());
  MountTask* mount_task = new MountTaskResetTpmContext(NULL, NULL);
  mount_task->set_complete_event(&event_);
  mount_task->set_result(&result_);
  runner_.message_loop()->PostTask(FROM_HERE, mount_task);
  event_.TimedWait(wait_time_);
  ASSERT_TRUE(event_.IsSignaled());
}

TEST_F(MountTaskTest, RemoveTrackedSubdirectories) {
  ASSERT_FALSE(event_.IsSignaled());
  MountTask* mount_task = new MountTaskRemoveTrackedSubdirectories(NULL,
                                                                   &mount_);
  mount_task->set_complete_event(&event_);
  mount_task->set_result(&result_);
  runner_.message_loop()->PostTask(FROM_HERE, mount_task);
  event_.TimedWait(wait_time_);
  ASSERT_TRUE(event_.IsSignaled());
}

TEST_F(MountTaskTest, AutomaticFreeDiskSpace) {
  ASSERT_FALSE(event_.IsSignaled());
  MountTask* mount_task = new MountTaskAutomaticFreeDiskSpace(NULL, &mount_);
  mount_task->set_complete_event(&event_);
  mount_task->set_result(&result_);
  runner_.message_loop()->PostTask(FROM_HERE, mount_task);
  event_.TimedWait(wait_time_);
  ASSERT_TRUE(event_.IsSignaled());
}

} // namespace cryptohome
