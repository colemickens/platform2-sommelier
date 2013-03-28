// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/external_backlight_controller.h"

#include <gtest/gtest.h>

#include "base/compiler_specific.h"
#include "power_manager/powerd/mock_backlight_controller_observer.h"
#include "power_manager/powerd/system/backlight_stub.h"
#include "power_manager/powerd/system/display_power_setter_stub.h"

namespace power_manager {

namespace {

// Default maximum and starting current levels for stub backlight.
const int64 kDefaultMaxBacklightLevel = 100;
const int64 kDefaultStartingBacklightLevel = 100;

}  // namespace

class ExternalBacklightControllerTest : public ::testing::Test {
 public:
  ExternalBacklightControllerTest()
      : backlight_(kDefaultMaxBacklightLevel,
                   kDefaultStartingBacklightLevel),
        controller_(&backlight_, &display_power_setter_) {
    CHECK(controller_.Init());
  }

 protected:
  // Returns |backlight_|'s current brightness, mapped to a
  // |controller_|-designated percent in the range [0.0, 100.0].
  double GetBacklightBrightnessPercent() {
    return controller_.LevelToPercent(backlight_.current_level());
  }

  // Returns the brightness percent as reported by the controller.
  double GetControllerBrightnessPercent() {
    double percent = 0.0;
    CHECK(controller_.GetBrightnessPercent(&percent));
    return percent;
  }

  system::BacklightStub backlight_;
  system::DisplayPowerSetterStub display_power_setter_;
  ExternalBacklightController controller_;
};

// Test that backlight failures are reported correctly.
TEST_F(ExternalBacklightControllerTest, FailedBacklightRequest) {
  backlight_.set_should_fail(true);
  double percent = 0;
  EXPECT_FALSE(controller_.GetBrightnessPercent(&percent));
  EXPECT_FALSE(
      controller_.SetUserBrightnessPercent(
          50.0, BacklightController::TRANSITION_INSTANT));
  EXPECT_FALSE(controller_.IncreaseUserBrightness());
  EXPECT_FALSE(controller_.DecreaseUserBrightness(true /* allow_off */));
  EXPECT_FALSE(controller_.Init());
}

// Test that we reinitialize the brightness range when notified that the
// backlight device has changed.
TEST_F(ExternalBacklightControllerTest, ReinitializeOnDeviceChange) {
  const int kStartingBacklightLevel = 67;
  backlight_.set_current_level(kStartingBacklightLevel);
  EXPECT_EQ(GetBacklightBrightnessPercent(),
            GetControllerBrightnessPercent());

  const int kNewMaxBacklightLevel = 60;
  const int kNewBacklightLevel = 45;
  backlight_.set_max_level(kNewMaxBacklightLevel);
  backlight_.set_current_level(kNewBacklightLevel);
  backlight_.NotifyObservers();
  EXPECT_EQ(GetBacklightBrightnessPercent(),
            GetControllerBrightnessPercent());
}

// Test that we dim whenever requested to do so.  We should do this without
// changing the monitor's brightness settings.
TEST_F(ExternalBacklightControllerTest, DimScreen) {
  const int kStartingBacklightLevel = 43;
  backlight_.set_current_level(kStartingBacklightLevel);
  EXPECT_FALSE(display_power_setter_.dimmed());
  EXPECT_EQ(kStartingBacklightLevel, backlight_.current_level());

  controller_.SetDimmedForInactivity(true);
  EXPECT_TRUE(display_power_setter_.dimmed());
  EXPECT_EQ(kStartingBacklightLevel, backlight_.current_level());

  controller_.SetDimmedForInactivity(false);
  EXPECT_FALSE(display_power_setter_.dimmed());
  EXPECT_EQ(kStartingBacklightLevel, backlight_.current_level());

  controller_.SetOffForInactivity(true);
  EXPECT_FALSE(display_power_setter_.dimmed());
  EXPECT_EQ(kStartingBacklightLevel, backlight_.current_level());

  controller_.SetSuspended(true);
  EXPECT_FALSE(display_power_setter_.dimmed());
  EXPECT_EQ(kStartingBacklightLevel, backlight_.current_level());
}

// Test that we turn the screen off when requested to do so and when the machine
// is suspended.  We should do this without changing the monitor's brightness
// settings.
TEST_F(ExternalBacklightControllerTest, TurnScreenOff) {
  const int kStartingBacklightLevel = 65;
  backlight_.set_current_level(kStartingBacklightLevel);
  EXPECT_EQ(chromeos::DISPLAY_POWER_ALL_ON, display_power_setter_.state());
  EXPECT_EQ(kStartingBacklightLevel, backlight_.current_level());

  controller_.SetDimmedForInactivity(true);
  EXPECT_EQ(chromeos::DISPLAY_POWER_ALL_ON, display_power_setter_.state());
  EXPECT_EQ(kStartingBacklightLevel, backlight_.current_level());

  controller_.SetOffForInactivity(true);
  EXPECT_EQ(chromeos::DISPLAY_POWER_ALL_OFF, display_power_setter_.state());
  EXPECT_EQ(kStartingBacklightLevel, backlight_.current_level());

  controller_.SetSuspended(true);
  EXPECT_EQ(chromeos::DISPLAY_POWER_ALL_OFF, display_power_setter_.state());
  EXPECT_EQ(kStartingBacklightLevel, backlight_.current_level());

  controller_.SetSuspended(false);
  controller_.SetOffForInactivity(false);
  controller_.SetDimmedForInactivity(false);
  EXPECT_EQ(chromeos::DISPLAY_POWER_ALL_ON, display_power_setter_.state());
  EXPECT_EQ(kStartingBacklightLevel, backlight_.current_level());
}

// Test that we correctly count the number of user-initiated brightness
// adjustments.
TEST_F(ExternalBacklightControllerTest, CountAdjustments) {
  const int kNumUserUpAdjustments = 10;
  const int kNumUserDownAdjustments = 8;
  const int kNumUserAbsoluteAdjustments = 6;
  const int kTotalUserAdjustments =
      kNumUserUpAdjustments +
      kNumUserDownAdjustments +
      kNumUserAbsoluteAdjustments;
  for (int i = 0; i < kNumUserUpAdjustments; ++i)
    controller_.IncreaseUserBrightness();
  for (int i = 0; i < kNumUserDownAdjustments; ++i)
    controller_.DecreaseUserBrightness(true /* allow_off */);
  for (int i = 0; i < kNumUserAbsoluteAdjustments; ++i) {
    controller_.SetUserBrightnessPercent(
        50.0, BacklightController::TRANSITION_INSTANT);
  }

  EXPECT_EQ(kTotalUserAdjustments, controller_.GetNumUserAdjustments());
}

// Test that GetBrightnessPercent() just returns the brightness as reported
// by the display.
TEST_F(ExternalBacklightControllerTest, QueryBrightness) {
  const int kNewLevel = kDefaultMaxBacklightLevel / 2;
  backlight_.set_current_level(kNewLevel);
  EXPECT_DOUBLE_EQ(GetBacklightBrightnessPercent(),
                   GetControllerBrightnessPercent());
  ASSERT_EQ(kNewLevel, backlight_.current_level());
}

// Test that requests to change the brightness are honored.
TEST_F(ExternalBacklightControllerTest, ChangeBrightness) {
  const int kNumAdjustmentsToReachLimit = 20;

  // Test setting the brightness to an absolute level.
  const double kNewPercent = 75.0;
  controller_.SetUserBrightnessPercent(kNewPercent,
                                       BacklightController::TRANSITION_INSTANT);
  EXPECT_DOUBLE_EQ(kNewPercent, GetBacklightBrightnessPercent());

  // Increase enough times to hit 100%.
  for (int i = 0; i < kNumAdjustmentsToReachLimit; ++i)
    controller_.IncreaseUserBrightness();
  EXPECT_DOUBLE_EQ(100.0, GetBacklightBrightnessPercent());

  // Decrease enough times to hit 0%.
  for (int i = 0; i < kNumAdjustmentsToReachLimit; ++i)
    controller_.DecreaseUserBrightness(true /* allow_off */);
  EXPECT_DOUBLE_EQ(0.0, GetBacklightBrightnessPercent());
}

// Test that the BrightnessControllerObserver is notified about changes.
TEST_F(ExternalBacklightControllerTest, NotifyObserver) {
  MockBacklightControllerObserver observer;
  controller_.AddObserver(&observer);

  observer.Clear();
  controller_.DecreaseUserBrightness(true /* allow_off */);
  ASSERT_EQ(1, static_cast<int>(observer.changes().size()));
  EXPECT_DOUBLE_EQ(GetBacklightBrightnessPercent(),
                   observer.changes()[0].percent);
  EXPECT_EQ(BacklightController::BRIGHTNESS_CHANGE_USER_INITIATED,
            observer.changes()[0].cause);

  controller_.RemoveObserver(&observer);
}

TEST_F(ExternalBacklightControllerTest, TurnDisplaysOffWhenShuttingDown) {
  controller_.SetShuttingDown(true);
  EXPECT_EQ(chromeos::DISPLAY_POWER_ALL_OFF, display_power_setter_.state());
  EXPECT_EQ(0, display_power_setter_.delay().InMilliseconds());
}

}  // namespace power_manager
