// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/policy/external_backlight_controller.h"

#include <gtest/gtest.h>

#include "base/compiler_specific.h"
#include "power_manager/powerd/policy/mock_backlight_controller_observer.h"
#include "power_manager/powerd/system/display_power_setter_stub.h"

namespace power_manager {
namespace policy {

class ExternalBacklightControllerTest : public ::testing::Test {
 public:
  ExternalBacklightControllerTest() : controller_(&display_power_setter_) {
    controller_.Init();
  }

 protected:
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

  controller_.SetDimmedForInactivity(true);
  EXPECT_TRUE(display_power_setter_.dimmed());
  EXPECT_EQ(chromeos::DISPLAY_POWER_ALL_ON, display_power_setter_.state());

  controller_.SetOffForInactivity(true);
  EXPECT_TRUE(display_power_setter_.dimmed());
  EXPECT_EQ(chromeos::DISPLAY_POWER_ALL_OFF, display_power_setter_.state());

  controller_.SetSuspended(true);
  EXPECT_TRUE(display_power_setter_.dimmed());
  EXPECT_EQ(chromeos::DISPLAY_POWER_ALL_OFF, display_power_setter_.state());

  controller_.SetSuspended(false);
  controller_.SetOffForInactivity(false);
  controller_.SetDimmedForInactivity(false);
  EXPECT_FALSE(display_power_setter_.dimmed());
  EXPECT_EQ(chromeos::DISPLAY_POWER_ALL_ON, display_power_setter_.state());
}

TEST_F(ExternalBacklightControllerTest, TurnDisplaysOffWhenShuttingDown) {
  controller_.SetShuttingDown(true);
  EXPECT_EQ(chromeos::DISPLAY_POWER_ALL_OFF, display_power_setter_.state());
  EXPECT_EQ(0, display_power_setter_.delay().InMilliseconds());
}

}  // namespace policy
}  // namespace power_manager
