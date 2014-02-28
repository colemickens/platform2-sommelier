// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/policy/external_backlight_controller.h"

#include <gtest/gtest.h>

#include "base/compiler_specific.h"
#include "power_manager/powerd/policy/backlight_controller_observer_stub.h"
#include "power_manager/powerd/system/display/display_power_setter_stub.h"

namespace power_manager {
namespace policy {

class ExternalBacklightControllerTest : public ::testing::Test {
 public:
  ExternalBacklightControllerTest() {
    controller_.AddObserver(&observer_);
    controller_.Init(&display_power_setter_);
  }

  virtual ~ExternalBacklightControllerTest() {
    controller_.RemoveObserver(&observer_);
  }

 protected:
  BacklightControllerObserverStub observer_;
  system::DisplayPowerSetterStub display_power_setter_;
  ExternalBacklightController controller_;
};

TEST_F(ExternalBacklightControllerTest, IgnoreBrightnessRequests) {
  // ExternalBacklightController doesn't support brightness adjustments.
  double percent = 0.0;
  EXPECT_FALSE(controller_.GetBrightnessPercent(&percent));
  EXPECT_FALSE(controller_.SetUserBrightnessPercent(
      50.0, BacklightController::TRANSITION_INSTANT));
  EXPECT_FALSE(controller_.IncreaseUserBrightness());
  EXPECT_FALSE(controller_.DecreaseUserBrightness(true /* allow_off */));
}

TEST_F(ExternalBacklightControllerTest, DimAndTurnOffScreen) {
  EXPECT_FALSE(display_power_setter_.dimmed());
  EXPECT_EQ(chromeos::DISPLAY_POWER_ALL_ON, display_power_setter_.state());

  observer_.Clear();
  controller_.SetDimmedForInactivity(true);
  EXPECT_TRUE(display_power_setter_.dimmed());
  EXPECT_EQ(chromeos::DISPLAY_POWER_ALL_ON, display_power_setter_.state());
  EXPECT_EQ(0, static_cast<int>(observer_.changes().size()));

  observer_.Clear();
  controller_.SetOffForInactivity(true);
  EXPECT_TRUE(display_power_setter_.dimmed());
  EXPECT_EQ(chromeos::DISPLAY_POWER_ALL_OFF, display_power_setter_.state());
  ASSERT_EQ(1, static_cast<int>(observer_.changes().size()));
  EXPECT_DOUBLE_EQ(0.0, observer_.changes()[0].percent);
  EXPECT_EQ(BacklightController::BRIGHTNESS_CHANGE_AUTOMATED,
            observer_.changes()[0].cause);
  EXPECT_EQ(&controller_, observer_.changes()[0].source);

  observer_.Clear();
  controller_.SetSuspended(true);
  EXPECT_TRUE(display_power_setter_.dimmed());
  EXPECT_EQ(chromeos::DISPLAY_POWER_ALL_OFF, display_power_setter_.state());
  EXPECT_EQ(0, static_cast<int>(observer_.changes().size()));

  observer_.Clear();
  controller_.SetSuspended(false);
  controller_.SetOffForInactivity(false);
  controller_.SetDimmedForInactivity(false);
  EXPECT_FALSE(display_power_setter_.dimmed());
  EXPECT_EQ(chromeos::DISPLAY_POWER_ALL_ON, display_power_setter_.state());
  ASSERT_EQ(1, static_cast<int>(observer_.changes().size()));
  EXPECT_DOUBLE_EQ(100.0, observer_.changes()[0].percent);
  EXPECT_EQ(BacklightController::BRIGHTNESS_CHANGE_AUTOMATED,
            observer_.changes()[0].cause);
  EXPECT_EQ(&controller_, observer_.changes()[0].source);
}

TEST_F(ExternalBacklightControllerTest, TurnDisplaysOffWhenShuttingDown) {
  controller_.SetShuttingDown(true);
  EXPECT_EQ(chromeos::DISPLAY_POWER_ALL_OFF, display_power_setter_.state());
  EXPECT_EQ(0, display_power_setter_.delay().InMilliseconds());
}

TEST_F(ExternalBacklightControllerTest, SetDisplayPowerOnChromeStart) {
  // The display power shouldn't be set by Init() (maybe Chrome hasn't started
  // yet).
  EXPECT_EQ(0, display_power_setter_.num_power_calls());
  EXPECT_EQ(0, static_cast<int>(observer_.changes().size()));

  // After Chrome starts, the state should be initialized to sane defaults.
  display_power_setter_.reset_num_power_calls();
  controller_.HandleChromeStart();
  EXPECT_EQ(1, display_power_setter_.num_power_calls());
  EXPECT_FALSE(display_power_setter_.dimmed());
  ASSERT_EQ(chromeos::DISPLAY_POWER_ALL_ON, display_power_setter_.state());
  ASSERT_EQ(1, static_cast<int>(observer_.changes().size()));
  EXPECT_DOUBLE_EQ(100.0, observer_.changes()[0].percent);
  EXPECT_EQ(BacklightController::BRIGHTNESS_CHANGE_AUTOMATED,
            observer_.changes()[0].cause);
  EXPECT_EQ(&controller_, observer_.changes()[0].source);

  controller_.SetDimmedForInactivity(true);
  ASSERT_TRUE(display_power_setter_.dimmed());
  controller_.SetOffForInactivity(true);
  ASSERT_EQ(chromeos::DISPLAY_POWER_ALL_OFF, display_power_setter_.state());

  // Reset the power setter's dimming state so we can check that another dimming
  // request is sent when Chrome restarts.
  display_power_setter_.reset_num_power_calls();
  display_power_setter_.SetDisplaySoftwareDimming(false);
  observer_.Clear();
  controller_.HandleChromeStart();
  EXPECT_EQ(chromeos::DISPLAY_POWER_ALL_OFF, display_power_setter_.state());
  EXPECT_EQ(1, display_power_setter_.num_power_calls());
  EXPECT_TRUE(display_power_setter_.dimmed());
  ASSERT_EQ(1, static_cast<int>(observer_.changes().size()));
  EXPECT_DOUBLE_EQ(0.0, observer_.changes()[0].percent);
  EXPECT_EQ(BacklightController::BRIGHTNESS_CHANGE_AUTOMATED,
            observer_.changes()[0].cause);
  EXPECT_EQ(&controller_, observer_.changes()[0].source);
}

}  // namespace policy
}  // namespace power_manager
