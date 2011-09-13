// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cros-disks/device-event-moderator.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "cros-disks/device-event.h"
#include "cros-disks/device-event-dispatcher-interface.h"
#include "cros-disks/device-event-source-interface.h"

using testing::DoAll;
using testing::InSequence;
using testing::Return;
using testing::SetArgumentPointee;
using testing::_;

namespace {

const char kSessionUser[] = "user";

}  // namespace

namespace cros_disks {

class MockDeviceEventDispatcher : public DeviceEventDispatcherInterface {
 public:
  MOCK_METHOD1(DispatchDeviceEvent, void(const DeviceEvent& event));
};

class MockDeviceEventSource : public DeviceEventSourceInterface {
 public:
  MOCK_METHOD1(GetDeviceEvent, bool(DeviceEvent* event));
};

class DeviceEventModeratorTest : public ::testing::Test {
 public:
  DeviceEventModeratorTest()
      : moderator_(&event_dispatcher_, &event_source_) {
  }

 protected:
  MockDeviceEventDispatcher event_dispatcher_;
  MockDeviceEventSource event_source_;
  DeviceEventModerator moderator_;
};

TEST_F(DeviceEventModeratorTest, DispatchQueuedDeviceEventsWithEmptyQueue) {
  EXPECT_CALL(event_dispatcher_, DispatchDeviceEvent(_)).Times(0);
  moderator_.DispatchQueuedDeviceEvents();
}

TEST_F(DeviceEventModeratorTest, OnScreenIsLocked) {
  InSequence sequence;
  EXPECT_CALL(event_source_, GetDeviceEvent(_))
      .WillOnce(DoAll(
          SetArgumentPointee<0>(DeviceEvent(DeviceEvent::kDeviceAdded, "1")),
          Return(true)))
      .WillOnce(Return(false))
      .WillOnce(DoAll(
          SetArgumentPointee<0>(DeviceEvent(DeviceEvent::kDeviceAdded, "2")),
          Return(true)));
  EXPECT_CALL(event_dispatcher_, DispatchDeviceEvent(_)).Times(0);

  moderator_.OnScreenIsLocked();
  moderator_.ProcessNextDeviceEvent();
  moderator_.ProcessNextDeviceEvent();
  moderator_.ProcessNextDeviceEvent();
  EXPECT_TRUE(moderator_.is_event_queued());
}

TEST_F(DeviceEventModeratorTest, OnScreenIsLockedAndThenUnlocked) {
  InSequence sequence;
  EXPECT_CALL(event_source_, GetDeviceEvent(_))
      .WillOnce(DoAll(
          SetArgumentPointee<0>(DeviceEvent(DeviceEvent::kDeviceAdded, "1")),
          Return(true)))
      .WillOnce(Return(false))
      .WillOnce(DoAll(
          SetArgumentPointee<0>(DeviceEvent(DeviceEvent::kDeviceAdded, "2")),
          Return(true)));
  EXPECT_CALL(event_dispatcher_,
              DispatchDeviceEvent(DeviceEvent(DeviceEvent::kDeviceAdded, "1")));
  EXPECT_CALL(event_dispatcher_,
              DispatchDeviceEvent(DeviceEvent(DeviceEvent::kDeviceAdded, "2")));

  moderator_.OnScreenIsLocked();
  EXPECT_TRUE(moderator_.is_event_queued());
  moderator_.ProcessNextDeviceEvent();
  moderator_.ProcessNextDeviceEvent();
  moderator_.ProcessNextDeviceEvent();
  moderator_.OnScreenIsUnlocked();
  EXPECT_FALSE(moderator_.is_event_queued());
}

TEST_F(DeviceEventModeratorTest, OnScreenIsUnlocked) {
  InSequence sequence;
  EXPECT_CALL(event_source_, GetDeviceEvent(_))
      .WillOnce(DoAll(
          SetArgumentPointee<0>(DeviceEvent(DeviceEvent::kDeviceAdded, "1")),
          Return(true)))
      .RetiresOnSaturation();
  EXPECT_CALL(event_dispatcher_,
              DispatchDeviceEvent(DeviceEvent(DeviceEvent::kDeviceAdded, "1")));
  EXPECT_CALL(event_source_, GetDeviceEvent(_))
      .WillOnce(Return(false))
      .RetiresOnSaturation();
  EXPECT_CALL(event_source_, GetDeviceEvent(_))
      .WillOnce(DoAll(
          SetArgumentPointee<0>(DeviceEvent(DeviceEvent::kDeviceAdded, "2")),
          Return(true)))
      .RetiresOnSaturation();
  EXPECT_CALL(event_dispatcher_,
              DispatchDeviceEvent(DeviceEvent(DeviceEvent::kDeviceAdded, "2")));

  moderator_.OnScreenIsUnlocked();
  EXPECT_FALSE(moderator_.is_event_queued());
  moderator_.ProcessNextDeviceEvent();
  moderator_.ProcessNextDeviceEvent();
  moderator_.ProcessNextDeviceEvent();
}

TEST_F(DeviceEventModeratorTest, OnSessionStarted) {
  InSequence sequence;
  EXPECT_CALL(event_source_, GetDeviceEvent(_))
      .WillOnce(DoAll(
          SetArgumentPointee<0>(DeviceEvent(DeviceEvent::kDeviceAdded, "1")),
          Return(true)))
      .RetiresOnSaturation();
  EXPECT_CALL(event_dispatcher_,
              DispatchDeviceEvent(DeviceEvent(DeviceEvent::kDeviceAdded, "1")));
  EXPECT_CALL(event_source_, GetDeviceEvent(_))
      .WillOnce(Return(false))
      .RetiresOnSaturation();
  EXPECT_CALL(event_source_, GetDeviceEvent(_))
      .WillOnce(DoAll(
          SetArgumentPointee<0>(DeviceEvent(DeviceEvent::kDeviceAdded, "2")),
          Return(true)))
      .RetiresOnSaturation();
  EXPECT_CALL(event_dispatcher_,
              DispatchDeviceEvent(DeviceEvent(DeviceEvent::kDeviceAdded, "2")));

  moderator_.OnSessionStarted(kSessionUser);
  EXPECT_FALSE(moderator_.is_event_queued());
  moderator_.ProcessNextDeviceEvent();
  moderator_.ProcessNextDeviceEvent();
  moderator_.ProcessNextDeviceEvent();
}

TEST_F(DeviceEventModeratorTest, OnSessionStopped) {
  InSequence sequence;
  EXPECT_CALL(event_source_, GetDeviceEvent(_))
      .WillOnce(DoAll(
          SetArgumentPointee<0>(DeviceEvent(DeviceEvent::kDeviceAdded, "1")),
          Return(true)))
      .WillOnce(Return(false))
      .WillOnce(DoAll(
          SetArgumentPointee<0>(DeviceEvent(DeviceEvent::kDeviceAdded, "2")),
          Return(true)));
  EXPECT_CALL(event_dispatcher_, DispatchDeviceEvent(_)).Times(0);

  moderator_.OnSessionStopped(kSessionUser);
  moderator_.ProcessNextDeviceEvent();
  moderator_.ProcessNextDeviceEvent();
  moderator_.ProcessNextDeviceEvent();
  EXPECT_TRUE(moderator_.is_event_queued());
}

TEST_F(DeviceEventModeratorTest, OnSessionStoppedAndThenStarted) {
  InSequence sequence;
  EXPECT_CALL(event_source_, GetDeviceEvent(_))
      .WillOnce(DoAll(
          SetArgumentPointee<0>(DeviceEvent(DeviceEvent::kDeviceAdded, "1")),
          Return(true)))
      .WillOnce(Return(false))
      .WillOnce(DoAll(
          SetArgumentPointee<0>(DeviceEvent(DeviceEvent::kDeviceAdded, "2")),
          Return(true)));
  EXPECT_CALL(event_dispatcher_,
              DispatchDeviceEvent(DeviceEvent(DeviceEvent::kDeviceAdded, "1")));
  EXPECT_CALL(event_dispatcher_,
              DispatchDeviceEvent(DeviceEvent(DeviceEvent::kDeviceAdded, "2")));

  moderator_.OnSessionStopped(kSessionUser);
  EXPECT_TRUE(moderator_.is_event_queued());
  moderator_.ProcessNextDeviceEvent();
  moderator_.ProcessNextDeviceEvent();
  moderator_.ProcessNextDeviceEvent();
  moderator_.OnSessionStarted(kSessionUser);
  EXPECT_FALSE(moderator_.is_event_queued());
}

}  // namespace cros_disks
