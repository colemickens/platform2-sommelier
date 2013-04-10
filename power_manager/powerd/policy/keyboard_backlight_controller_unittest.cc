// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/policy/keyboard_backlight_controller.h"

#include <gtest/gtest.h>

#include <string>

#include "base/memory/scoped_ptr.h"
#include "power_manager/common/fake_prefs.h"
#include "power_manager/common/power_constants.h"
#include "power_manager/common/util.h"
#include "power_manager/powerd/policy/backlight_controller.h"
#include "power_manager/powerd/policy/mock_backlight_controller_observer.h"
#include "power_manager/powerd/system/ambient_light_sensor_stub.h"
#include "power_manager/powerd/system/backlight_stub.h"

namespace power_manager {
namespace policy {

class KeyboardBacklightControllerTest : public ::testing::Test {
 public:
  KeyboardBacklightControllerTest()
      : max_backlight_level_(100),
        initial_backlight_level_(50),
        initial_als_percent_(0.0),
        initial_als_lux_(0),
        als_limits_pref_("0.0\n20.0\n75.0"),
        als_steps_pref_("20.0 -1 50\n50.0 35 75\n75.0 60 -1"),
        user_limits_pref_("0.0\n10.0\n100.0"),
        user_steps_pref_("0.0\n10.0\n40.0\n60.0\n100.0"),
        backlight_(max_backlight_level_, initial_backlight_level_),
        light_sensor_(initial_als_percent_, initial_als_lux_),
        controller_(&backlight_, &prefs_, &light_sensor_),
        test_api_(&controller_) {
  }

  virtual void SetUp() {
    controller_.AddObserver(&observer_);
  }

  virtual void TearDown() {
    controller_.RemoveObserver(&observer_);
  }

  // Initializes |controller_|.
  virtual bool Init() {
    backlight_.set_max_level(max_backlight_level_);
    backlight_.set_current_level(initial_backlight_level_);
    light_sensor_.set_values(initial_als_percent_, initial_als_lux_);

    prefs_.SetString(kKeyboardBacklightAlsLimitsPref, als_limits_pref_);
    prefs_.SetString(kKeyboardBacklightAlsStepsPref, als_steps_pref_);
    prefs_.SetString(kKeyboardBacklightUserLimitsPref, user_limits_pref_);
    prefs_.SetString(kKeyboardBacklightUserStepsPref, user_steps_pref_);
    prefs_.SetInt64(kDisableALSPref, 0);

    return controller_.Init();
  }

 protected:
  // Max and initial brightness levels for |backlight_|.
  int64 max_backlight_level_;
  int64 initial_backlight_level_;

  // Initial percent and lux levels reported by |light_sensor_|.
  double initial_als_percent_;
  int initial_als_lux_;

  // Values for various preferences.  These can be changed by tests before
  // Init() is called.
  std::string als_limits_pref_;
  std::string als_steps_pref_;
  std::string user_limits_pref_;
  std::string user_steps_pref_;

  FakePrefs prefs_;
  system::BacklightStub backlight_;
  system::AmbientLightSensorStub light_sensor_;
  MockBacklightControllerObserver observer_;
  KeyboardBacklightController controller_;
  KeyboardBacklightController::TestApi test_api_;
};

TEST_F(KeyboardBacklightControllerTest, InitFails) {
  backlight_.set_should_fail(true);
  EXPECT_FALSE(Init());
}

TEST_F(KeyboardBacklightControllerTest, GetBrightnessPercent) {
  ASSERT_TRUE(Init());

  // GetBrightnessPercent() should initially return the backlight's
  // starting level.  (It's safe to compare levels and percents since we're
  // using a [0, 100] range to make things simpler.)
  double percent = 0.0;
  EXPECT_TRUE(controller_.GetBrightnessPercent(&percent));
  EXPECT_DOUBLE_EQ(static_cast<double>(initial_backlight_level_), percent);

  // After increasing the brightness, the new level should be returned.
  EXPECT_TRUE(controller_.IncreaseUserBrightness());
  EXPECT_TRUE(controller_.GetBrightnessPercent(&percent));
  EXPECT_DOUBLE_EQ(static_cast<double>(backlight_.current_level()), percent);
}

TEST_F(KeyboardBacklightControllerTest, DimForFullscreenVideo) {
  als_limits_pref_ = "0.0\n20.0\n75.0";
  als_steps_pref_ = "20.0 -1 50\n50.0 35 75\n75.0 60 -1";
  ASSERT_TRUE(Init());
  controller_.HandleSessionStateChange(SESSION_STARTED);
  light_sensor_.NotifyObservers();
  ASSERT_EQ(20, backlight_.current_level());

  // Non-fullscreen video shouldn't turn off the backlight.
  controller_.HandleVideoActivity(false);
  EXPECT_EQ(20, backlight_.current_level());

  // Fullscreen video should turn it off.
  controller_.HandleVideoActivity(true);
  EXPECT_EQ(0, backlight_.current_level());
  EXPECT_EQ(kSlowBacklightTransitionMs,
            backlight_.current_interval().InMilliseconds());

  // If the video switches to non-fullscreen, the backlight should be turned on.
  controller_.HandleVideoActivity(false);
  EXPECT_EQ(20, backlight_.current_level());
  EXPECT_EQ(kSlowBacklightTransitionMs,
            backlight_.current_interval().InMilliseconds());

  // Let fullscreen video turn it off again.
  controller_.HandleVideoActivity(true);
  EXPECT_EQ(0, backlight_.current_level());
  EXPECT_EQ(kSlowBacklightTransitionMs,
            backlight_.current_interval().InMilliseconds());

  // If the timeout fires to indicate that video has stopped, the backlight
  // should be turned on.
  test_api_.TriggerVideoTimeout();
  EXPECT_EQ(20, backlight_.current_level());
  EXPECT_EQ(kSlowBacklightTransitionMs,
            backlight_.current_interval().InMilliseconds());

  // Fullscreen video should be ignored when the user isn't logged in.
  controller_.HandleSessionStateChange(SESSION_STOPPED);
  controller_.HandleVideoActivity(true);
  EXPECT_EQ(20, backlight_.current_level());
}

TEST_F(KeyboardBacklightControllerTest, OnAmbientLightChanged) {
  initial_backlight_level_ = 20;
  als_limits_pref_ = "0.0\n20.0\n75.0";
  als_steps_pref_ = "20.0 -1 50\n50.0 35 75\n75.0 60 -1";
  ASSERT_TRUE(Init());
  ASSERT_EQ(20, backlight_.current_level());

  // ALS returns bad value.
  light_sensor_.set_values(0.0, -1);
  light_sensor_.NotifyObservers();
  EXPECT_EQ(20, backlight_.current_level());

  // ALS returns a value in the middle of the initial step.
  light_sensor_.set_values(0.0, 25);
  light_sensor_.NotifyObservers();
  EXPECT_EQ(20, backlight_.current_level());

  // First increase; hysteresis not overcome.
  light_sensor_.set_values(0.0, 80);
  light_sensor_.NotifyObservers();
  EXPECT_EQ(20, backlight_.current_level());

  // Second increase; hysteresis overcome.  The lux is high enough that the
  // controller should skip over the middle step and use the top step.
  light_sensor_.NotifyObservers();
  EXPECT_EQ(75, backlight_.current_level());
  EXPECT_EQ(kSlowBacklightTransitionMs,
            backlight_.current_interval().InMilliseconds());

  // First decrease; hysteresis not overcome.
  light_sensor_.set_values(0.0, 50);
  light_sensor_.NotifyObservers();
  EXPECT_EQ(75, backlight_.current_level());

  // Second decrease; hysteresis overcome.  The controller should decrease
  // to the middle step.
  light_sensor_.NotifyObservers();
  EXPECT_EQ(50, backlight_.current_level());
  EXPECT_EQ(kSlowBacklightTransitionMs,
            backlight_.current_interval().InMilliseconds());
}

TEST_F(KeyboardBacklightControllerTest, TwoValueLimitsPref) {
  // We should use reasonable limits if the user limits pref only has two
  // values instead of the expected three.
  user_limits_pref_ = "0.0\n50.0";
  ASSERT_TRUE(Init());
  controller_.IncreaseUserBrightness();
  controller_.IncreaseUserBrightness();
  controller_.IncreaseUserBrightness();
  EXPECT_EQ(100, backlight_.current_level());
}

TEST_F(KeyboardBacklightControllerTest, UnparseableLimitsPref) {
  // We should use reasonable limits if the user limits pref contains a
  // non-numeric value.
  user_limits_pref_ = "0.0\n50.0\nfoo";
  ASSERT_TRUE(Init());
  controller_.IncreaseUserBrightness();
  controller_.IncreaseUserBrightness();
  controller_.IncreaseUserBrightness();
  EXPECT_EQ(100, backlight_.current_level());
}

TEST_F(KeyboardBacklightControllerTest, SkipBogusUserStep) {
  user_steps_pref_ = "0.0\n10.0\nfoo\n60.0\n100.0";
  initial_backlight_level_ = 0;
  ASSERT_TRUE(Init());
  ASSERT_EQ(0, backlight_.current_level());

  // The invalid value should be skipped over.
  EXPECT_TRUE(controller_.IncreaseUserBrightness());
  EXPECT_EQ(10, backlight_.current_level());
  EXPECT_TRUE(controller_.IncreaseUserBrightness());
  EXPECT_EQ(60, backlight_.current_level());
  EXPECT_TRUE(controller_.IncreaseUserBrightness());
  EXPECT_EQ(100, backlight_.current_level());
}

TEST_F(KeyboardBacklightControllerTest, EmptyUserStepsPref) {
  // If the user steps pref file is empty, the controller should use steps
  // representing the minimum, dimmed, and maximum levels.
  user_steps_pref_ = "";
  user_limits_pref_ = "15.0\n63.0\n87.0";
  initial_backlight_level_ = 0;
  ASSERT_TRUE(Init());
  ASSERT_EQ(0, backlight_.current_level());

  EXPECT_TRUE(controller_.IncreaseUserBrightness());
  EXPECT_EQ(63, backlight_.current_level());
  EXPECT_TRUE(controller_.IncreaseUserBrightness());
  EXPECT_EQ(87, backlight_.current_level());
}

TEST_F(KeyboardBacklightControllerTest, SkipBogusAlsStep) {
  // Set a pref with only two values in the middle step instead of three.
  als_steps_pref_ = "20.0 -1 50\n50.0 75\n75.0 60 -1";
  initial_backlight_level_ = 20;
  ASSERT_TRUE(Init());

  // After two readings above the bottom step's increase threshold, the
  // backlight should go to the top step.
  light_sensor_.set_values(0.0, 55);
  light_sensor_.NotifyObservers();
  light_sensor_.NotifyObservers();
  EXPECT_EQ(75, backlight_.current_level());
}

TEST_F(KeyboardBacklightControllerTest, EmptyAlsStepsPref) {
  als_steps_pref_ = "";
  als_limits_pref_ = "20.0\n50.0\n90.0";
  initial_backlight_level_ = 0;
  ASSERT_TRUE(Init());

  light_sensor_.set_values(0.0, 20);
  light_sensor_.NotifyObservers();
  EXPECT_EQ(90, backlight_.current_level());
}

TEST_F(KeyboardBacklightControllerTest, ChangeStates) {
  // Configure a single step for ALS and three steps for user control.
  als_limits_pref_ = "0.0\n20.0\n100.0";
  als_steps_pref_ = "50.0 -1 -1";
  user_limits_pref_ = "0.0\n10.0\n100.0";
  user_steps_pref_ = "0.0\n60.0\n100.0";
  initial_backlight_level_ = 50;
  ASSERT_TRUE(Init());
  light_sensor_.NotifyObservers();
  ASSERT_EQ(50, backlight_.current_level());

  // Requests to dim the backlight and turn it off should be honored, as
  // should changes to turn it back on and undim.
  controller_.SetDimmedForInactivity(true);
  EXPECT_EQ(20, backlight_.current_level());
  EXPECT_EQ(kSlowBacklightTransitionMs,
            backlight_.current_interval().InMilliseconds());
  controller_.SetOffForInactivity(true);
  EXPECT_EQ(0, backlight_.current_level());
  EXPECT_EQ(kSlowBacklightTransitionMs,
            backlight_.current_interval().InMilliseconds());
  controller_.SetOffForInactivity(false);
  EXPECT_EQ(20, backlight_.current_level());
  EXPECT_EQ(kSlowBacklightTransitionMs,
            backlight_.current_interval().InMilliseconds());
  controller_.SetDimmedForInactivity(false);
  EXPECT_EQ(50, backlight_.current_level());
  EXPECT_EQ(kSlowBacklightTransitionMs,
            backlight_.current_interval().InMilliseconds());

  // Send an increase request to switch to user control.
  controller_.IncreaseUserBrightness();
  EXPECT_EQ(100, backlight_.current_level());
  EXPECT_EQ(kFastBacklightTransitionMs,
            backlight_.current_interval().InMilliseconds());

  // Go through the same sequence of state changes and check that the
  // user-control dimming level is used.
  controller_.SetDimmedForInactivity(true);
  EXPECT_EQ(10, backlight_.current_level());
  EXPECT_EQ(kSlowBacklightTransitionMs,
            backlight_.current_interval().InMilliseconds());
  controller_.SetOffForInactivity(true);
  EXPECT_EQ(0, backlight_.current_level());
  EXPECT_EQ(kSlowBacklightTransitionMs,
            backlight_.current_interval().InMilliseconds());
  controller_.SetOffForInactivity(false);
  EXPECT_EQ(10, backlight_.current_level());
  EXPECT_EQ(kSlowBacklightTransitionMs,
            backlight_.current_interval().InMilliseconds());
  controller_.SetDimmedForInactivity(false);
  EXPECT_EQ(100, backlight_.current_level());
  EXPECT_EQ(kSlowBacklightTransitionMs,
            backlight_.current_interval().InMilliseconds());
}

TEST_F(KeyboardBacklightControllerTest, DontBrightenToDim) {
  // Set the bottom ALS step to 20%, but request dimming to 30%.
  als_limits_pref_ = "0.0\n30.0\n100.0";
  als_steps_pref_ = "20.0 -1 60\n80.0 40 -1";
  initial_als_lux_ = 20;
  ASSERT_TRUE(Init());

  light_sensor_.NotifyObservers();
  ASSERT_EQ(20, backlight_.current_level());

  // The controller should never increase the brightness level when dimming.
  controller_.SetDimmedForInactivity(true);
  EXPECT_EQ(20, backlight_.current_level());
}

TEST_F(KeyboardBacklightControllerTest, DeferChangesWhileDimmed) {
  als_limits_pref_ = "0.0\n10.0\n100.0";
  als_steps_pref_ = "20.0 -1 60\n80.0 40 -1";
  initial_als_lux_ = 20;
  ASSERT_TRUE(Init());

  light_sensor_.NotifyObservers();
  ASSERT_EQ(20, backlight_.current_level());

  controller_.SetDimmedForInactivity(true);
  EXPECT_EQ(10, backlight_.current_level());

  // ALS-driven changes shouldn't be applied while the screen is dimmed.
  light_sensor_.set_values(80.0, 80);
  light_sensor_.NotifyObservers();
  light_sensor_.NotifyObservers();
  EXPECT_EQ(10, backlight_.current_level());

  // The new ALS level should be used immediately after undimming, though.
  controller_.SetDimmedForInactivity(false);
  EXPECT_EQ(80, backlight_.current_level());
  EXPECT_EQ(kSlowBacklightTransitionMs,
            backlight_.current_interval().InMilliseconds());
}

TEST_F(KeyboardBacklightControllerTest, InitialUserLevel) {
  // Set user steps at 0, 10, 40, 60, and 100.  The backlight should remain
  // at its starting level when Init() is called.
  user_limits_pref_ = "0.0\n10.0\n100.0";
  user_steps_pref_ = "0.0\n10.0\n40.0\n60.0\n100.0";
  initial_backlight_level_ = 15;
  ASSERT_TRUE(Init());
  EXPECT_EQ(15, backlight_.current_level());

  // After an increase request switches to user control of the brightness
  // level, the controller should first choose the step (10) nearest to the
  // initial level (15) and then increase to the next step (40).
  EXPECT_TRUE(controller_.IncreaseUserBrightness());
  EXPECT_EQ(40, backlight_.current_level());
  EXPECT_EQ(kFastBacklightTransitionMs,
            backlight_.current_interval().InMilliseconds());
}

TEST_F(KeyboardBacklightControllerTest, InitialAlsLevel) {
  // Set an initial backlight level that's closest to the 60% step and
  // within its lux range of [50, 90].
  als_limits_pref_ = "0.0\n10.0\n100.0";
  als_steps_pref_ = "0.0 -1 30\n30.0 20 60\n60.0 50 90\n100.0 80 -1";
  initial_backlight_level_ = 55;
  initial_als_lux_ = 85;
  ASSERT_TRUE(Init());
  EXPECT_EQ(55, backlight_.current_level());

  // After an ambient light reading, the controller should slowly
  // transition to the 60% level.
  light_sensor_.NotifyObservers();
  EXPECT_EQ(60, backlight_.current_level());
  EXPECT_EQ(kSlowBacklightTransitionMs,
            backlight_.current_interval().InMilliseconds());
}

TEST_F(KeyboardBacklightControllerTest, IncreaseUserBrightness) {
  user_limits_pref_ = "0.0\n10.0\n100.0";
  user_steps_pref_ = "0.0\n10.0\n40.0\n60.0\n100.0";
  initial_backlight_level_ = 0;
  ASSERT_TRUE(Init());

  EXPECT_EQ(0, backlight_.current_level());

  EXPECT_TRUE(controller_.IncreaseUserBrightness());
  EXPECT_EQ(10, backlight_.current_level());
  EXPECT_EQ(kFastBacklightTransitionMs,
            backlight_.current_interval().InMilliseconds());
  EXPECT_EQ(1, controller_.GetNumUserAdjustments());

  EXPECT_TRUE(controller_.IncreaseUserBrightness());
  EXPECT_EQ(40, backlight_.current_level());
  EXPECT_EQ(2, controller_.GetNumUserAdjustments());

  EXPECT_TRUE(controller_.IncreaseUserBrightness());
  EXPECT_EQ(60, backlight_.current_level());
  EXPECT_EQ(3, controller_.GetNumUserAdjustments());

  EXPECT_TRUE(controller_.IncreaseUserBrightness());
  EXPECT_EQ(100, backlight_.current_level());
  EXPECT_EQ(4, controller_.GetNumUserAdjustments());

  EXPECT_FALSE(controller_.IncreaseUserBrightness());
  EXPECT_EQ(100, backlight_.current_level());
  EXPECT_EQ(5, controller_.GetNumUserAdjustments());
}

TEST_F(KeyboardBacklightControllerTest, DecreaseUserBrightness) {
  user_limits_pref_ = "0.0\n10.0\n100.0";
  user_steps_pref_ = "0.0\n10.0\n40.0\n60.0\n100.0";
  initial_backlight_level_ = 100;
  ASSERT_TRUE(Init());

  EXPECT_EQ(100, backlight_.current_level());

  EXPECT_TRUE(controller_.DecreaseUserBrightness(true /* allow_off */));
  EXPECT_EQ(60, backlight_.current_level());
  EXPECT_EQ(kFastBacklightTransitionMs,
            backlight_.current_interval().InMilliseconds());
  EXPECT_EQ(1, controller_.GetNumUserAdjustments());

  EXPECT_TRUE(controller_.DecreaseUserBrightness(true /* allow_off */));
  EXPECT_EQ(40, backlight_.current_level());
  EXPECT_EQ(2, controller_.GetNumUserAdjustments());

  EXPECT_TRUE(controller_.DecreaseUserBrightness(true /* allow_off */));
  EXPECT_EQ(10, backlight_.current_level());
  EXPECT_EQ(3, controller_.GetNumUserAdjustments());

  EXPECT_TRUE(controller_.DecreaseUserBrightness(true /* allow_off */));
  EXPECT_EQ(0, backlight_.current_level());
  EXPECT_EQ(4, controller_.GetNumUserAdjustments());

  EXPECT_FALSE(controller_.DecreaseUserBrightness(true /* allow_off */));
  EXPECT_EQ(0, backlight_.current_level());
  EXPECT_EQ(5, controller_.GetNumUserAdjustments());
}

TEST_F(KeyboardBacklightControllerTest, TurnOffWhenShuttingDown) {
  ASSERT_TRUE(Init());
  controller_.SetShuttingDown(true);
  EXPECT_EQ(0, backlight_.current_level());
  EXPECT_EQ(0, backlight_.current_interval().InMilliseconds());
}

TEST_F(KeyboardBacklightControllerTest, TurnOffWhenDocked) {
  ASSERT_TRUE(Init());
  controller_.SetDocked(true);
  EXPECT_EQ(0, backlight_.current_level());
  EXPECT_EQ(0, backlight_.current_interval().InMilliseconds());

  // User requests to increase the brightness shouldn't turn the backlight on.
  EXPECT_FALSE(controller_.IncreaseUserBrightness());
  EXPECT_EQ(0, backlight_.current_level());
}

}  // namespace policy
}  // namespace power_manager
