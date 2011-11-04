// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "base/logging.h"
#include "power_manager/backlight_controller.h"
#include "power_manager/mock_backlight.h"
#include "power_manager/power_constants.h"
#include "power_manager/power_prefs.h"

namespace power_manager {

using ::testing::_;
using ::testing::DoAll;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::SetArgumentPointee;

static const int64 kDefaultBrightness = 50;
static const int64 kMaxBrightness = 100;
static const int64 kPluggedBrightness = 70;
static const int64 kUnpluggedBrightness = 30;
static const int64 kAlsBrightness = 0;

// Repeating either increase or decrease brightness this many times should
// always leave the brightness at a limit.
const int kStepsToHitLimit = 20;

class BacklightControllerTest : public ::testing::Test {
 public:
  BacklightControllerTest()
      : prefs_(FilePath("."), FilePath(".")),
        controller_(&backlight_, &prefs_) {
  }

  virtual void SetUp() {
    EXPECT_CALL(backlight_, GetBrightness(NotNull(), NotNull()))
        .WillRepeatedly(DoAll(SetArgumentPointee<0>(kDefaultBrightness),
                              SetArgumentPointee<1>(kMaxBrightness),
                              Return(true)));
    prefs_.SetInt64(kPluggedBrightnessOffset, kPluggedBrightness);
    prefs_.SetInt64(kUnpluggedBrightnessOffset, kUnpluggedBrightness);
    prefs_.SetInt64(kAlsBrightnessLevel, kAlsBrightness);
    CHECK(controller_.Init());
  }

 protected:
  ::testing::StrictMock<MockBacklight> backlight_;
  PowerPrefs prefs_;
  BacklightController controller_;
};

TEST_F(BacklightControllerTest, IncreaseBrightness) {
  ASSERT_TRUE(controller_.SetPowerState(BACKLIGHT_ACTIVE));
  ASSERT_TRUE(controller_.OnPlugEvent(false));
  EXPECT_EQ(kUnpluggedBrightness, controller_.local_brightness());

  double old_local_brightness = controller_.local_brightness();
  controller_.IncreaseBrightness();
  // Check that the first step increases the brightness; within the loop
  // will just ensure that the brightness never decreases.
  EXPECT_GT(controller_.local_brightness(), old_local_brightness);

  for (int i = 0; i < kStepsToHitLimit; ++i) {
    old_local_brightness = controller_.local_brightness();
    controller_.IncreaseBrightness();
    EXPECT_GE(controller_.local_brightness(), old_local_brightness);
  }

  EXPECT_EQ(kMaxBrightness, controller_.local_brightness());
}

TEST_F(BacklightControllerTest, DecreaseBrightness) {
  ASSERT_TRUE(controller_.SetPowerState(BACKLIGHT_ACTIVE));
  ASSERT_TRUE(controller_.OnPlugEvent(true));
  EXPECT_EQ(kPluggedBrightness, controller_.local_brightness());

  double old_local_brightness = controller_.local_brightness();
  controller_.DecreaseBrightness(true);
  // Check that the first step decreases the brightness; within the loop
  // will just ensure that the brightness never increases.
  EXPECT_LT(controller_.local_brightness(), old_local_brightness);

  for (int i = 0; i < kStepsToHitLimit; ++i) {
    old_local_brightness = controller_.local_brightness();
    controller_.DecreaseBrightness(true);
    EXPECT_LE(controller_.local_brightness(), old_local_brightness);
  }

  // Backlight should now be off.
  EXPECT_EQ(0, controller_.local_brightness());
}

TEST_F(BacklightControllerTest, DecreaseBrightnessDisallowOff) {
  ASSERT_TRUE(controller_.SetPowerState(BACKLIGHT_ACTIVE));
  ASSERT_TRUE(controller_.OnPlugEvent(true));
  EXPECT_EQ(kPluggedBrightness, controller_.local_brightness());

  for (int i = 0; i < kStepsToHitLimit; ++i)
    controller_.DecreaseBrightness(false);

  // Backlight must still be on.
  EXPECT_GT(controller_.local_brightness(), 0);
}

}  // namespace power_manager
