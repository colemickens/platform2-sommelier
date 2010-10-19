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
static const int64 kPluggedBrightnessP = kPluggedBrightness * 10;
static const int64 kUnpluggedBrightnessP = kUnpluggedBrightness * 10;

class PlugDimmerTest : public Test {
 public:
  PlugDimmerTest()
      : prefs_(FilePath("/tmp"), FilePath("/tmp")),
        backlight_ctl_(&backlight_, &prefs_) {
    EXPECT_CALL(backlight_, GetBrightness(NotNull(), NotNull()))
        .WillOnce(DoAll(SetArgumentPointee<0>(kDefaultBrightness),
                        SetArgumentPointee<1>(kMaxBrightness),
                        Return(true)));
    prefs_.SetInt64(kPluggedBrightnessOffset, kPluggedBrightnessP);
    prefs_.SetInt64(kUnpluggedBrightnessOffset, kUnpluggedBrightnessP);
    CHECK(backlight_ctl_.Init());
  }

 protected:
  MockBacklight backlight_;
  PowerPrefs prefs_;
  BacklightController backlight_ctl_;
};

// Test that OnPlugEvent sets the brightness appropriately when
// the computer is plugged and unplugged.
TEST_F(PlugDimmerTest, TestPlug) {
  EXPECT_CALL(backlight_, SetBrightness(kUnpluggedBrightness))
      .WillOnce(Return(true));
  backlight_ctl_.OnPlugEvent(false);
  EXPECT_CALL(backlight_, SetBrightness(kPluggedBrightness))
      .WillOnce(Return(true));
  backlight_ctl_.OnPlugEvent(true);
  EXPECT_CALL(backlight_, SetBrightness(kUnpluggedBrightness))
      .WillOnce(Return(true));
  backlight_ctl_.OnPlugEvent(false);
  EXPECT_CALL(backlight_, SetBrightness(kPluggedBrightness))
      .WillOnce(Return(true));
  backlight_ctl_.OnPlugEvent(true);
}

// Test that OnPlugEvent sets the brightness appropriately when
// the computer is unplugged and plugged.
TEST_F(PlugDimmerTest, TestUnplug) {
  EXPECT_CALL(backlight_, SetBrightness(kPluggedBrightness))
      .WillOnce(Return(true));
  backlight_ctl_.OnPlugEvent(true);
  EXPECT_CALL(backlight_, SetBrightness(kUnpluggedBrightness))
      .WillOnce(Return(true));
  backlight_ctl_.OnPlugEvent(false);
  EXPECT_CALL(backlight_, SetBrightness(kPluggedBrightness))
      .WillOnce(Return(true));
  backlight_ctl_.OnPlugEvent(true);
  EXPECT_CALL(backlight_, SetBrightness(kUnpluggedBrightness))
      .WillOnce(Return(true));
  backlight_ctl_.OnPlugEvent(false);
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
