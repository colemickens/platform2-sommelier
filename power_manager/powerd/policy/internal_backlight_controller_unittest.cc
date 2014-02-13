// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/policy/internal_backlight_controller.h"

#include <gtest/gtest.h>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "chromeos/dbus/service_constants.h"
#include "power_manager/common/clock.h"
#include "power_manager/common/fake_prefs.h"
#include "power_manager/common/power_constants.h"
#include "power_manager/powerd/policy/backlight_controller_observer_stub.h"
#include "power_manager/powerd/system/ambient_light_sensor_stub.h"
#include "power_manager/powerd/system/backlight_stub.h"
#include "power_manager/powerd/system/display_power_setter_stub.h"
#include "power_manager/proto_bindings/policy.pb.h"

namespace power_manager {
namespace policy {

namespace {

// Number of ambient light sensor samples that should be supplied in order to
// trigger an update to InternalBacklightController's ALS brightness percent.
const int kAlsSamplesToTriggerAdjustment = 2;

}  // namespace

class InternalBacklightControllerTest : public ::testing::Test {
 public:
  InternalBacklightControllerTest()
      : max_backlight_level_(1024),
        initial_backlight_level_(512),
        pass_light_sensor_(true),
        initial_als_lux_(0),
        report_initial_als_reading_(true),
        report_initial_power_source_(true),
        default_min_visible_level_(1),
        default_als_limits_("0.0\n30.0\n100.0"),
        default_als_steps_("50.0 -1 400\n90.0 100 -1"),
        default_no_als_ac_brightness_(80.0),
        default_no_als_battery_brightness_(60.0),
        backlight_(max_backlight_level_, initial_backlight_level_),
        light_sensor_(initial_als_lux_) {
  }

  // Initializes |controller_| and sends it power source and ambient light
  // event such that it should make its first adjustment to the backlight
  // brightness.
  virtual void Init(PowerSource power_source) {
    prefs_.SetInt64(kMinVisibleBacklightLevelPref, default_min_visible_level_);
    prefs_.SetString(kInternalBacklightAlsLimitsPref, default_als_limits_);
    prefs_.SetString(kInternalBacklightAlsStepsPref, default_als_steps_);
    prefs_.SetDouble(kInternalBacklightNoAlsAcBrightnessPref,
                     default_no_als_ac_brightness_);
    prefs_.SetDouble(kInternalBacklightNoAlsBatteryBrightnessPref,
                     default_no_als_battery_brightness_);
    backlight_.set_max_level(max_backlight_level_);
    backlight_.set_current_level(initial_backlight_level_);
    light_sensor_.set_lux(initial_als_lux_);

    controller_.reset(new InternalBacklightController);
    if (!init_time_.is_null())
      controller_->clock()->set_current_time_for_testing(init_time_);
    controller_->Init(&backlight_, &prefs_,
                      pass_light_sensor_ ? &light_sensor_ : NULL,
                      &display_power_setter_);

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

  // Time that should be used as "now" when |controller_| is initialized.
  // If unset, the real time will be used.
  base::TimeTicks init_time_;

  // Should Init() pass |light_sensor_| to |controller_|?
  bool pass_light_sensor_;

  // Initial lux level reported by |light_sensor_|.
  int initial_als_lux_;

  // Should Init() tell |controller_| about an initial ambient light
  // reading and power source change?
  bool report_initial_als_reading_;
  bool report_initial_power_source_;

  // Default values for prefs.  Applied when Init() is called.
  int64 default_min_visible_level_;
  std::string default_als_limits_;
  std::string default_als_steps_;
  double default_no_als_ac_brightness_;
  double default_no_als_battery_brightness_;

  FakePrefs prefs_;
  system::BacklightStub backlight_;
  system::AmbientLightSensorStub light_sensor_;
  system::DisplayPowerSetterStub display_power_setter_;
  scoped_ptr<InternalBacklightController> controller_;
};

TEST_F(InternalBacklightControllerTest, IncreaseAndDecreaseBrightness) {
  default_min_visible_level_ = 100;
  default_als_steps_ = "50.0 -1 -1";
  Init(POWER_BATTERY);
  EXPECT_EQ(default_min_visible_level_,
            PercentToLevel(InternalBacklightController::kMinVisiblePercent));
  const int64 kAlsLevel = PercentToLevel(50.0);
  EXPECT_EQ(kAlsLevel, backlight_.current_level());

  // Check that the first step increases the brightness; within the loop
  // we'll just ensure that the brightness never decreases.
  controller_->IncreaseUserBrightness();
  EXPECT_GT(backlight_.current_level(), kAlsLevel);
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
  EXPECT_EQ(chromeos::DISPLAY_POWER_INTERNAL_OFF_EXTERNAL_ON,
            display_power_setter_.state());
  EXPECT_EQ(kFastBacklightTransitionMs,
            display_power_setter_.delay().InMilliseconds());

  // One increase request should raise the brightness to the minimum
  // visible level, while a second one should increase it above that.
  controller_->IncreaseUserBrightness();
  EXPECT_EQ(default_min_visible_level_, backlight_.current_level());
  EXPECT_EQ(chromeos::DISPLAY_POWER_ALL_ON, display_power_setter_.state());
  EXPECT_EQ(0, display_power_setter_.delay().InMilliseconds());
  controller_->IncreaseUserBrightness();
  EXPECT_GT(backlight_.current_level(), default_min_visible_level_);

  // Start at 3/4 of a step above the middle step. After a decrease request, the
  // brightness should be snapped to the middle step.
  const double kStep = 100.0 / InternalBacklightController::kMaxBrightnessSteps;
  const double kMiddlePercent =
      kStep * InternalBacklightController::kMaxBrightnessSteps / 2;
  controller_->SetUserBrightnessPercent(
      kMiddlePercent + 0.75 * kStep, BacklightController::TRANSITION_INSTANT);
  ASSERT_EQ(PercentToLevel(kMiddlePercent + 0.75 * kStep),
            backlight_.current_level());
  controller_->DecreaseUserBrightness(true);
  EXPECT_EQ(PercentToLevel(kMiddlePercent), backlight_.current_level());

  // Start at 1/4 of a step below the middle step. After an increase request,
  // the brightness should be snapped to one step above the middle step.
  controller_->SetUserBrightnessPercent(
      kMiddlePercent - 0.25 * kStep, BacklightController::TRANSITION_INSTANT);
  ASSERT_EQ(PercentToLevel(kMiddlePercent - 0.25 * kStep),
            backlight_.current_level());
  controller_->IncreaseUserBrightness();
  EXPECT_EQ(PercentToLevel(kMiddlePercent + kStep), backlight_.current_level());
}

// Test that InternalBacklightController notifies its observer in response to
// brightness changes.
TEST_F(InternalBacklightControllerTest, NotifyObserver) {
  default_min_visible_level_ = 100;
  default_als_steps_ = "50.0 -1 200\n75.0 100 -1";
  Init(POWER_BATTERY);
  EXPECT_EQ(PercentToLevel(50.0), backlight_.current_level());

  BacklightControllerObserverStub observer;
  controller_->AddObserver(&observer);

  // Send enough ambient light sensor samples to trigger a brightness change.
  observer.Clear();
  int64 old_level = backlight_.current_level();
  light_sensor_.set_lux(300);
  for (int i = 0; i < kAlsSamplesToTriggerAdjustment; ++i)
    light_sensor_.NotifyObservers();
  ASSERT_NE(old_level, backlight_.current_level());
  ASSERT_EQ(1, static_cast<int>(observer.changes().size()));
  EXPECT_EQ(backlight_.current_level(),
            PercentToLevel(observer.changes()[0].percent));
  EXPECT_EQ(BacklightController::BRIGHTNESS_CHANGE_AUTOMATED,
            observer.changes()[0].cause);
  EXPECT_EQ(controller_.get(), observer.changes()[0].source);

  // Increase the brightness and check that the observer is notified.
  observer.Clear();
  ASSERT_TRUE(controller_->IncreaseUserBrightness());
  ASSERT_EQ(1, static_cast<int>(observer.changes().size()));
  EXPECT_EQ(backlight_.current_level(),
            PercentToLevel(observer.changes()[0].percent));
  EXPECT_EQ(BacklightController::BRIGHTNESS_CHANGE_USER_INITIATED,
            observer.changes()[0].cause);
  EXPECT_EQ(controller_.get(), observer.changes()[0].source);

  // Decrease the brightness.
  observer.Clear();
  ASSERT_TRUE(controller_->DecreaseUserBrightness(true /* allow_off */));
  ASSERT_EQ(1, static_cast<int>(observer.changes().size()));
  EXPECT_EQ(backlight_.current_level(),
            PercentToLevel(observer.changes()[0].percent));
  EXPECT_EQ(BacklightController::BRIGHTNESS_CHANGE_USER_INITIATED,
            observer.changes()[0].cause);
  EXPECT_EQ(controller_.get(), observer.changes()[0].source);

  // Set the brightness to a low level.
  observer.Clear();
  const double kLowPercent = 10.0;
  ASSERT_TRUE(controller_->SetUserBrightnessPercent(
      kLowPercent, BacklightController::TRANSITION_INSTANT));
  ASSERT_EQ(1, static_cast<int>(observer.changes().size()));
  EXPECT_EQ(backlight_.current_level(),
            PercentToLevel(observer.changes()[0].percent));
  EXPECT_EQ(BacklightController::BRIGHTNESS_CHANGE_USER_INITIATED,
            observer.changes()[0].cause);
  EXPECT_EQ(controller_.get(), observer.changes()[0].source);

  // Plug the device in.
  observer.Clear();
  controller_->HandlePowerSourceChange(POWER_AC);
  ASSERT_EQ(1, static_cast<int>(observer.changes().size()));
  EXPECT_EQ(backlight_.current_level(),
            PercentToLevel(observer.changes()[0].percent));
  EXPECT_EQ(BacklightController::BRIGHTNESS_CHANGE_AUTOMATED,
            observer.changes()[0].cause);
  EXPECT_EQ(controller_.get(), observer.changes()[0].source);

  // Dim the backlight.
  observer.Clear();
  controller_->SetDimmedForInactivity(true);
  ASSERT_EQ(1, static_cast<int>(observer.changes().size()));
  EXPECT_EQ(backlight_.current_level(),
            PercentToLevel(observer.changes()[0].percent));
  EXPECT_EQ(BacklightController::BRIGHTNESS_CHANGE_AUTOMATED,
            observer.changes()[0].cause);
  EXPECT_EQ(controller_.get(), observer.changes()[0].source);

  controller_->RemoveObserver(&observer);
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
  const int64 initial_level = backlight_.current_level();

  // Test suspend and resume.  When suspending, the previously-current
  // brightness level should be saved as the resume level.
  display_power_setter_.reset_num_power_calls();
  controller_->SetSuspended(true);
  EXPECT_EQ(0, display_power_setter_.num_power_calls());
  EXPECT_EQ(0, backlight_.current_level());
  EXPECT_EQ(0, backlight_.current_interval().InMilliseconds());
  EXPECT_EQ(initial_level, backlight_.resume_level());

  controller_->SetSuspended(false);
  EXPECT_EQ(initial_level, backlight_.current_level());
  EXPECT_EQ(0, backlight_.current_interval().InMilliseconds());
  EXPECT_EQ(chromeos::DISPLAY_POWER_ALL_ON, display_power_setter_.state());
  EXPECT_EQ(0, display_power_setter_.delay().InMilliseconds());

  // Test idling into suspend state.  The backlight should be at 0% after the
  // display is turned off, but it should be set back to the active level (with
  // the screen still off) before suspending, so that the kernel driver can
  // restore that level after resuming.
  backlight_.clear_resume_level();
  controller_->SetDimmedForInactivity(true);
  EXPECT_LT(backlight_.current_level(), initial_level);
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
  EXPECT_EQ(initial_level, backlight_.resume_level());

  // Test resume.
  controller_->SetDimmedForInactivity(false);
  controller_->SetOffForInactivity(false);
  controller_->SetSuspended(false);
  EXPECT_EQ(initial_level, backlight_.current_level());
  EXPECT_EQ(0, backlight_.current_interval().InMilliseconds());
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
  default_als_steps_ = "50.0 -1 200\n75.0 100 -1";
  report_initial_als_reading_ = false;
  Init(POWER_AC);

  // The controller should leave the initial brightness unchanged before it's
  // received a reading from the ambient light sensor.
  EXPECT_EQ(initial_backlight_level_, backlight_.current_level());
  EXPECT_EQ(0, controller_->GetNumAmbientLightSensorAdjustments());

  // After getting the first reading from the sensor, we should do a slow
  // transition to the lower level. The initial adjustment doesn't contribute to
  // the adjustment count.
  light_sensor_.NotifyObservers();
  EXPECT_EQ(PercentToLevel(50.0), backlight_.current_level());
  EXPECT_EQ(kSlowBacklightTransitionMs,
            backlight_.current_interval().InMilliseconds());
  EXPECT_EQ(0, controller_->GetNumAmbientLightSensorAdjustments());

  // Pass a bunch of higher readings and check that we slowly increase the
  // brightness.
  light_sensor_.set_lux(400);
  for (int i = 0; i < kAlsSamplesToTriggerAdjustment; ++i)
    light_sensor_.NotifyObservers();
  EXPECT_EQ(PercentToLevel(75.0), backlight_.current_level());
  EXPECT_EQ(kSlowBacklightTransitionMs,
            backlight_.current_interval().InMilliseconds());
  EXPECT_EQ(1, controller_->GetNumAmbientLightSensorAdjustments());

  // Check that the adjustment count is reset when a new session starts.
  controller_->HandleSessionStateChange(SESSION_STARTED);
  EXPECT_EQ(0, controller_->GetNumAmbientLightSensorAdjustments());
}

TEST_F(InternalBacklightControllerTest, PowerSourceChangeNotReportedAsAmbient) {
  // Set a single step for ambient-light-controlled brightness levels, using 60%
  // while on AC and 50% while on battery.
  initial_backlight_level_ = max_backlight_level_;
  default_als_steps_ = "60.0 50.0 -1 -1";
  Init(POWER_AC);
  ASSERT_EQ(PercentToLevel(60.0), backlight_.current_level());
  EXPECT_EQ(0, controller_->GetNumAmbientLightSensorAdjustments());

  // The brightness should be updated after switching to battery power, but the
  // change shouldn't be reported as having been triggered by the ambient light
  // sensor.
  controller_->HandlePowerSourceChange(POWER_BATTERY);
  ASSERT_EQ(PercentToLevel(50.0), backlight_.current_level());
  EXPECT_EQ(0, controller_->GetNumAmbientLightSensorAdjustments());
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
TEST_F(InternalBacklightControllerTest, TestPlugAndUnplug) {
  Init(POWER_BATTERY);

  const double kUnpluggedPercent = 40.0;
  EXPECT_TRUE(controller_->SetUserBrightnessPercent(
      kUnpluggedPercent, BacklightController::TRANSITION_INSTANT));
  EXPECT_EQ(PercentToLevel(kUnpluggedPercent), backlight_.current_level());

  const double kPluggedPercent = 60.0;
  controller_->HandlePowerSourceChange(POWER_AC);
  EXPECT_TRUE(controller_->SetUserBrightnessPercent(
      kPluggedPercent, BacklightController::TRANSITION_INSTANT));
  EXPECT_EQ(PercentToLevel(kPluggedPercent), backlight_.current_level());

  controller_->HandlePowerSourceChange(POWER_BATTERY);
  EXPECT_EQ(PercentToLevel(kUnpluggedPercent), backlight_.current_level());
  controller_->HandlePowerSourceChange(POWER_AC);
  EXPECT_EQ(PercentToLevel(kPluggedPercent), backlight_.current_level());

  // After requesting a brightness lower than the battery brightness while
  // on AC and then switching to battery, the screen should stay at the low
  // level instead of being brightened.
  const double kNewPluggedPercent = 20.0;
  ASSERT_TRUE(controller_->SetUserBrightnessPercent(
      kNewPluggedPercent, BacklightController::TRANSITION_INSTANT));
  EXPECT_EQ(PercentToLevel(kNewPluggedPercent), backlight_.current_level());
  controller_->HandlePowerSourceChange(POWER_BATTERY);
  EXPECT_EQ(PercentToLevel(kNewPluggedPercent), backlight_.current_level());

  // The screen also shouldn't be dimmed in response to a change to AC.
  const double kNewUnpluggedPercent = 60.0;
  ASSERT_TRUE(controller_->SetUserBrightnessPercent(
      kNewUnpluggedPercent, BacklightController::TRANSITION_INSTANT));
  EXPECT_EQ(PercentToLevel(kNewUnpluggedPercent), backlight_.current_level());
  controller_->HandlePowerSourceChange(POWER_AC);
  EXPECT_EQ(PercentToLevel(kNewUnpluggedPercent), backlight_.current_level());
}

TEST_F(InternalBacklightControllerTest, TestDimming) {
  default_als_steps_ = "50.0 -1 200\n75.0 100 -1";
  Init(POWER_AC);
  int64 bottom_als_level = PercentToLevel(50.0);
  EXPECT_EQ(bottom_als_level, backlight_.current_level());

  controller_->SetDimmedForInactivity(true);
  int64 dimmed_level = backlight_.current_level();
  EXPECT_LT(dimmed_level, bottom_als_level);
  EXPECT_GT(dimmed_level, 0);
  EXPECT_EQ(kFastBacklightTransitionMs,
            backlight_.current_interval().InMilliseconds());

  // A second dim request shouldn't change the level.
  controller_->SetDimmedForInactivity(true);
  EXPECT_EQ(dimmed_level, backlight_.current_level());

  // Ambient light readings shouldn't change the backlight level while it's
  // dimmed.
  light_sensor_.set_lux(400);
  for (int i = 0; i < kAlsSamplesToTriggerAdjustment; ++i)
    light_sensor_.NotifyObservers();
  EXPECT_EQ(dimmed_level, backlight_.current_level());

  // After leaving the dimmed state, the updated ALS level should be used.
  controller_->SetDimmedForInactivity(false);
  EXPECT_EQ(PercentToLevel(75.0), backlight_.current_level());
  EXPECT_EQ(kFastBacklightTransitionMs,
            backlight_.current_interval().InMilliseconds());

  // User requests shouldn't change the brightness while dimmed either.
  controller_->SetDimmedForInactivity(true);
  EXPECT_EQ(dimmed_level, backlight_.current_level());
  const double kNewUserOffset = 67.0;
  EXPECT_FALSE(controller_->SetUserBrightnessPercent(
      kNewUserOffset, BacklightController::TRANSITION_INSTANT));
  EXPECT_EQ(dimmed_level, backlight_.current_level());

  // After leaving the dimmed state, the updated user level should be used.
  controller_->SetDimmedForInactivity(false);
  EXPECT_EQ(PercentToLevel(kNewUserOffset), backlight_.current_level());
  EXPECT_EQ(kFastBacklightTransitionMs,
            backlight_.current_interval().InMilliseconds());

  // If the brightness is already below the dimmed level, it shouldn't be
  // changed when dimming is requested.
  ASSERT_TRUE(controller_->SetUserBrightnessPercent(
      InternalBacklightController::kMinVisiblePercent,
      BacklightController::TRANSITION_INSTANT));
  int64 new_undimmed_level = backlight_.current_level();
  ASSERT_LT(new_undimmed_level, dimmed_level);
  controller_->SetDimmedForInactivity(true);
  EXPECT_EQ(new_undimmed_level, backlight_.current_level());
}

TEST_F(InternalBacklightControllerTest, UserLevelOverridesAmbientLight) {
  default_als_steps_ = "50.0 -1 200\n75.0 100 -1";
  Init(POWER_AC);
  EXPECT_EQ(PercentToLevel(50.0), backlight_.current_level());
  EXPECT_EQ(0, controller_->GetNumAmbientLightSensorAdjustments());
  EXPECT_EQ(0, controller_->GetNumUserAdjustments());

  const double kUserPercent = 80.0;
  ASSERT_TRUE(controller_->SetUserBrightnessPercent(
      kUserPercent, BacklightController::TRANSITION_INSTANT));
  EXPECT_EQ(PercentToLevel(kUserPercent), backlight_.current_level());
  EXPECT_EQ(1, controller_->GetNumUserAdjustments());

  // Changes to the ambient light level shouldn't affect the backlight
  // brightness after the user has manually set it.
  light_sensor_.set_lux(400);
  for (int i = 0; i < kAlsSamplesToTriggerAdjustment; ++i)
    light_sensor_.NotifyObservers();
  EXPECT_EQ(PercentToLevel(kUserPercent), backlight_.current_level());
  EXPECT_EQ(0, controller_->GetNumAmbientLightSensorAdjustments());

  // Starting a new session should reset the user adjustment count.
  controller_->HandleSessionStateChange(SESSION_STARTED);
  EXPECT_EQ(0, controller_->GetNumUserAdjustments());
}

TEST_F(InternalBacklightControllerTest, DeferInitialAdjustment) {
  // The brightness level should remain unchanged when the power source and
  // initial ambient light reading haven't been received.
  report_initial_power_source_ = false;
  report_initial_als_reading_ = false;
  default_als_steps_ = "50.0 -1 -1";
  Init(POWER_AC);
  EXPECT_EQ(initial_backlight_level_, backlight_.current_level());

  // Send the power source; the level still shouldn't change.
  controller_->HandlePowerSourceChange(POWER_AC);
  EXPECT_EQ(initial_backlight_level_, backlight_.current_level());

  // After the ambient light level is also received, the backlight should
  // slowly transition to the ALS-derived level.
  light_sensor_.NotifyObservers();
  EXPECT_EQ(PercentToLevel(50.0), backlight_.current_level());
  EXPECT_EQ(kSlowBacklightTransitionMs,
            backlight_.current_interval().InMilliseconds());
}

TEST_F(InternalBacklightControllerTest, NoAmbientLightSensor) {
  pass_light_sensor_ = false;
  report_initial_power_source_ = false;
  report_initial_als_reading_ = false;
  default_no_als_ac_brightness_ = 95.0;
  default_no_als_battery_brightness_ = 75.0;
  Init(POWER_AC);
  EXPECT_EQ(initial_backlight_level_, backlight_.current_level());

  // The brightness percentages from the "no ALS" prefs should be used as
  // starting points when there's no ALS.
  controller_->HandlePowerSourceChange(POWER_AC);
  EXPECT_EQ(PercentToLevel(default_no_als_ac_brightness_),
            backlight_.current_level());

  controller_->HandlePowerSourceChange(POWER_BATTERY);
  EXPECT_EQ(PercentToLevel(default_no_als_battery_brightness_),
            backlight_.current_level());
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

  // Ditto for most user activity.
  ASSERT_TRUE(controller_->SetUserBrightnessPercent(
      0.0, BacklightController::TRANSITION_INSTANT));
  ASSERT_EQ(0, backlight_.current_level());
  controller_->HandleUserActivity(USER_ACTIVITY_OTHER);
  EXPECT_EQ(kMinVisibleLevel, backlight_.current_level());

  // User activity corresponding to brightness- or volume-related key presses
  // shouldn't increase the brightness, though.
  ASSERT_TRUE(controller_->SetUserBrightnessPercent(
      0.0, BacklightController::TRANSITION_INSTANT));
  ASSERT_EQ(0, backlight_.current_level());
  controller_->HandleUserActivity(USER_ACTIVITY_BRIGHTNESS_UP_KEY_PRESS);
  EXPECT_EQ(0, backlight_.current_level());
  controller_->HandleUserActivity(USER_ACTIVITY_BRIGHTNESS_DOWN_KEY_PRESS);
  EXPECT_EQ(0, backlight_.current_level());
  controller_->HandleUserActivity(USER_ACTIVITY_VOLUME_UP_KEY_PRESS);
  EXPECT_EQ(0, backlight_.current_level());
  controller_->HandleUserActivity(USER_ACTIVITY_VOLUME_DOWN_KEY_PRESS);
  EXPECT_EQ(0, backlight_.current_level());
  controller_->HandleUserActivity(USER_ACTIVITY_VOLUME_MUTE_KEY_PRESS);
  EXPECT_EQ(0, backlight_.current_level());

  // Enter presentation mode.  The same actions that forced the backlight
  // on before shouldn't do anything now; turning the panel back on while a
  // second display is connected would resize the desktop.
  controller_->HandleDisplayModeChange(DISPLAY_PRESENTATION);
  ASSERT_EQ(0, backlight_.current_level());
  controller_->HandleSessionStateChange(SESSION_STOPPED);
  EXPECT_EQ(0, backlight_.current_level());
  controller_->HandlePowerButtonPress();
  EXPECT_EQ(0, backlight_.current_level());
  controller_->HandleUserActivity(USER_ACTIVITY_OTHER);
  EXPECT_EQ(0, backlight_.current_level());

  // The backlight should be turned on after exiting presentation mode.
  controller_->HandleDisplayModeChange(DISPLAY_NORMAL);
  EXPECT_EQ(kMinVisibleLevel, backlight_.current_level());
}

TEST_F(InternalBacklightControllerTest, DockedMode) {
  Init(POWER_AC);
  const int64 initial_level = backlight_.current_level();
  ASSERT_GT(initial_level, 0);

  controller_->SetDocked(true);
  EXPECT_EQ(chromeos::DISPLAY_POWER_INTERNAL_OFF_EXTERNAL_ON,
            display_power_setter_.state());
  EXPECT_EQ(0, display_power_setter_.delay().InMilliseconds());
  EXPECT_EQ(0, backlight_.current_level());

  // Increasing the brightness or dimming the backlight shouldn't change
  // the displays' power settings while docked.
  controller_->IncreaseUserBrightness();
  EXPECT_EQ(chromeos::DISPLAY_POWER_INTERNAL_OFF_EXTERNAL_ON,
            display_power_setter_.state());
  EXPECT_EQ(0, backlight_.current_level());
  controller_->SetDimmedForInactivity(true);
  EXPECT_EQ(chromeos::DISPLAY_POWER_INTERNAL_OFF_EXTERNAL_ON,
            display_power_setter_.state());
  EXPECT_EQ(0, backlight_.current_level());

  // The external display should still be turned off for inactivity.
  controller_->SetOffForInactivity(true);
  EXPECT_EQ(chromeos::DISPLAY_POWER_ALL_OFF, display_power_setter_.state());
  EXPECT_EQ(0, backlight_.current_level());
  controller_->SetOffForInactivity(false);
  EXPECT_EQ(chromeos::DISPLAY_POWER_INTERNAL_OFF_EXTERNAL_ON,
            display_power_setter_.state());
  EXPECT_EQ(0, backlight_.current_level());

  // After exiting docked mode, the internal display should be turned back on
  // and the backlight should be at the dimmed level.
  controller_->SetDocked(false);
  EXPECT_EQ(chromeos::DISPLAY_POWER_ALL_ON, display_power_setter_.state());
  EXPECT_EQ(0, display_power_setter_.delay().InMilliseconds());
  EXPECT_GT(backlight_.current_level(), 0);
  EXPECT_LT(backlight_.current_level(), initial_level);
}

TEST_F(InternalBacklightControllerTest, GiveUpOnBrokenAmbientLightSensor) {
  // Don't report any ambient light readings. As before, the controller
  // should avoid changing the backlight from its initial brightness.
  init_time_ = base::TimeTicks::FromInternalValue(1000);
  report_initial_als_reading_ = false;
  Init(POWER_AC);
  EXPECT_EQ(initial_backlight_level_, backlight_.current_level());
  controller_->HandlePowerSourceChange(POWER_AC);
  EXPECT_EQ(initial_backlight_level_, backlight_.current_level());

  // After the timeout has elapsed, state changes (like dimming due to
  // inactivity) should be honored.
  const base::TimeTicks kUpdateTime = init_time_ + base::TimeDelta::FromSeconds(
      InternalBacklightController::kAmbientLightSensorTimeoutSec);
  controller_->clock()->set_current_time_for_testing(kUpdateTime);
  controller_->SetDimmedForInactivity(true);
  EXPECT_LT(backlight_.current_level(), initial_backlight_level_);
}

TEST_F(InternalBacklightControllerTest, UserAdjustmentBeforeAmbientLight) {
  report_initial_als_reading_ = false;
  Init(POWER_AC);
  ASSERT_EQ(initial_backlight_level_, backlight_.current_level());

  // Check that a decrease request actually decreases the brightness (i.e. the
  // initial backlight level, rather than 100%, is used as the starting point
  // when the ambient light level hasn't been received yet). See
  // http://crosbug.com/p/22380.
  controller_->DecreaseUserBrightness(true);
  EXPECT_LT(backlight_.current_level(), initial_backlight_level_);
}

TEST_F(InternalBacklightControllerTest,
       DockedNotificationReceivedBeforeAmbientLight) {
  // Send a docked-mode request before the first ambient light reading.
  report_initial_als_reading_ = false;
  Init(POWER_AC);
  controller_->SetDocked(true);
  EXPECT_EQ(initial_backlight_level_, backlight_.current_level());

  // Check that the system goes into docked mode after an ambient light
  // reading is received.
  light_sensor_.NotifyObservers();
  EXPECT_EQ(0, backlight_.current_level());
  EXPECT_EQ(chromeos::DISPLAY_POWER_INTERNAL_OFF_EXTERNAL_ON,
            display_power_setter_.state());
  EXPECT_EQ(0, display_power_setter_.delay().InMilliseconds());
}

TEST_F(InternalBacklightControllerTest, BrightnessPolicy) {
  default_als_steps_ = "50.0 -1 200\n90.0 100 -1";
  Init(POWER_AC);
  ASSERT_EQ(PercentToLevel(50.0), backlight_.current_level());

  BacklightControllerObserverStub observer;
  controller_->AddObserver(&observer);

  // When an AC brightness policy is sent while on AC power, the brightness
  // should change immediately.
  PowerManagementPolicy policy;
  policy.set_ac_brightness_percent(75.0);
  controller_->HandlePolicyChange(policy);
  EXPECT_EQ(PercentToLevel(75.0), backlight_.current_level());
  EXPECT_EQ(kFastBacklightTransitionMs,
            backlight_.current_interval().InMilliseconds());
  ASSERT_EQ(static_cast<size_t>(1), observer.changes().size());
  EXPECT_EQ(BacklightController::BRIGHTNESS_CHANGE_AUTOMATED,
            observer.changes()[0].cause);
  EXPECT_EQ(controller_.get(), observer.changes()[0].source);

  // Passing a battery policy while on AC shouldn't do anything.
  observer.Clear();
  policy.Clear();
  policy.set_battery_brightness_percent(43.0);
  controller_->HandlePolicyChange(policy);
  EXPECT_EQ(PercentToLevel(75.0), backlight_.current_level());
  EXPECT_EQ(static_cast<size_t>(0), observer.changes().size());

  // The previously-set brightness should be used after switching to battery.
  observer.Clear();
  controller_->HandlePowerSourceChange(POWER_BATTERY);
  EXPECT_EQ(PercentToLevel(43.0), backlight_.current_level());
  ASSERT_EQ(static_cast<size_t>(1), observer.changes().size());
  EXPECT_EQ(BacklightController::BRIGHTNESS_CHANGE_AUTOMATED,
            observer.changes()[0].cause);
  EXPECT_EQ(controller_.get(), observer.changes()[0].source);

  // An empty policy shouldn't do anything.
  observer.Clear();
  policy.Clear();
  controller_->HandlePolicyChange(policy);
  EXPECT_EQ(PercentToLevel(43.0), backlight_.current_level());
  EXPECT_EQ(static_cast<size_t>(0), observer.changes().size());

  // Ambient light readings shouldn't affect the brightness after it's been set
  // via a policy.
  observer.Clear();
  light_sensor_.set_lux(400);
  for (int i = 0; i < kAlsSamplesToTriggerAdjustment; ++i)
    light_sensor_.NotifyObservers();
  EXPECT_EQ(PercentToLevel(43.0), backlight_.current_level());
  EXPECT_EQ(static_cast<size_t>(0), observer.changes().size());

  // Set the brightness to 0% and check that the actions that usually turn the
  // screen back on don't do anything.
  policy.Clear();
  policy.set_battery_brightness_percent(0.0);
  controller_->HandlePolicyChange(policy);
  EXPECT_EQ(0, backlight_.current_level());
  controller_->HandleSessionStateChange(SESSION_STARTED);
  controller_->HandlePowerButtonPress();
  controller_->HandleUserActivity(USER_ACTIVITY_OTHER);

  // After sending an empty policy, user activity should increase the
  // brightness from 0%.
  policy.Clear();
  controller_->HandlePolicyChange(policy);
  EXPECT_EQ(0, backlight_.current_level());
  controller_->HandleUserActivity(USER_ACTIVITY_OTHER);
  EXPECT_GT(backlight_.current_level(), 0);

  // After a policy sets the brightness to 0%, the brightness keys should still
  // work, and after user-triggered adjustments are made, hitting the power
  // button should turn the backlight on.
  policy.Clear();
  policy.set_battery_brightness_percent(0.0);
  controller_->HandlePolicyChange(policy);
  EXPECT_EQ(0, backlight_.current_level());
  controller_->IncreaseUserBrightness();
  EXPECT_GT(backlight_.current_level(), 0);
  controller_->DecreaseUserBrightness(true);
  EXPECT_EQ(0, backlight_.current_level());
  controller_->HandlePowerButtonPress();
  EXPECT_GT(backlight_.current_level(), 0);

  controller_->RemoveObserver(&observer);
}

TEST_F(InternalBacklightControllerTest, SetDisplayPowerOnChromeStart) {
  // Init() shouldn't ask Chrome to turn all displays on (maybe Chrome hasn't
  // started yet).
  Init(POWER_AC);
  EXPECT_EQ(0, display_power_setter_.num_power_calls());

  // After Chrome starts, the controller should turn the displays on.
  display_power_setter_.reset_num_power_calls();
  controller_->HandleChromeStart();
  EXPECT_EQ(1, display_power_setter_.num_power_calls());
  EXPECT_EQ(chromeos::DISPLAY_POWER_ALL_ON, display_power_setter_.state());
  EXPECT_EQ(0, display_power_setter_.delay().InMilliseconds());

  // Enter docked mode.
  controller_->SetDocked(true);
  ASSERT_EQ(chromeos::DISPLAY_POWER_INTERNAL_OFF_EXTERNAL_ON,
            display_power_setter_.state());

  // If Chrome restarts, the controller should notify it about the current power
  // state.
  display_power_setter_.reset_num_power_calls();
  controller_->HandleChromeStart();
  EXPECT_EQ(1, display_power_setter_.num_power_calls());
  EXPECT_EQ(chromeos::DISPLAY_POWER_INTERNAL_OFF_EXTERNAL_ON,
            display_power_setter_.state());
}

}  // namespace policy
}  // namespace power_manager
