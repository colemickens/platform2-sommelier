// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "power_manager/common/fake_prefs.h"
#include "power_manager/common/power_constants.h"
#include "power_manager/powerd/internal_backlight_controller.h"
#include "power_manager/powerd/mock_backlight_controller_observer.h"
#include "power_manager/powerd/system/ambient_light_sensor_stub.h"
#include "power_manager/powerd/system/backlight_stub.h"
#include "power_manager/powerd/system/display_power_setter_stub.h"

namespace power_manager {

namespace {

// Number of ambient light sensor samples that should be supplied in order to
// trigger an update to InternalBacklightController's ALS offset.
const int kAlsSamplesToTriggerAdjustment = 2;

}  // namespace

class InternalBacklightControllerTest : public ::testing::Test {
 public:
  InternalBacklightControllerTest()
      : max_backlight_level_(1024),
        initial_backlight_level_(512),
        pass_light_sensor_(true),
        initial_als_percent_(0.0),
        initial_als_lux_(0),
        report_initial_als_reading_(true),
        report_initial_power_source_(true),
        default_plugged_offset_(70.0),
        default_unplugged_offset_(30.0),
        default_min_visible_level_(1),
        backlight_(max_backlight_level_, initial_backlight_level_),
        light_sensor_(initial_als_percent_, initial_als_lux_) {
  }

  // Initializes |controller_| and sends it power source and ambient light
  // event such that it should make its first adjustment to the backlight
  // brightness.
  virtual void Init(PowerSource power_source) {
    prefs_.SetDouble(kPluggedBrightnessOffsetPref, default_plugged_offset_);
    prefs_.SetDouble(kUnpluggedBrightnessOffsetPref, default_unplugged_offset_);
    prefs_.SetInt64(kMinVisibleBacklightLevelPref, default_min_visible_level_);
    backlight_.set_max_level(max_backlight_level_);
    backlight_.set_current_level(initial_backlight_level_);
    light_sensor_.set_values(initial_als_percent_, initial_als_lux_);

    controller_.reset(new InternalBacklightController(
        &backlight_, &prefs_, pass_light_sensor_ ? &light_sensor_ : NULL,
        &display_power_setter_));
    CHECK(controller_->Init());
    if (report_initial_power_source_)
      controller_->HandlePowerSourceChange(power_source);
    if (pass_light_sensor_ && report_initial_als_reading_)
      light_sensor_.NotifyObservers();
  }

  // Map |percent| to a |controller_|-designated level in the range [0,
  // max_backlight_level_].
  int64 PercentToLevel(double percent) {
    return controller_->PercentToLevel(percent);
  }

 protected:
  // Max and initial brightness levels for |backlight_|.
  int64 max_backlight_level_;
  int64 initial_backlight_level_;

  // Should Init() pass |light_sensor_| to |controller_|?
  bool pass_light_sensor_;

  // Initial percent and lux levels reported by |light_sensor_|.
  double initial_als_percent_;
  int initial_als_lux_;

  // Should Init() tell |controller_| about an initial ambient light
  // reading and power source change?
  bool report_initial_als_reading_;
  bool report_initial_power_source_;

  // Default values for prefs.  Applied when Init() is called.
  double default_plugged_offset_;
  double default_unplugged_offset_;
  int64 default_min_visible_level_;

  FakePrefs prefs_;
  system::BacklightStub backlight_;
  system::AmbientLightSensorStub light_sensor_;
  system::DisplayPowerSetterStub display_power_setter_;
  scoped_ptr<InternalBacklightController> controller_;
};

TEST_F(InternalBacklightControllerTest, IncreaseAndDecreaseBrightness) {
  default_min_visible_level_ = 100;
  Init(POWER_BATTERY);
  EXPECT_EQ(default_min_visible_level_,
            PercentToLevel(InternalBacklightController::kMinVisiblePercent));
  const int64 kPrefLevel = PercentToLevel(default_unplugged_offset_);
  EXPECT_EQ(kPrefLevel, backlight_.current_level());

  // Check that the first step increases the brightness; within the loop
  // we'll just ensure that the brightness never decreases.
  controller_->IncreaseUserBrightness();
  EXPECT_GT(backlight_.current_level(), kPrefLevel);
  for (int i = 0; i < InternalBacklightController::kMaxBrightnessSteps; ++i) {
    int64 old_level = backlight_.current_level();
    controller_->IncreaseUserBrightness();
    EXPECT_GE(backlight_.current_level(), old_level);
  }
  EXPECT_EQ(max_backlight_level_, backlight_.current_level());

  // Now do the same checks in the opposite direction.  The controller
  // should stop at the minimum visible level if |allow_off| is false.
  controller_->DecreaseUserBrightness(false /* allow_off */);
  EXPECT_LT(backlight_.current_level(), max_backlight_level_);
  for (int i = 0; i < InternalBacklightController::kMaxBrightnessSteps; ++i) {
    int64 old_level = backlight_.current_level();
    controller_->DecreaseUserBrightness(false /* allow_off */);
    EXPECT_LE(backlight_.current_level(), old_level);
  }
  EXPECT_EQ(default_min_visible_level_, backlight_.current_level());

  // One more request with |allow_off| should go to 0.
  controller_->DecreaseUserBrightness(true /* allow_off */);
  EXPECT_EQ(0, backlight_.current_level());

  // One increase request should raise the brightness to the minimum
  // visible level, while a second one should increase it above that.
  controller_->IncreaseUserBrightness();
  EXPECT_EQ(default_min_visible_level_, backlight_.current_level());
  controller_->IncreaseUserBrightness();
  EXPECT_GT(backlight_.current_level(), default_min_visible_level_);
}

// Test that InternalBacklightController notifies its observer in response to
// brightness changes.
TEST_F(InternalBacklightControllerTest, NotifyObserver) {
  Init(POWER_BATTERY);

  MockBacklightControllerObserver observer;
  controller_->AddObserver(&observer);

  // Increase the brightness and check that the observer is notified.
  controller_->IncreaseUserBrightness();
  ASSERT_EQ(1, static_cast<int>(observer.changes().size()));
  EXPECT_EQ(backlight_.current_level(),
            PercentToLevel(observer.changes()[0].percent));
  EXPECT_EQ(BacklightController::BRIGHTNESS_CHANGE_USER_INITIATED,
            observer.changes()[0].cause);

  // Decrease the brightness.
  observer.Clear();
  controller_->DecreaseUserBrightness(true /* allow_off */);
  ASSERT_EQ(1, static_cast<int>(observer.changes().size()));
  EXPECT_EQ(backlight_.current_level(),
            PercentToLevel(observer.changes()[0].percent));
  EXPECT_EQ(BacklightController::BRIGHTNESS_CHANGE_USER_INITIATED,
            observer.changes()[0].cause);

  // Send enough ambient light sensor samples to trigger a brightness change.
  observer.Clear();
  int64 old_level = backlight_.current_level();
  light_sensor_.set_values(32.0, 32);
  for (int i = 0; i < kAlsSamplesToTriggerAdjustment; ++i)
    light_sensor_.NotifyObservers();
  ASSERT_NE(old_level, backlight_.current_level());
  ASSERT_EQ(1, static_cast<int>(observer.changes().size()));
  EXPECT_EQ(backlight_.current_level(),
            PercentToLevel(observer.changes()[0].percent));
  EXPECT_EQ(BacklightController::BRIGHTNESS_CHANGE_AUTOMATED,
            observer.changes()[0].cause);

  // Plug the device in.
  observer.Clear();
  controller_->HandlePowerSourceChange(POWER_AC);
  ASSERT_EQ(1, static_cast<int>(observer.changes().size()));
  EXPECT_EQ(backlight_.current_level(),
            PercentToLevel(observer.changes()[0].percent));
  EXPECT_EQ(BacklightController::BRIGHTNESS_CHANGE_AUTOMATED,
            observer.changes()[0].cause);

  // Dim the backlight.
  observer.Clear();
  controller_->SetDimmedForInactivity(true);
  ASSERT_EQ(1, static_cast<int>(observer.changes().size()));
  EXPECT_EQ(backlight_.current_level(),
            PercentToLevel(observer.changes()[0].percent));
  EXPECT_EQ(BacklightController::BRIGHTNESS_CHANGE_AUTOMATED,
            observer.changes()[0].cause);
}

// Test the case where the minimum visible backlight level matches the maximum
// level exposed by hardware.
TEST_F(InternalBacklightControllerTest, MinBrightnessLevelMatchesMax) {
  default_min_visible_level_ = max_backlight_level_;
  Init(POWER_AC);

  // Decrease the brightness with allow_off=false.
  controller_->DecreaseUserBrightness(false /* allow_off */);
  EXPECT_EQ(default_min_visible_level_, backlight_.current_level());

  // Decrease again with allow_off=true.
  controller_->DecreaseUserBrightness(true /* allow_off */);
  EXPECT_EQ(0, backlight_.current_level());
}

// Test the saved brightness level before and after suspend.
TEST_F(InternalBacklightControllerTest, SuspendBrightnessLevel) {
  Init(POWER_AC);
  const int64 kDefaultLevel = PercentToLevel(default_plugged_offset_);
  EXPECT_EQ(kDefaultLevel, backlight_.current_level());

  // Test suspend and resume.  When suspending, the previously-current
  // brightness level should be saved as the resume level.
  display_power_setter_.reset_num_power_calls();
  controller_->SetSuspended(true);
  EXPECT_EQ(0, display_power_setter_.num_power_calls());
  EXPECT_EQ(0, backlight_.current_level());
  EXPECT_EQ(kDefaultLevel, backlight_.resume_level());

  controller_->SetSuspended(false);
  EXPECT_EQ(kDefaultLevel, backlight_.current_level());
  EXPECT_EQ(chromeos::DISPLAY_POWER_ALL_ON, display_power_setter_.state());
  EXPECT_EQ(0, display_power_setter_.delay().InMilliseconds());

  // Test idling into suspend state.  The backlight should be at 0% after the
  // display is turned off, but it should be set back to the active level (with
  // the screen still off) before suspending, so that the kernel driver can
  // restore that level after resuming.
  backlight_.clear_resume_level();
  controller_->SetDimmedForInactivity(true);
  EXPECT_LT(backlight_.current_level(), kDefaultLevel);
  EXPECT_EQ(chromeos::DISPLAY_POWER_ALL_ON, display_power_setter_.state());

  // The displays are turned off for the idle-off state.
  controller_->SetOffForInactivity(true);
  EXPECT_EQ(0, backlight_.current_level());
  EXPECT_EQ(chromeos::DISPLAY_POWER_ALL_OFF, display_power_setter_.state());

  // The power manager shouldn't change the display power before
  // suspending; Chrome will turn the displays on (without any involvement
  // from powerd) so that they come back up in the correct state after
  // resuming.
  display_power_setter_.reset_num_power_calls();
  controller_->SetSuspended(true);
  EXPECT_EQ(0, display_power_setter_.num_power_calls());
  EXPECT_EQ(0, backlight_.current_level());
  EXPECT_EQ(kDefaultLevel, backlight_.resume_level());

  // Test resume.
  controller_->SetSuspended(false);
  controller_->SetOffForInactivity(false);
  controller_->SetDimmedForInactivity(false);
  EXPECT_EQ(kDefaultLevel, backlight_.current_level());
  EXPECT_EQ(chromeos::DISPLAY_POWER_ALL_ON, display_power_setter_.state());
  EXPECT_EQ(0, display_power_setter_.delay().InMilliseconds());
}

// Test that we use a linear mapping between brightness levels and percentages
// when a small range of levels is exposed by the hardware.
TEST_F(InternalBacklightControllerTest, LinearMappingForSmallBacklightRange) {
  max_backlight_level_ = initial_backlight_level_ = 10;
  Init(POWER_BATTERY);

  // The minimum visible level should use the bottom brightness step's
  // percentage, and above it, there should be a linear mapping between levels
  // and percentages.
  const double kMinVisiblePercent =
      InternalBacklightController::kMinVisiblePercent;
  for (int i = 1; i <= max_backlight_level_; ++i) {
    double percent = kMinVisiblePercent +
        (100.0 - kMinVisiblePercent) * (i - 1) / (max_backlight_level_ - 1);
    EXPECT_EQ(static_cast<int64>(i), PercentToLevel(percent));
  }
}

TEST_F(InternalBacklightControllerTest, NonLinearMapping) {
  // We should use a non-linear mapping that provides more granularity at
  // the bottom end when a large range is exposed.
  max_backlight_level_ = initial_backlight_level_ = 1000;
  Init(POWER_BATTERY);

  EXPECT_EQ(0, PercentToLevel(0.0));
  EXPECT_LT(PercentToLevel(50.0), max_backlight_level_ / 2);
  EXPECT_EQ(max_backlight_level_, PercentToLevel(100.0));
}

TEST_F(InternalBacklightControllerTest, AmbientLightTransitions) {
  initial_backlight_level_ = max_backlight_level_;
  report_initial_als_reading_ = false;
  Init(POWER_AC);

  // The controller should leave the initial brightness unchanged before it's
  // received a reading from the ambient light sensor.
  EXPECT_EQ(initial_backlight_level_, backlight_.current_level());

  // After getting the first reading from the sensor, we should do a slow
  // transition to a lower level.
  light_sensor_.NotifyObservers();
  EXPECT_EQ(PercentToLevel(default_plugged_offset_),
            backlight_.current_level());
  EXPECT_EQ(kSlowBacklightTransitionMs,
            backlight_.current_interval().InMilliseconds());

  // Pass a bunch of 100% readings and check that we slowly increase the
  // brightness.
  light_sensor_.set_values(100.0, 100);
  for (int i = 0; i < kAlsSamplesToTriggerAdjustment; ++i)
    light_sensor_.NotifyObservers();
  EXPECT_GT(backlight_.current_level(),
            PercentToLevel(default_plugged_offset_));
  EXPECT_EQ(kSlowBacklightTransitionMs,
            backlight_.current_interval().InMilliseconds());
}

TEST_F(InternalBacklightControllerTest, TurnDisplaysOffWhenShuttingDown) {
  Init(POWER_AC);

  // When the backlight controller is told that the system is shutting down, it
  // should turn off all displays.
  controller_->SetShuttingDown(true);
  EXPECT_EQ(chromeos::DISPLAY_POWER_ALL_OFF, display_power_setter_.state());
  EXPECT_EQ(0, display_power_setter_.delay().InMilliseconds());

  // This isn't expected, but if the state changes after we start shutting down,
  // the displays should be turned back on.
  controller_->SetShuttingDown(false);
  EXPECT_EQ(chromeos::DISPLAY_POWER_ALL_ON, display_power_setter_.state());
  EXPECT_EQ(0, display_power_setter_.delay().InMilliseconds());
}

// Test that HandlePowerSourceChange() sets the brightness appropriately
// when the computer is plugged and unplugged.
TEST_F(InternalBacklightControllerTest, TestPlug) {
  Init(POWER_BATTERY);
  const int64 kUnpluggedLevel = PercentToLevel(default_unplugged_offset_);
  const int64 kPluggedLevel = PercentToLevel(default_plugged_offset_);

  EXPECT_EQ(kUnpluggedLevel, backlight_.current_level());
  controller_->HandlePowerSourceChange(POWER_AC);
  EXPECT_EQ(kPluggedLevel, backlight_.current_level());
  controller_->HandlePowerSourceChange(POWER_BATTERY);
  EXPECT_EQ(kUnpluggedLevel, backlight_.current_level());
  controller_->HandlePowerSourceChange(POWER_AC);
  EXPECT_EQ(kPluggedLevel, backlight_.current_level());
  controller_->HandlePowerSourceChange(POWER_AC);
  EXPECT_EQ(kPluggedLevel, backlight_.current_level());
}

// Test that HandlePowerSourceChange() sets the brightness appropriately
// when the computer is unplugged and plugged.
TEST_F(InternalBacklightControllerTest, TestUnplug) {
  Init(POWER_AC);
  const int64 kUnpluggedLevel = PercentToLevel(default_unplugged_offset_);
  const int64 kPluggedLevel = PercentToLevel(default_plugged_offset_);

  EXPECT_EQ(kPluggedLevel, backlight_.current_level());
  controller_->HandlePowerSourceChange(POWER_BATTERY);
  EXPECT_EQ(kUnpluggedLevel, backlight_.current_level());
  controller_->HandlePowerSourceChange(POWER_AC);
  EXPECT_EQ(kPluggedLevel, backlight_.current_level());
  controller_->HandlePowerSourceChange(POWER_BATTERY);
  EXPECT_EQ(kUnpluggedLevel, backlight_.current_level());
}

TEST_F(InternalBacklightControllerTest, TestDimming) {
  Init(POWER_AC);
  const int64 kPluggedLevel = PercentToLevel(default_plugged_offset_);
  EXPECT_EQ(kPluggedLevel, backlight_.current_level());

  controller_->SetDimmedForInactivity(true);
  int64 dimmed_level = backlight_.current_level();
  EXPECT_LT(dimmed_level, kPluggedLevel);
  EXPECT_GT(dimmed_level, 0);
  EXPECT_EQ(kFastBacklightTransitionMs,
            backlight_.current_interval().InMilliseconds());

  // A second dim request shouldn't change the level.
  controller_->SetDimmedForInactivity(true);
  EXPECT_EQ(dimmed_level, backlight_.current_level());

  // User requests and ambient light readings shouldn't change the
  // backlight level while it's dimmed.
  const double kNewUserOffset = 67.0;
  EXPECT_FALSE(controller_->SetUserBrightnessPercent(
      kNewUserOffset, BacklightController::TRANSITION_INSTANT));
  EXPECT_EQ(dimmed_level, backlight_.current_level());

  const double kNewAlsOffset = 12.0;
  light_sensor_.set_values(kNewAlsOffset, 0);
  for (int i = 0; i < kAlsSamplesToTriggerAdjustment; ++i)
    light_sensor_.NotifyObservers();
  EXPECT_EQ(dimmed_level, backlight_.current_level());

  // After leaving the dimmed state, the updated user plus ALS offset
  // should be used.
  controller_->SetDimmedForInactivity(false);
  EXPECT_EQ(PercentToLevel(kNewUserOffset + kNewAlsOffset),
            backlight_.current_level());
  EXPECT_EQ(kFastBacklightTransitionMs,
            backlight_.current_interval().InMilliseconds());

  // If the brightness is already below the dimmed level, it shouldn't be
  // changed when dimming is requested.
  light_sensor_.set_values(0.0, 0);
  for (int i = 0; i < kAlsSamplesToTriggerAdjustment; ++i)
    light_sensor_.NotifyObservers();
  ASSERT_TRUE(controller_->SetUserBrightnessPercent(
      InternalBacklightController::kMinVisiblePercent,
      BacklightController::TRANSITION_INSTANT));
  int64 new_undimmed_level = backlight_.current_level();
  ASSERT_LT(new_undimmed_level, dimmed_level);
  controller_->SetDimmedForInactivity(true);
  EXPECT_EQ(new_undimmed_level, backlight_.current_level());
}

TEST_F(InternalBacklightControllerTest, UserOffsets) {
  // Start out negative user offsets and 0% ambient light.  The backlight
  // should be turned on at the minimum level after initialization.
  default_plugged_offset_ = -4.0;
  default_unplugged_offset_ = -10.0;
  initial_als_percent_ = 0.0;
  Init(POWER_AC);
  const int kMinVisibleLevel =
      PercentToLevel(InternalBacklightController::kMinVisiblePercent);
  EXPECT_EQ(kMinVisibleLevel, backlight_.current_level());

  // The user offset prefs should stay at their initial values after Init()
  // is called.
  double pref_value;
  ASSERT_TRUE(prefs_.GetDouble(kPluggedBrightnessOffsetPref, &pref_value));
  EXPECT_DOUBLE_EQ(default_plugged_offset_, pref_value);
  ASSERT_TRUE(prefs_.GetDouble(kUnpluggedBrightnessOffsetPref, &pref_value));
  EXPECT_DOUBLE_EQ(default_unplugged_offset_, pref_value);

  // Calling SetUserBrightnessPercent() while on AC power should update the
  // plugged-offset pref but leave the unplugged pref untouched.
  const double kNewPluggedOffset = 80.0;
  ASSERT_TRUE(controller_->SetUserBrightnessPercent(
      kNewPluggedOffset, BacklightController::TRANSITION_INSTANT));
  ASSERT_TRUE(prefs_.GetDouble(kPluggedBrightnessOffsetPref, &pref_value));
  EXPECT_DOUBLE_EQ(kNewPluggedOffset, pref_value);
  ASSERT_TRUE(prefs_.GetDouble(kUnpluggedBrightnessOffsetPref, &pref_value));
  EXPECT_DOUBLE_EQ(default_unplugged_offset_, pref_value);

  // Now check that the unplugged-offset pref is written.
  const double kNewUnpluggedOffset = 70.0;
  controller_->HandlePowerSourceChange(POWER_BATTERY);
  ASSERT_TRUE(controller_->SetUserBrightnessPercent(
      kNewUnpluggedOffset, BacklightController::TRANSITION_INSTANT));
  ASSERT_TRUE(prefs_.GetDouble(kPluggedBrightnessOffsetPref, &pref_value));
  EXPECT_DOUBLE_EQ(kNewPluggedOffset, pref_value);
  ASSERT_TRUE(prefs_.GetDouble(kUnpluggedBrightnessOffsetPref, &pref_value));
  EXPECT_DOUBLE_EQ(kNewUnpluggedOffset, pref_value);

  // Increase the ambient brightness.
  const double kNewAlsPercent = 35.0;
  light_sensor_.set_values(kNewAlsPercent, 0);
  for (int i = 0; i < kAlsSamplesToTriggerAdjustment; ++i)
    light_sensor_.NotifyObservers();
  EXPECT_EQ(max_backlight_level_, backlight_.current_level());

  // Request a lower brightness than the ALS offset.  The request should be
  // honored and the user offset pref should be updated correspondingly.
  const double kLowBrightness = 10.0;
  ASSERT_TRUE(controller_->SetUserBrightnessPercent(
      kLowBrightness, BacklightController::TRANSITION_INSTANT));
  EXPECT_EQ(PercentToLevel(kLowBrightness), backlight_.current_level());
  ASSERT_TRUE(prefs_.GetDouble(kUnpluggedBrightnessOffsetPref, &pref_value));
  EXPECT_DOUBLE_EQ(kLowBrightness - kNewAlsPercent, pref_value);

  // Set the ambient brightness to 0.  Even though the sum of the user
  // offset and the ALS offset is negative, the backlight should stay on.
  light_sensor_.set_values(0.0, 0);
  for (int i = 0; i < kAlsSamplesToTriggerAdjustment; ++i)
    light_sensor_.NotifyObservers();
  EXPECT_EQ(kMinVisibleLevel, backlight_.current_level());

  // Request a brightness of 0% and check that the backlight is turned off.
  ASSERT_TRUE(controller_->SetUserBrightnessPercent(
      0.0, BacklightController::TRANSITION_INSTANT));
  ASSERT_TRUE(prefs_.GetDouble(kUnpluggedBrightnessOffsetPref, &pref_value));
  EXPECT_DOUBLE_EQ(0.0, pref_value);
  EXPECT_EQ(0, backlight_.current_level());

  // The backlight should stay off even when the ambient light increases.
  light_sensor_.set_values(25.0, 0);
  for (int i = 0; i < kAlsSamplesToTriggerAdjustment; ++i)
    light_sensor_.NotifyObservers();
  EXPECT_EQ(0, backlight_.current_level());
}

TEST_F(InternalBacklightControllerTest, DeferInitialAdjustment) {
  // The brightness level should remain unchanged when the power source and
  // initial ambient light reading haven't been received.
  report_initial_power_source_ = false;
  report_initial_als_reading_ = false;
  Init(POWER_AC);
  EXPECT_EQ(initial_backlight_level_, backlight_.current_level());

  // Send the power source; the level still shouldn't change.
  controller_->HandlePowerSourceChange(POWER_AC);
  EXPECT_EQ(initial_backlight_level_, backlight_.current_level());

  // After the ambient light level is also received, the backlight should
  // slowly transition to the level from the pref.
  light_sensor_.NotifyObservers();
  EXPECT_EQ(PercentToLevel(default_plugged_offset_),
            backlight_.current_level());
  EXPECT_EQ(kSlowBacklightTransitionMs,
            backlight_.current_interval().InMilliseconds());
}

TEST_F(InternalBacklightControllerTest, NoAmbientLightSensor) {
  pass_light_sensor_ = false;
  report_initial_power_source_ = false;
  report_initial_als_reading_ = false;
  Init(POWER_AC);
  EXPECT_EQ(initial_backlight_level_, backlight_.current_level());

  // When no ambient light sensor was passed to the controller, it should
  // update the brightness level immediately after getting the plugged
  // state instead of waiting for an ambient light reading.
  controller_->HandlePowerSourceChange(POWER_AC);
  EXPECT_EQ(PercentToLevel(default_plugged_offset_),
            backlight_.current_level());
  EXPECT_EQ(kFastBacklightTransitionMs,
            backlight_.current_interval().InMilliseconds());
}

TEST_F(InternalBacklightControllerTest, AvoidStrangePowerSourceAdjustments) {
  default_plugged_offset_ = 40.0;
  default_unplugged_offset_ = 20.0;
  Init(POWER_AC);
  EXPECT_EQ(PercentToLevel(default_plugged_offset_),
            backlight_.current_level());

  // After requesting a brightness lower than the battery brightness while
  // on AC and then switching to battery, the screen should stay at the low
  // level instead of being brightened.
  const double kNewPluggedPercent = 10.0;
  ASSERT_TRUE(controller_->SetUserBrightnessPercent(
      kNewPluggedPercent, BacklightController::TRANSITION_INSTANT));
  EXPECT_EQ(PercentToLevel(kNewPluggedPercent), backlight_.current_level());
  controller_->HandlePowerSourceChange(POWER_BATTERY);
  EXPECT_EQ(PercentToLevel(kNewPluggedPercent), backlight_.current_level());

  // The unplugged pref shouldn't be changed.
  double pref_value = 0.0;
  ASSERT_TRUE(prefs_.GetDouble(kUnpluggedBrightnessOffsetPref, &pref_value));
  EXPECT_DOUBLE_EQ(default_unplugged_offset_, pref_value);

  // The screen also shouldn't be dimmed in response to a change to AC.
  const double kNewUnpluggedPercent = 60.0;
  ASSERT_TRUE(controller_->SetUserBrightnessPercent(
      kNewUnpluggedPercent, BacklightController::TRANSITION_INSTANT));
  EXPECT_EQ(PercentToLevel(kNewUnpluggedPercent), backlight_.current_level());
  controller_->HandlePowerSourceChange(POWER_AC);
  EXPECT_EQ(PercentToLevel(kNewUnpluggedPercent), backlight_.current_level());

  // The plugged pref shouldn't be changed.
  ASSERT_TRUE(prefs_.GetDouble(kPluggedBrightnessOffsetPref, &pref_value));
  EXPECT_DOUBLE_EQ(kNewPluggedPercent, pref_value);
}

TEST_F(InternalBacklightControllerTest, ForceBacklightOn) {
  // Set the brightness to zero and check that it's increased to the
  // minimum visible level when the session state changes.
  Init(POWER_AC);
  const int kMinVisibleLevel =
      PercentToLevel(InternalBacklightController::kMinVisiblePercent);
  ASSERT_TRUE(controller_->SetUserBrightnessPercent(
      0.0, BacklightController::TRANSITION_INSTANT));
  ASSERT_EQ(0, backlight_.current_level());
  controller_->HandleSessionStateChange(SESSION_STARTED);
  EXPECT_EQ(kMinVisibleLevel, backlight_.current_level());

  // Pressing the power button should also increase the brightness.
  ASSERT_TRUE(controller_->SetUserBrightnessPercent(
      0.0, BacklightController::TRANSITION_INSTANT));
  ASSERT_EQ(0, backlight_.current_level());
  controller_->HandlePowerButtonPress();
  EXPECT_EQ(kMinVisibleLevel, backlight_.current_level());

  // Enter presentation mode.  The same actions that forced the backlight
  // on before shouldn't do anything now; turning the panel back on while a
  // second display is connected would resize the desktop.
  controller_->HandleDisplayModeChange(DISPLAY_PRESENTATION);
  ASSERT_TRUE(controller_->SetUserBrightnessPercent(
      0.0, BacklightController::TRANSITION_INSTANT));
  ASSERT_EQ(0, backlight_.current_level());
  controller_->HandleSessionStateChange(SESSION_STOPPED);
  EXPECT_EQ(0, backlight_.current_level());
  controller_->HandlePowerButtonPress();
  EXPECT_EQ(0, backlight_.current_level());

  // The backlight should be turned on after exiting presentation mode.
  controller_->HandleDisplayModeChange(DISPLAY_NORMAL);
  EXPECT_EQ(kMinVisibleLevel, backlight_.current_level());
}

}  // namespace power_manager
