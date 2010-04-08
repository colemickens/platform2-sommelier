// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include "base/command_line.h"
#include "base/logging.h"
#include "power_manager/idle_dimmer.h"
#include "power_manager/mock_backlight.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::SetArgumentPointee;
using ::testing::Test;

namespace power_manager {

static const int64 kIdleBrightness = 3;
static const int64 kDefaultBrightness = 5;
static const int64 kMaxBrightness = 10;

class IdleDimmerTest : public Test {
 public:
  IdleDimmerTest(): backlight_(), dimmer_(kIdleBrightness, &backlight_) {}

 protected:
  MockBacklight backlight_;
  IdleDimmer dimmer_;
};

// Test that OnIdleEvent does not set the brightness when GetBrightness
// returns false
TEST_F(IdleDimmerTest, TestBrokenBacklight) {
  EXPECT_CALL(backlight_, GetBrightness(NotNull(), NotNull()))
      .WillOnce(DoAll(SetArgumentPointee<0>(kDefaultBrightness),
                      SetArgumentPointee<1>(kMaxBrightness),
                      Return(false)));
  EXPECT_CALL(backlight_, SetBrightness(_))
      .Times(0);
  dimmer_.OnIdleEvent(true, 0);
}

// Test that OnIdleEvent sets the brightness appropriately when
// the user becomes idle
TEST_F(IdleDimmerTest, TestIdle) {
  EXPECT_CALL(backlight_, GetBrightness(NotNull(), NotNull()))
      .WillOnce(DoAll(SetArgumentPointee<0>(kDefaultBrightness),
                      SetArgumentPointee<1>(kMaxBrightness),
                      Return(true)));
  EXPECT_CALL(backlight_, SetBrightness(kIdleBrightness))
      .WillOnce(Return(true));
  dimmer_.OnIdleEvent(true, 0);
}

// Test that OnIdleEvent does not mess with the user's brightness settings
// when we receive duplicate idle events
TEST_F(IdleDimmerTest, TestDuplicateIdleEvent) {
  EXPECT_CALL(backlight_, GetBrightness(NotNull(), NotNull()))
      .WillOnce(DoAll(SetArgumentPointee<0>(kDefaultBrightness),
                      SetArgumentPointee<1>(kMaxBrightness),
                      Return(true)));
  EXPECT_CALL(backlight_, SetBrightness(kIdleBrightness))
      .WillOnce(Return(true));
  dimmer_.OnIdleEvent(true, 0);
  EXPECT_CALL(backlight_, GetBrightness(NotNull(), NotNull()))
      .WillOnce(DoAll(SetArgumentPointee<0>(kDefaultBrightness),
                      SetArgumentPointee<1>(kMaxBrightness),
                      Return(true)));
  dimmer_.OnIdleEvent(true, 0);
}

// Test that OnIdleEvent does not set the brightness when the screen is
// already dim and the user becomes idle
TEST_F(IdleDimmerTest, TestIdleNoop) {
  EXPECT_CALL(backlight_, GetBrightness(NotNull(), NotNull()))
      .WillOnce(DoAll(SetArgumentPointee<0>(kIdleBrightness),
                      SetArgumentPointee<1>(kMaxBrightness),
                      Return(true)));
  EXPECT_CALL(backlight_, SetBrightness(_))
      .Times(0);
  dimmer_.OnIdleEvent(true, 0);
}

// Test that OnIdleEvent does not set the brightness when we receive
// an idle event for a non-idle user
TEST_F(IdleDimmerTest, TestNonIdle) {
  EXPECT_CALL(backlight_, GetBrightness(NotNull(), NotNull()))
      .WillOnce(DoAll(SetArgumentPointee<0>(kDefaultBrightness),
                      SetArgumentPointee<1>(kMaxBrightness),
                      Return(true)));
  EXPECT_CALL(backlight_, SetBrightness(_))
      .Times(0);
  dimmer_.OnIdleEvent(false, 0);
}

// Test that OnIdleEvent sets the brightness appropriately when the
// user transitions to idle and back
TEST_F(IdleDimmerTest, TestIdleTransition) {
  EXPECT_CALL(backlight_, GetBrightness(NotNull(), NotNull()))
      .WillOnce(DoAll(SetArgumentPointee<0>(kDefaultBrightness),
                      SetArgumentPointee<1>(kMaxBrightness),
                      Return(true)));
  EXPECT_CALL(backlight_, SetBrightness(kIdleBrightness))
      .WillOnce(Return(true));
  dimmer_.OnIdleEvent(true, 0);
  EXPECT_CALL(backlight_, GetBrightness(NotNull(), NotNull()))
      .WillOnce(DoAll(SetArgumentPointee<0>(kIdleBrightness+2),
                      SetArgumentPointee<1>(kMaxBrightness),
                      Return(true)));
  EXPECT_CALL(backlight_, SetBrightness(kDefaultBrightness+2))
      .WillOnce(Return(true));
  dimmer_.OnIdleEvent(false, 0);
}

// Test that OnIdleEvent sets the brightness appropriately when the
// user transitions to idle and back, and the max brightness setting
// is reached
TEST_F(IdleDimmerTest, TestOverflowIdleTransition) {
  EXPECT_CALL(backlight_, GetBrightness(NotNull(), NotNull()))
      .WillOnce(DoAll(SetArgumentPointee<0>(kDefaultBrightness),
                      SetArgumentPointee<1>(kMaxBrightness),
                      Return(true)));
  EXPECT_CALL(backlight_, SetBrightness(kIdleBrightness))
      .WillOnce(Return(true));
  dimmer_.OnIdleEvent(true, 0);
  EXPECT_CALL(backlight_, GetBrightness(NotNull(), NotNull()))
      .WillOnce(DoAll(SetArgumentPointee<0>(kMaxBrightness-1),
                      SetArgumentPointee<1>(kMaxBrightness),
                      Return(true)));
  EXPECT_CALL(backlight_, SetBrightness(kMaxBrightness))
      .WillOnce(Return(true));
  dimmer_.OnIdleEvent(false, 0);
}

}  // namespace power_manager
