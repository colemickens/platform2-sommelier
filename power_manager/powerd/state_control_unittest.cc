// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include <string>

#include "base/basictypes.h"
#include "power_manager/powerd/state_control.h"
#include "power_state_control.pb.cc"

using ::testing::Test;

namespace power_manager {

class StateControlTest : public Test {
 public:
  StateControlTest() {}

 protected:
  StateControl state_control_;
  void NothingDisabled();
  void CheckDisabled(bool disable_idle_dim,
                     bool disable_idle_blank,
                     bool disable_idle_suspend,
                     bool disable_lid_suspend);
  void DisableAndCheck(bool disable_idle_dim,
                       bool disable_idle_blank,
                       bool disable_idle_suspend,
                       bool disable_lid_suspend);
};  // class StateControlTest

void StateControlTest::NothingDisabled() {
  EXPECT_FALSE(state_control_.IsStateDisabled(kIdleDimDisabled));
  EXPECT_FALSE(state_control_.IsStateDisabled(kIdleBlankDisabled));
  EXPECT_FALSE(state_control_.IsStateDisabled(kIdleSuspendDisabled));
  EXPECT_FALSE(state_control_.IsStateDisabled(kLidSuspendDisabled));
}
void StateControlTest::CheckDisabled(bool disable_idle_dim,
                                     bool disable_idle_blank,
                                     bool disable_idle_suspend,
                                     bool disable_lid_suspend) {
  EXPECT_EQ(disable_idle_dim,
            state_control_.IsStateDisabled(kIdleDimDisabled));
  EXPECT_EQ(disable_idle_blank,
               state_control_.IsStateDisabled(kIdleBlankDisabled));
  EXPECT_EQ(disable_idle_suspend,
               state_control_.IsStateDisabled(kIdleSuspendDisabled));
  EXPECT_EQ(disable_lid_suspend,
               state_control_.IsStateDisabled(kLidSuspendDisabled));
}

void StateControlTest::DisableAndCheck(bool disable_idle_dim,
                                       bool disable_idle_blank,
                                       bool disable_idle_suspend,
                                       bool disable_lid_suspend) {
  StateControlInfo info;
  int value;

  memset(&info, 0, sizeof(info));
  info.duration = 100;
  info.disable_idle_dim = disable_idle_dim;
  info.disable_idle_blank = disable_idle_blank;
  info.disable_idle_suspend = disable_idle_suspend;
  info.disable_lid_suspend = disable_lid_suspend;
  EXPECT_TRUE(state_control_.StateOverrideRequestStruct(&info, &value));
  CheckDisabled(disable_idle_dim, disable_idle_blank, disable_idle_suspend,
                disable_lid_suspend);
  state_control_.RemoveOverride(value);
}

TEST_F(StateControlTest, SingleRequests) {
  {
    SCOPED_TRACE("idle suspend disable");
    DisableAndCheck(false, false, true, false);
  }
  {
    SCOPED_TRACE("idle blank and suspend disable");
    DisableAndCheck(false, true, true, false);
  }
  {
    SCOPED_TRACE("idle dim, blank and suspend disable");
    DisableAndCheck(true, true, true, false);
  }
  {
    SCOPED_TRACE("idle suspend disable and lid disable");
    DisableAndCheck(false, false, true, true);
  }
  {
    SCOPED_TRACE("idle blank and suspend disable and lid disable");
    DisableAndCheck(false, true, true, true);
  }
  {
    SCOPED_TRACE("idle dim, blank and suspend disable and lid disable");
    DisableAndCheck(true, true, true, true);
  }
}

TEST_F(StateControlTest, InterleavedRequests) {
  StateControlInfo info;
  int idle_and_lid;
  int all;
  int lid_suspend;

  memset(&info, 0, sizeof(info));
  info.duration = 100;

  {
    SCOPED_TRACE("idle and lid suspend disabled");
    info.disable_idle_suspend = true;
    info.disable_lid_suspend = true;
    EXPECT_TRUE(state_control_.StateOverrideRequestStruct(&info,
                                                          &idle_and_lid));
    CheckDisabled(false, false, true, true);
  }

  {
    SCOPED_TRACE("all disabled");
    info.disable_idle_dim = true;
    info.disable_idle_blank = true;
    info.disable_idle_suspend = true;
    info.disable_lid_suspend = true;
    EXPECT_TRUE(state_control_.StateOverrideRequestStruct(&info, &all));
    CheckDisabled(true, true, true, true);
  }

  {
    SCOPED_TRACE("lid suspend disabled");
    info.disable_idle_dim = false;
    info.disable_idle_blank = false;
    info.disable_idle_suspend = false;
    info.disable_lid_suspend = true;
    EXPECT_TRUE(state_control_.StateOverrideRequestStruct(&info,
                                                          &lid_suspend));
    CheckDisabled(true, true, true, true);
  }

  {
    SCOPED_TRACE("all disabled request removed");
    state_control_.RemoveOverride(all);
    CheckDisabled(false, false, true, true);
  }

  {
    SCOPED_TRACE("idle and lid suspend request removed");
    state_control_.RemoveOverride(idle_and_lid);
    CheckDisabled(false, false, false, true);
  }

  {
    SCOPED_TRACE("lid suspend request removed");
    state_control_.RemoveOverride(lid_suspend);
    CheckDisabled(false, false, false, false);
  }
}

TEST_F(StateControlTest, TimingRequests) {
  StateControlInfo info;
  int idle_and_lid;
  int all;
  int lid_suspend;
  time_t start_time, end_time;

  start_time = time(NULL);
  memset(&info, 0, sizeof(info));
  {
    SCOPED_TRACE("idle and lid suspend disabled");
    info.duration = 120;
    info.disable_idle_suspend = true;
    info.disable_lid_suspend = true;
    EXPECT_TRUE(state_control_.StateOverrideRequestStruct(&info,
                                                          &idle_and_lid));
    CheckDisabled(false, false, true, true);
  }

  {
    SCOPED_TRACE("all disabled");
    info.duration = 60;
    info.disable_idle_dim = true;
    info.disable_idle_blank = true;
    info.disable_idle_suspend = true;
    info.disable_lid_suspend = true;
    EXPECT_TRUE(state_control_.StateOverrideRequestStruct(&info, &all));
    CheckDisabled(true, true, true, true);
  }

  {
    info.duration = 180;
    SCOPED_TRACE("lid suspend disabled");
    info.disable_idle_dim = false;
    info.disable_idle_blank = false;
    info.disable_idle_suspend = false;
    info.disable_lid_suspend = true;
    EXPECT_TRUE(state_control_.StateOverrideRequestStruct(&info,
                                                          &lid_suspend));
    CheckDisabled(true, true, true, true);
  }
  end_time = time(NULL);
  ASSERT_LT(end_time - start_time, 2);

  {
    SCOPED_TRACE("all disabled requests removed");
    state_control_.RescanState(end_time + 65);
    CheckDisabled(false, false, true, true);
  }

  {
    SCOPED_TRACE("idle and lid suspend request removed");
    state_control_.RescanState(end_time + 125);
    CheckDisabled(false, false, false, true);
  }

  {
    SCOPED_TRACE("lid suspend request removed");
    state_control_.RescanState(end_time + 185);
    CheckDisabled(false, false, false, false);
  }
}

// Test that we properly wrap when we exceed maxint
TEST_F(StateControlTest, WrapTest) {
  StateControlInfo info;
  int value;

  memset(&info, 0, sizeof(info));
  info.duration = 60;
  info.disable_idle_suspend = true;
  // We only attempt 20 times to find a hole in the map so go slightly over
  for (int i = 0; i < 25; i++) {
    ASSERT_TRUE(state_control_.StateOverrideRequestStruct(&info, &value));
    ASSERT_EQ(value, i+1);
  }
  state_control_.RemoveOverride(1);
  {
    SCOPED_TRACE("before wrap.  idle suspend should be true");
    CheckDisabled(false, false, true, false);
  }
  state_control_.last_id_ = UINT32_MAX - 1;
  info.disable_idle_suspend = false;
  info.disable_lid_suspend = true;
  // First two should succeed into 0 & 1
  ASSERT_TRUE(state_control_.StateOverrideRequestStruct(&info, &value));
  ASSERT_EQ(value, UINT32_MAX);
  ASSERT_TRUE(state_control_.StateOverrideRequestStruct(&info, &value));
  ASSERT_EQ(value, 1);
  // Third time should fail as spots 2-25 are filled from above
  ASSERT_FALSE(state_control_.StateOverrideRequestStruct(&info, &value));
  // Fourth time should suceed at spot 26
  ASSERT_TRUE(state_control_.StateOverrideRequestStruct(&info, &value));
  ASSERT_EQ(value, 26);
  {
    SCOPED_TRACE("after wrap.  idle and lid suspend should be true");
    CheckDisabled(false, false, true, true);
  }
  for (int i = 2; i < 26; i++) {
    state_control_.RemoveOverride(i);
  }
  {
    SCOPED_TRACE("removed early tests.  only lid suspend should be true");
    CheckDisabled(false, false, false, true);
  }
  // Now test for wrapping that ends on 0.
  state_control_.RemoveOverride(UINT32_MAX);  // Cleanup previous request
  state_control_.last_id_ = UINT32_MAX - 20;
  for (int i= 0; i < 20; i++) {
    ASSERT_TRUE(state_control_.StateOverrideRequestStruct(&info, &value));
  }
  ASSERT_EQ(value, UINT32_MAX);
  state_control_.last_id_ = UINT32_MAX - 19;
  EXPECT_FALSE(state_control_.StateOverrideRequestStruct(&info, &value));
  EXPECT_EQ(state_control_.last_id_, 0);
}

TEST_F(StateControlTest, InvalidRequests) {
  StateControlInfo info;
  int value;

  memset(&info, 0, sizeof(info));
  EXPECT_FALSE(state_control_.StateOverrideRequestStruct(&info, &value));
  info.duration = state_control_.max_duration_ + 1;
  EXPECT_FALSE(state_control_.StateOverrideRequestStruct(&info, &value));
  info.duration = 100;
  info.disable_idle_dim = true;
  EXPECT_FALSE(state_control_.StateOverrideRequestStruct(&info, &value));
  info.disable_idle_blank = true;
  EXPECT_FALSE(state_control_.StateOverrideRequestStruct(&info, &value));
  info.disable_idle_dim = false;
  EXPECT_FALSE(state_control_.StateOverrideRequestStruct(&info, &value));
}

TEST_F(StateControlTest, Protobuf) {
  PowerStateControl protobuf;
  std::string serialized_proto;
  int value;

  protobuf.set_request_id(0);
  protobuf.set_duration(1);
  protobuf.set_disable_idle_dim(false);
  protobuf.set_disable_idle_blank(false);
  protobuf.set_disable_idle_suspend(false);
  protobuf.set_disable_lid_suspend(true);
  ASSERT_NE(protobuf.SerializeToString(&serialized_proto), NULL);
  state_control_.StateOverrideRequest(serialized_proto.data(),
                                      serialized_proto.size(),
                                      &value);
  {
    SCOPED_TRACE("protobuf test");
    CheckDisabled(false, false, false, true);
  }
}

};  // namespace power_manager
