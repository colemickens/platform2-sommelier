// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>
#include <string>

#include "base/memory/scoped_ptr.h"
#include "power_manager/common/mock_backlight.h"
#include "power_manager/common/mock_power_prefs_interface.h"
#include "power_manager/common/power_constants.h"
#include "power_manager/powerd/backlight_controller.h"
#include "power_manager/powerd/keyboard_backlight_controller.h"
#include "power_manager/powerd/mock_ambient_light_sensor.h"
#include "power_manager/powerd/mock_backlight_controller_observer.h"

using ::testing::Mock;
using ::testing::StrictMock;
using ::testing::Test;

namespace power_manager {
const double kTestPercentDim = 20.0;
const double kTestPercentMax = 75.0;
const double kTestPercentMin = 0.0;
const int kTestLevelDim = 20;
const int kTestLevelMax = 75;
const int kTestLevelMin = 0;
const std::string kTestStepsString = "20.0 -1 50\n50.0 35 75\n75.0 60 -1";
const double kTestLevels[] = { 20.0, 50.0, 75.0 };
const int kTestDecreaseThresholds[] = { -1, 35, 60 };
const int kTestIncreaseThresholds[] = { 50, 75, -1 };
const int kTestStepIndex = 1;
const int kTestSyntheticLux = 55;
const int kTestDecreaseLux = 20;
const int kTestIncreaseLux = 80;
const int64 kTestCurrentLevel = kTestLevels[kTestStepIndex];
const double kTestCurrentPercent = kTestLevels[kTestStepIndex];

const int64 kTestBrightnessMaxLevel = 100;
const double kTestBrightnessMaxPercent = 100.0;
const int64 kTestTimeoutIntervalMs = 10000;  // Intentionally larger then the
                                             // timeout in the class.
class KeyboardBacklightControllerTest : public Test {
 public:
  KeyboardBacklightControllerTest() { }

  virtual void SetUp() {
    light_sensor_.ExpectAddObserver();
    controller_.reset(new KeyboardBacklightController(&backlight_,
                                                      &power_prefs_,
                                                      &light_sensor_));
    controller_->user_enabled_ = true;
    controller_->state_ = BACKLIGHT_ACTIVE;
    controller_->max_level_ = kTestBrightnessMaxLevel;

    controller_->target_percent_ = 0.0;
    backlight_.ExpectGetMaxBrightnessLevel(kTestBrightnessMaxLevel, true);
    backlight_.ExpectGetCurrentBrightnessLevel(kTestCurrentLevel, true);
    power_prefs_.ExpectGetDouble(kKeyboardBacklightDimPercentPref,
                                 kTestPercentDim, true);
    power_prefs_.ExpectGetDouble(kKeyboardBacklightMaxPercentPref,
                                 kTestPercentMax, true);
    power_prefs_.ExpectGetDouble(kKeyboardBacklightMinPercentPref,
                                 kTestPercentMin, true);
    power_prefs_.ExpectGetString(kKeyboardBacklightStepsPref,
                                 kTestStepsString, true);
    ASSERT_TRUE(controller_->Init());
  }

  virtual void TearDown() {
    if (controller_->light_sensor_ != NULL)
      light_sensor_.ExpectRemoveObserver(controller_.get());
    controller_.reset();
  }

  void SetControllerState(int64 current_level,
                          double target_percent,
                          bool user_enabled,
                          bool video_enabled,
                          BacklightControllerObserver* observer,
                          int user_adjustments) {
    controller_->current_level_ = current_level;
    controller_->target_percent_ = target_percent;
    controller_->user_enabled_ = user_enabled;
    controller_->video_enabled_ = video_enabled;
    controller_->observer_ = observer;
    controller_->num_user_adjustments_ = user_adjustments;
  }

  void CheckCurrentBrightnessPercent(int level,
                                     double percent,
                                     int num_user_adjustments) {
    EXPECT_EQ(level, controller_->current_level_);
    EXPECT_EQ(percent, controller_->target_percent_);
    EXPECT_EQ(num_user_adjustments, controller_->num_user_adjustments_);

  }

  void CheckCurrentStep(int index, int lux) {
    EXPECT_EQ(index, controller_->current_step_index_);
    EXPECT_EQ(kTestLevels[index], controller_->target_percent_);
    EXPECT_EQ(lux, controller_->lux_level_);
  }

  void CheckHysteresisState(BacklightController::AlsHysteresisState state,
                            int hysteresis_count) {
    EXPECT_EQ(state, controller_->hysteresis_state_);
    EXPECT_EQ(hysteresis_count, controller_->hysteresis_count_);
  }

  SIGNAL_CALLBACK_0(KeyboardBacklightControllerTest, gboolean, TestTimeout);

 protected:
  StrictMock<MockAmbientLightSensor> light_sensor_;
  StrictMock<MockBacklight> backlight_;
  StrictMock<MockPowerPrefsInterface> power_prefs_;
  scoped_ptr<KeyboardBacklightController> controller_;
};  // class KeyboardBacklightControllerTest

gboolean KeyboardBacklightControllerTest::TestTimeout() {
  return false;
}

TEST_F(KeyboardBacklightControllerTest, Init) {
  light_sensor_.ExpectRemoveObserver(controller_.get());
  controller_.reset();
  Mock::VerifyAndClearExpectations(&backlight_);

  light_sensor_.ExpectAddObserver();
  controller_.reset(new KeyboardBacklightController(&backlight_,
                                                    &power_prefs_,
                                                    &light_sensor_));


  // GetMaxBrightnessLevel fails.
  controller_->target_percent_ = 0.0;
  backlight_.ExpectGetMaxBrightnessLevel(kTestBrightnessMaxLevel, false);
  EXPECT_FALSE(controller_->Init());
  EXPECT_EQ(0.0, controller_->target_percent_);
  Mock::VerifyAndClearExpectations(&backlight_);

  // GetCurrentBrightnessLevel fails.
  controller_->target_percent_ = 0.0;
  backlight_.ExpectGetMaxBrightnessLevel(kTestBrightnessMaxLevel, true);
  backlight_.ExpectGetCurrentBrightnessLevel(kTestCurrentLevel, false);
  EXPECT_FALSE(controller_->Init());
  EXPECT_EQ(0.0, controller_->target_percent_);
  Mock::VerifyAndClearExpectations(&backlight_);

  // Operation succeeds.
  controller_->target_percent_ = 0.0;
  backlight_.ExpectGetMaxBrightnessLevel(kTestBrightnessMaxLevel, true);
  backlight_.ExpectGetCurrentBrightnessLevel(kTestCurrentLevel, true);
  power_prefs_.ExpectGetDouble(kKeyboardBacklightDimPercentPref,
                               kTestPercentDim, true);
  power_prefs_.ExpectGetDouble(kKeyboardBacklightMaxPercentPref,
                               kTestPercentMax, true);
  power_prefs_.ExpectGetDouble(kKeyboardBacklightMinPercentPref,
                               kTestPercentMin, true);
  power_prefs_.ExpectGetString(kKeyboardBacklightStepsPref,
                               kTestStepsString, true);
  EXPECT_TRUE(controller_->Init());
  CheckCurrentStep(kTestStepIndex, kTestSyntheticLux);
  for(unsigned int i = 0; i < controller_->brightness_steps_.size(); i++) {
    EXPECT_EQ(kTestLevels[i], controller_->brightness_steps_[i].target_percent);
    EXPECT_EQ(kTestDecreaseThresholds[i],
              controller_->brightness_steps_[i].decrease_threshold);
    EXPECT_EQ(kTestIncreaseThresholds[i],
              controller_->brightness_steps_[i].increase_threshold);
  }
}

TEST_F(KeyboardBacklightControllerTest, GetCurrentBrightnessPercent) {
  double percent;
  // |max_level_| is uninitialized.
  controller_->max_level_ = 0;
  controller_->current_level_ = kTestCurrentLevel;
  percent = 0.0;
  EXPECT_FALSE(controller_->GetCurrentBrightnessPercent(&percent));
  EXPECT_EQ(-1.0, percent);

  // Operation succeeds.
  controller_->max_level_ = kTestBrightnessMaxLevel;
  controller_->current_level_ = kTestCurrentLevel;
  percent = 0.0;
  EXPECT_TRUE(controller_->GetCurrentBrightnessPercent(&percent));
  EXPECT_EQ(kTestCurrentPercent, percent);
}

TEST_F(KeyboardBacklightControllerTest, SetCurrentBrightnessPercent) {
  // Cause is user initiated.
  SetControllerState(kTestLevelDim, kTestPercentDim,
                                      true, true, NULL, 0);
  backlight_.ExpectSetBrightnessLevel(kTestCurrentLevel, true);
  EXPECT_TRUE(controller_->SetCurrentBrightnessPercent(
      kTestCurrentPercent,
      BRIGHTNESS_CHANGE_USER_INITIATED,
      TRANSITION_INSTANT));
  Mock::VerifyAndClearExpectations(&backlight_);

  // User has turned off the backlight.
  SetControllerState(kTestLevelDim, kTestPercentDim,
                                      false, true, NULL, 0);
  backlight_.ExpectSetBrightnessLevel(kTestLevelMin, true);
  EXPECT_TRUE(controller_->SetCurrentBrightnessPercent(
      kTestCurrentPercent,
      BRIGHTNESS_CHANGE_USER_INITIATED,
      TRANSITION_INSTANT));
  CheckCurrentBrightnessPercent(kTestLevelMin, kTestCurrentPercent, 1);
  Mock::VerifyAndClearExpectations(&backlight_);

  // Observer present.
  MockBacklightControllerObserver observer;
  ASSERT_EQ(observer.changes().size(), 0);
  SetControllerState(kTestLevelMin, kTestPercentMin,
                                      true, true, &observer, 0);
  backlight_.ExpectSetBrightnessLevel(kTestCurrentLevel, true);
  EXPECT_TRUE(controller_->SetCurrentBrightnessPercent(
      kTestCurrentPercent,
      BRIGHTNESS_CHANGE_USER_INITIATED,
      TRANSITION_INSTANT));
  CheckCurrentBrightnessPercent(kTestCurrentLevel, kTestCurrentPercent,
                                      1);
  EXPECT_EQ(1, observer.changes().size());
}

TEST_F(KeyboardBacklightControllerTest, OnVideoDetectorEvent) {
  // Interval already expired.
  controller_->is_fullscreen_ = true;
  controller_->is_video_playing_ = true;
  controller_->OnVideoDetectorEvent(
      base::TimeTicks::Now()
      - base::TimeDelta::FromMilliseconds(kTestTimeoutIntervalMs),
      false);
  EXPECT_FALSE(controller_->is_fullscreen_);
  EXPECT_FALSE(controller_->is_video_playing_);

  // Interval is valid.
  controller_->is_fullscreen_ = false;
  controller_->is_video_playing_ = false;
  controller_->video_enabled_ = true;
  controller_->observer_ = NULL;
  controller_->current_level_ = kTestCurrentLevel;
  controller_->target_percent_ = kTestCurrentPercent;
  backlight_.ExpectSetBrightnessLevel(0, true);
  controller_->OnVideoDetectorEvent(base::TimeTicks::Now(), true);
  EXPECT_TRUE(controller_->is_fullscreen_);
  EXPECT_TRUE(controller_->is_video_playing_);
  EXPECT_FALSE(controller_->video_enabled_);
  EXPECT_EQ(0, controller_->current_level_);
  EXPECT_EQ(kTestCurrentPercent, controller_->target_percent_);
  ASSERT_GT(controller_->video_timeout_timer_id_, 0);
  g_source_remove(controller_->video_timeout_timer_id_);
}

TEST_F(KeyboardBacklightControllerTest, UpdateBacklightEnabled) {
  // No change in state.
  controller_->is_video_playing_ = false;
  controller_->is_fullscreen_ = false;
  controller_->video_enabled_ = true;
  controller_->UpdateBacklightEnabled();
  EXPECT_TRUE(controller_->video_enabled_);

  controller_->is_video_playing_ = true;
  controller_->is_fullscreen_ = true;
  controller_->video_enabled_ = false;
  controller_->UpdateBacklightEnabled();
  EXPECT_FALSE(controller_->video_enabled_);

  controller_->target_percent_ = kTestCurrentPercent;
  // Enabling the backlight due to video not playing.
  controller_->current_level_ = 0;
  controller_->is_video_playing_ = false;
  controller_->is_fullscreen_ = true;
  controller_->video_enabled_ = false;
  backlight_.ExpectSetBrightnessLevel(kTestCurrentLevel, true);
  controller_->UpdateBacklightEnabled();
  EXPECT_TRUE(controller_->video_enabled_);
  EXPECT_EQ(kTestCurrentLevel, controller_->current_level_);
  Mock::VerifyAndClearExpectations(&backlight_);

  // Enabling the backlight due to not fullscreen.
  controller_->current_level_ = 0;
  controller_->is_video_playing_ = true;
  controller_->is_fullscreen_ = false;
  controller_->video_enabled_ = false;
  backlight_.ExpectSetBrightnessLevel(kTestCurrentLevel, true);
  controller_->UpdateBacklightEnabled();
  EXPECT_TRUE(controller_->video_enabled_);
  EXPECT_EQ(kTestCurrentLevel, controller_->current_level_);
  Mock::VerifyAndClearExpectations(&backlight_);

  // Disabling the backlight.
  controller_->current_level_ = kTestCurrentLevel;
  controller_->is_video_playing_ = true;
  controller_->is_fullscreen_ = true;
  controller_->video_enabled_ = true;
  backlight_.ExpectSetBrightnessLevel(0, true);
  controller_->UpdateBacklightEnabled();
  EXPECT_FALSE(controller_->video_enabled_);
  EXPECT_EQ(0, controller_->current_level_);
}

TEST_F(KeyboardBacklightControllerTest, WriteBrightnessLevel) {
  // Operation succeeds.
  controller_->current_level_ = 0;
  backlight_.ExpectSetBrightnessLevel(kTestCurrentLevel, true);
  controller_->WriteBrightnessLevel(kTestCurrentLevel);
  EXPECT_EQ(kTestCurrentLevel, controller_->current_level_);
  Mock::VerifyAndClearExpectations(&backlight_);
}

TEST_F(KeyboardBacklightControllerTest, HaltVideoTimeout) {
  controller_->video_timeout_timer_id_ = g_timeout_add(kTestTimeoutIntervalMs,
                                                       TestTimeoutThunk,
                                                       this);
  ASSERT_GT(controller_->video_timeout_timer_id_, 0);
  controller_->HaltVideoTimeout();
  EXPECT_EQ(0, controller_->video_timeout_timer_id_);
}

TEST_F(KeyboardBacklightControllerTest, VideoTimeout) {
  // Timeout causes transition from disabled to enabled.
  controller_->target_percent_ = kTestCurrentPercent;
  controller_->current_level_ = 0;
  controller_->is_video_playing_ = true;
  controller_->is_fullscreen_ = true;
  controller_->video_enabled_ = false;
  backlight_.ExpectSetBrightnessLevel(kTestCurrentLevel, true);
  EXPECT_FALSE(controller_->VideoTimeout());
  EXPECT_FALSE(controller_->is_video_playing_);
  EXPECT_TRUE(controller_->video_enabled_);
  EXPECT_EQ(kTestCurrentLevel, controller_->current_level_);
  Mock::VerifyAndClearExpectations(&backlight_);

  // Timeout doesn't cause causes transition.
  controller_->target_percent_ = kTestCurrentPercent;
  controller_->current_level_ = kTestCurrentLevel;
  controller_->is_video_playing_ = true;
  controller_->is_fullscreen_ = false;
  controller_->video_enabled_ = true;
  EXPECT_FALSE(controller_->VideoTimeout());
  EXPECT_FALSE(controller_->is_video_playing_);
  EXPECT_TRUE(controller_->video_enabled_);
}

TEST_F(KeyboardBacklightControllerTest, LevelToPercent) {
  // |max_level_| is uninitialized.
  controller_->max_level_ = 0;
  EXPECT_EQ(-1.0, controller_->LevelToPercent(kTestCurrentLevel));

  // |max_level_| is initialized.
  controller_->max_level_ = kTestBrightnessMaxLevel;
  EXPECT_EQ(kTestCurrentPercent,
            controller_->LevelToPercent(kTestCurrentLevel));

  // |level| is too high.
  EXPECT_EQ(controller_->LevelToPercent(kTestCurrentLevel +
                                        kTestBrightnessMaxLevel),
            kTestBrightnessMaxPercent);

  // |level| is too low.
  EXPECT_EQ(0.0,
            controller_->LevelToPercent(kTestCurrentLevel -
                                        kTestBrightnessMaxLevel));
}

TEST_F(KeyboardBacklightControllerTest, PercentToLevel) {
  // |max_level_| is uninitialized.
  controller_->max_level_ = 0;
  EXPECT_EQ(-1, controller_->PercentToLevel(kTestCurrentPercent));

  // |max_level_| is initialized.
  controller_->max_level_ = kTestBrightnessMaxLevel;
  EXPECT_EQ(kTestCurrentLevel,
            controller_->PercentToLevel(kTestCurrentPercent));

  // |percent| is too high.
  EXPECT_EQ(kTestBrightnessMaxLevel,
            controller_->PercentToLevel(kTestCurrentPercent + 100.0));

  // |percent| is too low.
  EXPECT_EQ(0, controller_->PercentToLevel(kTestCurrentPercent - 100.0));
}

TEST_F(KeyboardBacklightControllerTest, OnAmbientLightChanged) {
  controller_->observer_ = NULL;
  controller_->video_enabled_ = true;

  // Incorrect sensor.
  controller_->OnAmbientLightChanged(NULL);
  CheckCurrentStep(kTestStepIndex, kTestSyntheticLux);
  CheckHysteresisState(BacklightController::ALS_HYST_IDLE, 0);

  // ALS returns bad value.
  light_sensor_.ExpectGetAmbientLightLux(-1);
  controller_->OnAmbientLightChanged(&light_sensor_);
  CheckCurrentStep(kTestStepIndex, kTestSyntheticLux);
  CheckHysteresisState(BacklightController::ALS_HYST_IDLE, 0);
  Mock::VerifyAndClearExpectations(&light_sensor_);

  // ALS returns the already set value.
  light_sensor_.ExpectGetAmbientLightLux(controller_->lux_level_);
  controller_->OnAmbientLightChanged(&light_sensor_);
  CheckCurrentStep(kTestStepIndex, kTestSyntheticLux);
  CheckHysteresisState(BacklightController::ALS_HYST_IDLE, 0);
  Mock::VerifyAndClearExpectations(&light_sensor_);

  // First Increase, hysteresis not overcome.
  light_sensor_.ExpectGetAmbientLightLux(kTestIncreaseLux);
  controller_->OnAmbientLightChanged(&light_sensor_);
  CheckCurrentStep(kTestStepIndex, kTestSyntheticLux);
  CheckHysteresisState(BacklightController::ALS_HYST_UP, 1);
  Mock::VerifyAndClearExpectations(&light_sensor_);

  // Second Increase, hysteresis overcome.
  light_sensor_.ExpectGetAmbientLightLux(kTestIncreaseLux);
  backlight_.ExpectSetBrightnessLevel(kTestLevels[kTestStepIndex + 1], true);
  controller_->OnAmbientLightChanged(&light_sensor_);
  CheckCurrentStep(kTestStepIndex + 1, kTestIncreaseLux);
  CheckHysteresisState(BacklightController::ALS_HYST_UP, 1);
  Mock::VerifyAndClearExpectations(&light_sensor_);
  Mock::VerifyAndClearExpectations(&backlight_);

  // Reset for testing the other direction.
  controller_->current_step_index_ = kTestStepIndex;
  controller_->target_percent_ = kTestLevels[kTestStepIndex];
  controller_->lux_level_ = kTestSyntheticLux;
  controller_->hysteresis_state_ = BacklightController::ALS_HYST_IDLE;
  controller_->hysteresis_count_ = 0;

  // First Decrease, hysteresis not overcome.
  light_sensor_.ExpectGetAmbientLightLux(kTestDecreaseLux);
  controller_->OnAmbientLightChanged(&light_sensor_);
  CheckCurrentStep(kTestStepIndex, kTestSyntheticLux);
  CheckHysteresisState(BacklightController::ALS_HYST_DOWN, 1);
  Mock::VerifyAndClearExpectations(&light_sensor_);

  // Second Decrease, hysteresis overcome.
  light_sensor_.ExpectGetAmbientLightLux(kTestDecreaseLux);
  backlight_.ExpectSetBrightnessLevel(kTestLevels[kTestStepIndex - 1], true);
  controller_->OnAmbientLightChanged(&light_sensor_);
  CheckCurrentStep(kTestStepIndex - 1, kTestDecreaseLux);
  CheckHysteresisState(BacklightController::ALS_HYST_DOWN, 1);
  Mock::VerifyAndClearExpectations(&light_sensor_);
  Mock::VerifyAndClearExpectations(&backlight_);
}

}  // namespace power_manager
