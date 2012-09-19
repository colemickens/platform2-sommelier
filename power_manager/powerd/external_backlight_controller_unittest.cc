// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/external_backlight_controller.h"

#include <gtest/gtest.h>

#include "base/compiler_specific.h"
#include "power_manager/common/backlight_interface.h"
#include "power_manager/powerd/mock_backlight_controller_observer.h"

namespace power_manager {

namespace {

// Default maximum level for TestBacklight.
const int64 kDefaultMaxBacklightLevel = 100;

// Default starting current level for TestBacklight.
const int64 kDefaultStartingBacklightLevel = 100;

// Fake BacklightInterface implementation.
// TODO(derat): Replace MockBacklight with this class.
class TestBacklight : public BacklightInterface {
 public:
  TestBacklight()
      : max_level_(kDefaultMaxBacklightLevel),
        current_level_(kDefaultStartingBacklightLevel),
        should_fail_(false) {}
  virtual ~TestBacklight() {}

  void set_levels(int64 max_level, int64 current_level) {
    max_level_ = max_level;
    current_level_ = current_level;
  }
  void set_should_fail(bool should_fail) { should_fail_ = should_fail; }

  int64 current_level() const { return current_level_; }

  void NotifyObserver() {
    if (observer_)
      observer_->OnBacklightDeviceChanged();
  }

  // BacklightInterface implementation:
  virtual bool GetMaxBrightnessLevel(int64* max_level) OVERRIDE {
    if (should_fail_)
      return false;
    *max_level = max_level_;
    return true;
  }

  virtual bool GetCurrentBrightnessLevel(int64* current_level) OVERRIDE {
    if (should_fail_)
      return false;
    *current_level = current_level_;
    return true;
  }

  virtual bool SetBrightnessLevel(int64 level) OVERRIDE {
    if (should_fail_)
      return false;
    current_level_ = level;
    return true;
  }

 private:
  // Maximum backlight level.
  int64 max_level_;

  // Most-recently-set backlight level.
  int64 current_level_;

  // Should we report failure in response to all future requests?
  bool should_fail_;

  DISALLOW_COPY_AND_ASSIGN(TestBacklight);
};

}  // namespace

class ExternalBacklightControllerTest : public ::testing::Test {
 public:
  ExternalBacklightControllerTest() : controller_(&backlight_) {
    controller_.set_disable_dbus_for_testing(true);
    CHECK(controller_.Init());
  }

 protected:
  int64 GetCurrentBrightnessLevel() {
    double percent = 0.0;
    CHECK(controller_.GetCurrentBrightnessPercent(&percent));
    return controller_.PercentToLevel(percent);
  }

  int64 GetTargetBrightnessLevel() {
    return controller_.PercentToLevel(controller_.GetTargetBrightnessPercent());
  }

  TestBacklight backlight_;
  ExternalBacklightController controller_;
};

// Test that we record the power state.
TEST_F(ExternalBacklightControllerTest, GetPowerState) {
  EXPECT_TRUE(controller_.SetPowerState(BACKLIGHT_ACTIVE));
  EXPECT_EQ(BACKLIGHT_ACTIVE, controller_.GetPowerState());

  EXPECT_TRUE(controller_.SetPowerState(BACKLIGHT_DIM));
  EXPECT_EQ(BACKLIGHT_DIM, controller_.GetPowerState());
}

// Test that backlight failures are reported correctly.
TEST_F(ExternalBacklightControllerTest, FailedBacklightRequest) {
  backlight_.set_should_fail(true);
  double percent = 0;
  EXPECT_FALSE(controller_.GetCurrentBrightnessPercent(&percent));
  EXPECT_FALSE(
      controller_.SetCurrentBrightnessPercent(
          50.0, BRIGHTNESS_CHANGE_USER_INITIATED, TRANSITION_INSTANT));
  EXPECT_FALSE(
      controller_.IncreaseBrightness(BRIGHTNESS_CHANGE_USER_INITIATED));
  EXPECT_FALSE(
      controller_.DecreaseBrightness(false, BRIGHTNESS_CHANGE_USER_INITIATED));
  EXPECT_FALSE(controller_.Init());
}

// Test that we reinitialize the brightness range when notified that the
// backlight device has changed.
TEST_F(ExternalBacklightControllerTest, ReinitializeOnDeviceChange) {
  const int kStartingBacklightLevel = 67;
  backlight_.set_levels(kDefaultMaxBacklightLevel, kStartingBacklightLevel);
  EXPECT_EQ(kStartingBacklightLevel, GetCurrentBrightnessLevel());

  const int kNewMaxBacklightLevel = 60;
  const int kNewBacklightLevel = 45;
  backlight_.set_levels(kNewMaxBacklightLevel, kNewBacklightLevel);
  controller_.OnBacklightDeviceChanged();
  EXPECT_EQ(kNewBacklightLevel, GetCurrentBrightnessLevel());
}

// Test that we dim whenever we're not in the active state.  We should do this
// without changing the monitor's brightness settings.
TEST_F(ExternalBacklightControllerTest, DimScreen) {
  const int kStartingBacklightLevel = 43;
  backlight_.set_levels(kDefaultMaxBacklightLevel, kStartingBacklightLevel);
  EXPECT_FALSE(controller_.currently_dimming());
  EXPECT_EQ(kStartingBacklightLevel, backlight_.current_level());

  EXPECT_TRUE(controller_.SetPowerState(BACKLIGHT_ACTIVE));
  EXPECT_FALSE(controller_.currently_dimming());
  EXPECT_EQ(kStartingBacklightLevel, backlight_.current_level());

  EXPECT_TRUE(controller_.SetPowerState(BACKLIGHT_DIM));
  EXPECT_TRUE(controller_.currently_dimming());
  EXPECT_EQ(kStartingBacklightLevel, backlight_.current_level());

  EXPECT_TRUE(controller_.SetPowerState(BACKLIGHT_ACTIVE));
  EXPECT_FALSE(controller_.currently_dimming());
  EXPECT_EQ(kStartingBacklightLevel, backlight_.current_level());

  EXPECT_TRUE(controller_.SetPowerState(BACKLIGHT_IDLE_OFF));
  EXPECT_TRUE(controller_.currently_dimming());
  EXPECT_EQ(kStartingBacklightLevel, backlight_.current_level());

  EXPECT_TRUE(controller_.SetPowerState(BACKLIGHT_SUSPENDED));
  EXPECT_TRUE(controller_.currently_dimming());
  EXPECT_EQ(kStartingBacklightLevel, backlight_.current_level());

  EXPECT_TRUE(controller_.SetPowerState(BACKLIGHT_ACTIVE));
  EXPECT_FALSE(controller_.currently_dimming());
  EXPECT_EQ(kStartingBacklightLevel, backlight_.current_level());
}

// Test that we turn the screen off when requested to do so and when the machine
// is suspended.  We should do this without changing the monitor's brightness
// settings.
TEST_F(ExternalBacklightControllerTest, TurnScreenOff) {
  const int kStartingBacklightLevel = 65;
  backlight_.set_levels(kDefaultMaxBacklightLevel, kStartingBacklightLevel);
  EXPECT_FALSE(controller_.currently_off());
  EXPECT_EQ(kStartingBacklightLevel, backlight_.current_level());

  EXPECT_TRUE(controller_.SetPowerState(BACKLIGHT_ACTIVE));
  EXPECT_FALSE(controller_.currently_off());
  EXPECT_EQ(kStartingBacklightLevel, backlight_.current_level());

  EXPECT_TRUE(controller_.SetPowerState(BACKLIGHT_DIM));
  EXPECT_FALSE(controller_.currently_off());
  EXPECT_EQ(kStartingBacklightLevel, backlight_.current_level());

  EXPECT_TRUE(controller_.SetPowerState(BACKLIGHT_IDLE_OFF));
  EXPECT_TRUE(controller_.currently_off());
  EXPECT_EQ(kStartingBacklightLevel, backlight_.current_level());

  EXPECT_TRUE(controller_.SetPowerState(BACKLIGHT_SUSPENDED));
  EXPECT_TRUE(controller_.currently_off());
  EXPECT_EQ(kStartingBacklightLevel, backlight_.current_level());

  EXPECT_TRUE(controller_.SetPowerState(BACKLIGHT_ACTIVE));
  EXPECT_FALSE(controller_.currently_off());
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
    controller_.IncreaseBrightness(BRIGHTNESS_CHANGE_USER_INITIATED);
  for (int i = 0; i < kNumUserDownAdjustments; ++i)
    controller_.DecreaseBrightness(false, BRIGHTNESS_CHANGE_USER_INITIATED);
  for (int i = 0; i < kNumUserAbsoluteAdjustments; ++i) {
    controller_.SetCurrentBrightnessPercent(
        50.0, BRIGHTNESS_CHANGE_USER_INITIATED, TRANSITION_INSTANT);
  }

  // Throw in some random automated adjustments.
  controller_.IncreaseBrightness(BRIGHTNESS_CHANGE_AUTOMATED);
  controller_.DecreaseBrightness(false, BRIGHTNESS_CHANGE_AUTOMATED);
  controller_.SetCurrentBrightnessPercent(
      50.0, BRIGHTNESS_CHANGE_AUTOMATED, TRANSITION_INSTANT);

  EXPECT_EQ(kTotalUserAdjustments, controller_.GetNumUserAdjustments());

  // The external controller ignores ALS readings.
  const int kNumAmbientLightSensorReadings = 20;
  for (int i = 0; i < kNumAmbientLightSensorReadings; ++i)
    controller_.OnAmbientLightChanged(NULL);
  EXPECT_EQ(0, controller_.GetNumAmbientLightSensorAdjustments());
}

// Test that for both the current and target brightness, we just return the
// brightness as reported by the display.
TEST_F(ExternalBacklightControllerTest, QueryBrightness) {
  EXPECT_EQ(kDefaultStartingBacklightLevel, GetCurrentBrightnessLevel());
  EXPECT_EQ(kDefaultStartingBacklightLevel, GetTargetBrightnessLevel());

  const int kNewLevel = kDefaultMaxBacklightLevel / 2;
  backlight_.set_levels(kDefaultMaxBacklightLevel, kNewLevel);
  EXPECT_EQ(kNewLevel, GetCurrentBrightnessLevel());
  EXPECT_EQ(kNewLevel, GetTargetBrightnessLevel());
}

// Test that requests to change the brightness are honored.
TEST_F(ExternalBacklightControllerTest, ChangeBrightness) {
  const int kNumAdjustmentsToReachLimit = 20;

  // Test setting the brightness to an absolute level.
  const double kNewPercent = 75.0;
  controller_.SetCurrentBrightnessPercent(
      kNewPercent, BRIGHTNESS_CHANGE_USER_INITIATED, TRANSITION_INSTANT);
  EXPECT_DOUBLE_EQ(kNewPercent, controller_.GetTargetBrightnessPercent());
  EXPECT_EQ(GetTargetBrightnessLevel(), backlight_.current_level());

  // Increase enough times to hit 100%.
  for (int i = 0; i < kNumAdjustmentsToReachLimit; ++i)
    controller_.IncreaseBrightness(BRIGHTNESS_CHANGE_USER_INITIATED);
  EXPECT_DOUBLE_EQ(100.0, controller_.GetTargetBrightnessPercent());
  EXPECT_EQ(kDefaultMaxBacklightLevel, backlight_.current_level());

  // Decrease enough times to hit 0%.
  for (int i = 0; i < kNumAdjustmentsToReachLimit; ++i)
    controller_.DecreaseBrightness(true, BRIGHTNESS_CHANGE_USER_INITIATED);
  EXPECT_DOUBLE_EQ(0.0, controller_.GetTargetBrightnessPercent());
  EXPECT_EQ(0, backlight_.current_level());
}

// Test that the BrightnessControllerObserver is notified about changes.
TEST_F(ExternalBacklightControllerTest, NotifyObserver) {
  MockBacklightControllerObserver observer;
  controller_.SetObserver(&observer);

  observer.Clear();
  controller_.IncreaseBrightness(BRIGHTNESS_CHANGE_AUTOMATED);
  ASSERT_EQ(1, static_cast<int>(observer.changes().size()));
  EXPECT_DOUBLE_EQ(controller_.GetTargetBrightnessPercent(),
                   observer.changes()[0].percent);
  EXPECT_EQ(BRIGHTNESS_CHANGE_AUTOMATED, observer.changes()[0].cause);

  observer.Clear();
  controller_.DecreaseBrightness(true, BRIGHTNESS_CHANGE_USER_INITIATED);
  ASSERT_EQ(1, static_cast<int>(observer.changes().size()));
  EXPECT_DOUBLE_EQ(controller_.GetTargetBrightnessPercent(),
                   observer.changes()[0].percent);
  EXPECT_EQ(BRIGHTNESS_CHANGE_USER_INITIATED, observer.changes()[0].cause);
}

}  // namespace power_manager
