// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include "base/command_line.h"
#include "base/logging.h"
#include "power_manager/backlight_controller.h"
#include "power_manager/mock_backlight.h"

using ::testing::DoAll;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::SetArgumentPointee;
using ::testing::Test;

namespace power_manager {

static const int64 kPluggedBrightness = 7;
static const int64 kDefaultBrightness = 5;
static const int64 kUnpluggedBrightness = 3;
static const int64 kMaxBrightness = 10;

class PlugDimmerTest : public Test {
 public:
  PlugDimmerTest() : backlight_ctl_(&backlight_) {
    EXPECT_CALL(backlight_, GetBrightness(NotNull(), NotNull()))
        .WillOnce(DoAll(SetArgumentPointee<0>(kDefaultBrightness),
                        SetArgumentPointee<1>(kMaxBrightness),
                        Return(true)));
    backlight_ctl_.set_plugged_brightness_offset(kPluggedBrightness);
    backlight_ctl_.set_unplugged_brightness_offset(kUnpluggedBrightness);
    CHECK(backlight_ctl_.Init());
  }

 protected:
  MockBacklight backlight_;
  BacklightController backlight_ctl_;
};

// Test that OnPlugEvent sets the brightness appropriately when
// the computer is plugged and unplugged.
TEST_F(PlugDimmerTest, TestPlug) {
  EXPECT_CALL(backlight_, SetBrightness(kUnpluggedBrightness))
      .WillOnce(Return(true));
  backlight_ctl_.OnPlugEvent(false);
  EXPECT_CALL(backlight_, GetBrightness(NotNull(), NotNull()))
      .WillOnce(DoAll(SetArgumentPointee<0>(kDefaultBrightness),
                      SetArgumentPointee<1>(kMaxBrightness),
                      Return(true)));
  EXPECT_CALL(backlight_, SetBrightness(kPluggedBrightness))
      .WillOnce(Return(true));
  backlight_ctl_.OnPlugEvent(true);
  EXPECT_CALL(backlight_, GetBrightness(NotNull(), NotNull()))
      .WillOnce(DoAll(SetArgumentPointee<0>(kPluggedBrightness+1),
                      SetArgumentPointee<1>(kMaxBrightness),
                      Return(true)));
  EXPECT_CALL(backlight_, SetBrightness(kDefaultBrightness))
      .WillOnce(Return(true));
  backlight_ctl_.OnPlugEvent(false);
  EXPECT_CALL(backlight_, GetBrightness(NotNull(), NotNull()))
      .WillOnce(DoAll(SetArgumentPointee<0>(kUnpluggedBrightness),
                      SetArgumentPointee<1>(kMaxBrightness),
                      Return(true)));
  EXPECT_CALL(backlight_, SetBrightness(kPluggedBrightness+1))
      .WillOnce(Return(true));
  backlight_ctl_.OnPlugEvent(true);
  EXPECT_CALL(backlight_, GetBrightness(NotNull(), NotNull()))
      .WillOnce(DoAll(SetArgumentPointee<0>(kPluggedBrightness),
                      SetArgumentPointee<1>(kMaxBrightness),
                      Return(true)));
  EXPECT_CALL(backlight_, SetBrightness(kUnpluggedBrightness))
      .WillOnce(Return(true));
  backlight_ctl_.OnPlugEvent(false);
}

// Test that OnPlugEvent sets the brightness appropriately when
// the computer is unplugged and plugged.
TEST_F(PlugDimmerTest, TestUnplug) {
  EXPECT_CALL(backlight_, SetBrightness(kPluggedBrightness))
      .WillOnce(Return(true));
  backlight_ctl_.OnPlugEvent(true);
  EXPECT_CALL(backlight_, GetBrightness(NotNull(), NotNull()))
      .WillOnce(DoAll(SetArgumentPointee<0>(kDefaultBrightness),
                      SetArgumentPointee<1>(kMaxBrightness),
                      Return(true)));
  EXPECT_CALL(backlight_, SetBrightness(kUnpluggedBrightness))
      .WillOnce(Return(true));
  backlight_ctl_.OnPlugEvent(false);
  EXPECT_CALL(backlight_, GetBrightness(NotNull(), NotNull()))
      .WillOnce(DoAll(SetArgumentPointee<0>(kUnpluggedBrightness),
                      SetArgumentPointee<1>(kMaxBrightness),
                      Return(true)));
  EXPECT_CALL(backlight_, SetBrightness(kDefaultBrightness))
      .WillOnce(Return(true));
  backlight_ctl_.OnPlugEvent(true);
}

// Test that OnPlugEvent does not mess with the user's brightness settings
// when we receive duplicate plug events.
TEST_F(PlugDimmerTest, TestDuplicatePlugEvent) {
  EXPECT_CALL(backlight_, SetBrightness(kUnpluggedBrightness))
      .WillOnce(Return(true));
  backlight_ctl_.OnPlugEvent(false);
  backlight_ctl_.OnPlugEvent(false);
}

}  // namespace power_manager
