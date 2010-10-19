// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include "base/command_line.h"
#include "base/logging.h"
#include "power_manager/backlight_controller.h"
#include "power_manager/mock_backlight.h"
#include "power_manager/power_constants.h"
#include "power_manager/power_prefs.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::SetArgumentPointee;
using ::testing::Test;

namespace power_manager {

static const int64 kIdleBrightness = 0;
static const int64 kDefaultBrightness = 5;
static const int64 kMaxBrightness = 10;
static const int64 kPluggedBrightness = 7;
static const int64 kUnpluggedBrightness = 3;
static const int64 kPluggedBrightnessP = kPluggedBrightness * 10;
static const int64 kUnpluggedBrightnessP = kUnpluggedBrightness * 10;

class IdleDimmerTest : public Test {
 public:
  IdleDimmerTest()
      : prefs_(FilePath("/tmp"), FilePath("/tmp")),
        backlight_ctl_(&backlight_, &prefs_) {
    EXPECT_CALL(backlight_, GetBrightness(NotNull(), NotNull()))
        .WillOnce(DoAll(SetArgumentPointee<0>(kDefaultBrightness),
                        SetArgumentPointee<1>(kMaxBrightness),
                        Return(true)));
    prefs_.SetInt64(kPluggedBrightnessOffset, kPluggedBrightnessP);
    prefs_.SetInt64(kUnpluggedBrightnessOffset, kUnpluggedBrightnessP);
    EXPECT_CALL(backlight_, SetBrightness(kPluggedBrightness))
        .WillOnce(Return(true));
    CHECK(backlight_ctl_.Init());
    backlight_ctl_.OnPlugEvent(true);
  }

 protected:
  MockBacklight backlight_;
  PowerPrefs prefs_;
  BacklightController backlight_ctl_;
};

// Test that OnIdleEvent sets the brightness appropriately when
// the user becomes idle
TEST_F(IdleDimmerTest, TestIdle) {
  EXPECT_CALL(backlight_, GetBrightness(NotNull(), NotNull()))
      .WillOnce(DoAll(SetArgumentPointee<0>(kDefaultBrightness),
                      SetArgumentPointee<1>(kMaxBrightness),
                      Return(true)));
  EXPECT_CALL(backlight_, SetBrightness(kIdleBrightness))
      .WillOnce(Return(true));
  backlight_ctl_.SetDimState(BACKLIGHT_DIM);
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
  backlight_ctl_.SetDimState(BACKLIGHT_DIM);
  backlight_ctl_.SetDimState(BACKLIGHT_DIM);
}

// Test that OnIdleEvent does not set the brightness when we receive
// an idle event for a non-idle user
TEST_F(IdleDimmerTest, TestNonIdle) {
  EXPECT_CALL(backlight_, SetBrightness(_))
      .Times(0);
  backlight_ctl_.SetDimState(BACKLIGHT_ACTIVE);
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
  backlight_ctl_.SetDimState(BACKLIGHT_DIM);
  EXPECT_CALL(backlight_, GetBrightness(NotNull(), NotNull()))
      .WillOnce(DoAll(SetArgumentPointee<0>(kIdleBrightness+2),
                      SetArgumentPointee<1>(kMaxBrightness),
                      Return(true)));
  EXPECT_CALL(backlight_, SetBrightness(kDefaultBrightness+2))
      .WillOnce(Return(true));
  backlight_ctl_.SetDimState(BACKLIGHT_ACTIVE);
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
  backlight_ctl_.SetDimState(BACKLIGHT_DIM);
  EXPECT_CALL(backlight_, GetBrightness(NotNull(), NotNull()))
      .WillOnce(DoAll(SetArgumentPointee<0>(kMaxBrightness-1),
                      SetArgumentPointee<1>(kMaxBrightness),
                      Return(true)));
  EXPECT_CALL(backlight_, SetBrightness(kMaxBrightness))
      .WillOnce(Return(true));
  backlight_ctl_.SetDimState(BACKLIGHT_ACTIVE);
}

}  // namespace power_manager
