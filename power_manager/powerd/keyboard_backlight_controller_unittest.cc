// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include <cmath>
#include <string>

#include "base/memory/scoped_ptr.h"
#include "power_manager/common/fake_prefs.h"
#include "power_manager/common/power_constants.h"
#include "power_manager/common/util.h"
#include "power_manager/powerd/backlight_controller.h"
#include "power_manager/powerd/keyboard_backlight_controller.h"
#include "power_manager/powerd/mock_backlight_controller_observer.h"
#include "power_manager/powerd/system/ambient_light_sensor_stub.h"
#include "power_manager/powerd/system/backlight_stub.h"

namespace power_manager {

namespace {

const std::string kTestAlsLimitsString = "0.0\n20.0\n75.0";
const double kTestAlsPercentDim = 20.0;
const double kTestAlsPercentMax = 75.0;
const double kTestAlsPercentMin = 0.0;
const int64 kTestAlsLevelDim = 20;
const int64 kTestAlsLevelMax = 75;
const int64 kTestAlsLevelMin = 0;
const size_t kTestNumAlsSteps = 3;
const std::string kTestAlsStepsString = "20.0 -1 50\n50.0 35 75\n75.0 60 -1";
const double kTestAlsPercents[] = { 20.0, 50.0, 75.0 };
const int64 kTestAlsDecreaseThresholds[] = { -1, 35, 60 };
const int64 kTestAlsIncreaseThresholds[] = { 50, 75, -1 };
const int64 kTestAlsLevels[] = { 20, 50, 75 };
const size_t kDefaultNumAlsSteps = 1;
const double kDefaultAlsPercent = 75.0;
const int64 kDefaultAlsDecreaseThreshold = -1;
const int64 kDefaultAlsIncreaseThreshold = -1;
const std::string kTestUserLimitsString = "0.0\n10.0\n100.0";
const double kTestUserPercentDim = 10.0;
const double kTestUserPercentMax = 100.0;
const double kTestUserPercentMin = 0.0;
const int64 kTestUserLevelDim = 10;
const int64 kTestUserLevelMax = 100;
const int64 kTestUserLevelMin = 0;
const int64 kTestNumUserSteps = 5;
const std::string kTestUserStepsString = "0.0\n10.0\n40.0\n60.0\n100.0";
const double kTestUserPercents[] = { 0.0, 10.0, 40.0, 60.0, 100.0 };
const int64 kTestUserLevels[] = { 0, 10, 40, 60, 100 };
const size_t kDefaultNumUserSteps = 3;
const double kDefaultUserPercents[] = { kTestUserPercentMin,
                                        kTestUserPercentDim,
                                        kTestUserPercentMax };
const size_t kTestFoundUserStepIndex = 2;
const size_t kTestStepIndex = 1;
const int64 kTestSyntheticLux = 55;
const int64 kTestDecreaseLux = 20;
const int64 kTestIncreaseLux = 80;
const int64 kTestCurrentLevel = kTestAlsLevels[kTestStepIndex];
const double kTestCurrentPercent = kTestAlsPercents[kTestStepIndex];
const int64 kTestBrightnessMaxLevel = 100;
const double kTestBrightnessMaxPercent = 100.0;
const int64 kTestTimeoutIntervalMs = 10000;  // Intentionally larger then the
                                             // timeout in the class.

// Initial ambient light levels.
const double kInitialAmbientLightPercent = 0.0;
const int kInitialAmbientLightLux = 0;

}  // namespace

class KeyboardBacklightControllerTest : public ::testing::Test {
 public:
  KeyboardBacklightControllerTest()
      : light_sensor_(kInitialAmbientLightPercent, kInitialAmbientLightLux),
        backlight_(kTestBrightnessMaxLevel, kTestCurrentLevel) {
  }

  virtual void SetUp() {
    controller_.reset(new KeyboardBacklightController(&backlight_,
                                                      &prefs_,
                                                      &light_sensor_));
    controller_->state_ = BACKLIGHT_ACTIVE;
    controller_->max_level_ = kTestBrightnessMaxLevel;

    controller_->als_target_percent_ = 0.0;
    prefs_.SetString(kKeyboardBacklightAlsLimitsPref, kTestAlsLimitsString);
    prefs_.SetString(kKeyboardBacklightAlsStepsPref, kTestAlsStepsString);
    prefs_.SetString(kKeyboardBacklightUserLimitsPref, kTestUserLimitsString);
    prefs_.SetString(kKeyboardBacklightUserStepsPref, kTestUserStepsString);
    prefs_.SetInt64(kDisableALSPref, 0);
    ASSERT_TRUE(controller_->Init());
  }

  virtual void TearDown() {
    controller_.reset();
  }

  void SetControllerState(int64 current_level,
                          double als_target_percent,
                          double user_target_percent,
                          size_t user_step_index,
                          bool video_enabled,
                          BacklightControllerObserver* observer) {
    controller_->current_level_ = current_level;
    controller_->als_target_percent_ = als_target_percent;
    controller_->user_target_percent_ = user_target_percent;
    controller_->user_step_index_ = user_step_index;
    controller_->video_enabled_ = video_enabled;
    controller_->observer_ = observer;
  }

  void CheckCurrentBrightnessPercent(int level,
                                     double als_percent,
                                     double user_percent) {
    EXPECT_EQ(level, controller_->current_level_);
    EXPECT_EQ(als_percent, controller_->als_target_percent_);
    EXPECT_EQ(user_percent, controller_->user_target_percent_);
  }

  void CheckAlsStep(int index, int lux) {
    EXPECT_EQ(index, controller_->als_step_index_);
    EXPECT_EQ(kTestAlsPercents[index], controller_->als_target_percent_);
    EXPECT_EQ(lux, controller_->lux_level_);
  }

  void CheckHysteresisState(
      KeyboardBacklightController::AlsHysteresisState state,
      int hysteresis_count) {
    EXPECT_EQ(state, controller_->hysteresis_state_);
    EXPECT_EQ(hysteresis_count, controller_->hysteresis_count_);
  }

  SIGNAL_CALLBACK_0(KeyboardBacklightControllerTest, gboolean, TestTimeout);

 protected:
  system::AmbientLightSensorStub light_sensor_;
  system::BacklightStub backlight_;
  FakePrefs prefs_;
  scoped_ptr<KeyboardBacklightController> controller_;
};  // class KeyboardBacklightControllerTest

gboolean KeyboardBacklightControllerTest::TestTimeout() {
  return false;
}

TEST_F(KeyboardBacklightControllerTest, Init) {
  controller_.reset();
  controller_.reset(new KeyboardBacklightController(&backlight_,
                                                    &prefs_,
                                                    &light_sensor_));

  // GetMaxBrightnessLevel and GetCurrentBrightnessLevel fail.
  controller_->als_target_percent_ = 0.0;
  backlight_.set_should_fail(true);
  EXPECT_FALSE(controller_->Init());
  EXPECT_EQ(0.0, controller_->als_target_percent_);
  backlight_.set_should_fail(false);

  // Operation succeeds.
  controller_->als_target_percent_ = 0.0;
  EXPECT_TRUE(controller_->Init());
  CheckAlsStep(kTestStepIndex, kTestSyntheticLux);
  for(unsigned int i = 0; i < controller_->als_steps_.size(); i++) {
    EXPECT_EQ(kTestAlsPercents[i], controller_->als_steps_[i].target_percent);
    EXPECT_EQ(kTestAlsDecreaseThresholds[i],
              controller_->als_steps_[i].decrease_threshold);
    EXPECT_EQ(kTestAlsIncreaseThresholds[i],
              controller_->als_steps_[i].increase_threshold);
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
  controller_->state_ = BACKLIGHT_ACTIVE;
  // Cause is automated.
  SetControllerState(kTestAlsLevelDim, kTestAlsPercentDim,
                     -1.0, -1, true, NULL);
  EXPECT_TRUE(controller_->SetCurrentBrightnessPercent(
      kTestCurrentPercent,
      BRIGHTNESS_CHANGE_AUTOMATED,
      TRANSITION_INSTANT));
  CheckCurrentBrightnessPercent(kTestCurrentLevel,
                                kTestCurrentPercent,
                                -1.0);
  EXPECT_EQ(kTestCurrentLevel, backlight_.current_level());

  // Call causes no change in brightness.
  SetControllerState(kTestAlsLevelDim, kTestAlsPercentDim,
                     -1.0, -1, true, NULL);
  EXPECT_FALSE(controller_->SetCurrentBrightnessPercent(
      kTestAlsPercentDim,
      BRIGHTNESS_CHANGE_AUTOMATED,
      TRANSITION_INSTANT));
  CheckCurrentBrightnessPercent(kTestAlsLevelDim,
                                kTestAlsPercentDim,
                                -1.0);
  EXPECT_EQ(kTestCurrentLevel, backlight_.current_level());

  // Cause is user initiated, but user index not initialized.
  SetControllerState(kTestAlsLevelDim, kTestAlsPercentDim,
                     -1.0, -1, true, NULL);
  EXPECT_FALSE(controller_->SetCurrentBrightnessPercent(
      kTestUserPercents[kTestFoundUserStepIndex],
      BRIGHTNESS_CHANGE_USER_INITIATED,
      TRANSITION_INSTANT));
  CheckCurrentBrightnessPercent(kTestAlsLevelDim,
                                kTestAlsPercentDim,
                                -1.0);

  // Cause is user initiated.
  SetControllerState(kTestUserLevelDim, kTestAlsPercentDim,
                     kTestUserPercents[kTestFoundUserStepIndex - 1],
                     kTestFoundUserStepIndex, true, NULL);
  EXPECT_TRUE(controller_->SetCurrentBrightnessPercent(
      kTestUserPercents[kTestFoundUserStepIndex],
      BRIGHTNESS_CHANGE_USER_INITIATED,
      TRANSITION_INSTANT));
  CheckCurrentBrightnessPercent(kTestUserLevels[kTestFoundUserStepIndex],
                                kTestAlsPercentDim,
                                kTestUserPercents[kTestFoundUserStepIndex]);
  EXPECT_EQ(kTestUserLevels[kTestFoundUserStepIndex],
            backlight_.current_level());

  // Observer present.
  MockBacklightControllerObserver observer;
  ASSERT_EQ(observer.changes().size(), 0);
  SetControllerState(kTestAlsLevelMin, kTestAlsPercentMin,
                     -1.0, -1, true, &observer);
  EXPECT_TRUE(controller_->SetCurrentBrightnessPercent(
      kTestCurrentPercent,
      BRIGHTNESS_CHANGE_AUTOMATED,
      TRANSITION_INSTANT));
  CheckCurrentBrightnessPercent(kTestCurrentLevel,
                                kTestCurrentPercent,
                                -1.0);
  EXPECT_EQ(1, observer.changes().size());
  EXPECT_EQ(kTestCurrentLevel, backlight_.current_level());
}

TEST_F(KeyboardBacklightControllerTest, HandleVideoActivity) {
  // Interval already expired.
  controller_->is_fullscreen_ = true;
  controller_->is_video_playing_ = true;
  controller_->HandleVideoActivity(
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
  controller_->als_target_percent_ = kTestCurrentPercent;
  controller_->HandleVideoActivity(base::TimeTicks::Now(), true);
  EXPECT_TRUE(controller_->is_fullscreen_);
  EXPECT_TRUE(controller_->is_video_playing_);
  EXPECT_FALSE(controller_->video_enabled_);
  EXPECT_EQ(0, controller_->current_level_);
  EXPECT_EQ(kTestCurrentPercent, controller_->als_target_percent_);
  ASSERT_GT(controller_->video_timeout_timer_id_, 0);
  EXPECT_EQ(0, backlight_.current_level());
  util::RemoveTimeout(&controller_->video_timeout_timer_id_);
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

  controller_->als_target_percent_ = kTestCurrentPercent;
  // Enabling the backlight due to video not playing.
  controller_->current_level_ = 0;
  controller_->is_video_playing_ = false;
  controller_->is_fullscreen_ = true;
  controller_->video_enabled_ = false;
  controller_->UpdateBacklightEnabled();
  EXPECT_TRUE(controller_->video_enabled_);
  EXPECT_EQ(kTestCurrentLevel, controller_->current_level_);
  EXPECT_EQ(kTestCurrentLevel, backlight_.current_level());

  // Enabling the backlight due to not fullscreen.
  controller_->current_level_ = 0;
  controller_->is_video_playing_ = true;
  controller_->is_fullscreen_ = false;
  controller_->video_enabled_ = false;
  controller_->UpdateBacklightEnabled();
  EXPECT_TRUE(controller_->video_enabled_);
  EXPECT_EQ(kTestCurrentLevel, controller_->current_level_);
  EXPECT_EQ(kTestCurrentLevel, backlight_.current_level());

  // Disabling the backlight.
  controller_->current_level_ = kTestCurrentLevel;
  controller_->is_video_playing_ = true;
  controller_->is_fullscreen_ = true;
  controller_->video_enabled_ = true;
  controller_->UpdateBacklightEnabled();
  EXPECT_FALSE(controller_->video_enabled_);
  EXPECT_EQ(0, controller_->current_level_);
  EXPECT_EQ(0, backlight_.current_level());
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
  controller_->als_target_percent_ = kTestCurrentPercent;
  controller_->current_level_ = 0;
  controller_->is_video_playing_ = true;
  controller_->is_fullscreen_ = true;
  controller_->video_enabled_ = false;
  EXPECT_FALSE(controller_->VideoTimeout());
  EXPECT_FALSE(controller_->is_video_playing_);
  EXPECT_TRUE(controller_->video_enabled_);
  EXPECT_EQ(kTestCurrentLevel, controller_->current_level_);
  EXPECT_EQ(kTestCurrentLevel, backlight_.current_level());

  // Timeout doesn't cause causes transition.
  controller_->als_target_percent_ = kTestCurrentPercent;
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

  // ALS returns bad value.
  light_sensor_.set_values(0.0, -1);
  controller_->OnAmbientLightChanged(&light_sensor_);
  CheckAlsStep(kTestStepIndex, kTestSyntheticLux);
  CheckHysteresisState(KeyboardBacklightController::ALS_HYST_IDLE, 0);

  // ALS returns the already set value.
  light_sensor_.set_values(0.0, controller_->lux_level_);
  light_sensor_.NotifyObservers();
  CheckAlsStep(kTestStepIndex, kTestSyntheticLux);
  CheckHysteresisState(KeyboardBacklightController::ALS_HYST_IDLE, 0);

  // First Increase, hysteresis not overcome.
  light_sensor_.set_values(0.0, kTestIncreaseLux);
  light_sensor_.NotifyObservers();
  CheckAlsStep(kTestStepIndex, kTestSyntheticLux);
  CheckHysteresisState(KeyboardBacklightController::ALS_HYST_UP, 1);

  // Second Increase, hysteresis overcome.
  light_sensor_.set_values(0.0, kTestIncreaseLux);
  light_sensor_.NotifyObservers();
  EXPECT_EQ(kTestAlsPercents[kTestStepIndex + 1], backlight_.current_level());
  CheckAlsStep(kTestStepIndex + 1, kTestIncreaseLux);
  CheckHysteresisState(KeyboardBacklightController::ALS_HYST_UP, 1);

  // Reset for testing the other direction.
  controller_->als_step_index_ = kTestStepIndex;
  controller_->als_target_percent_ = kTestAlsPercents[kTestStepIndex];
  controller_->lux_level_ = kTestSyntheticLux;
  controller_->hysteresis_state_ = KeyboardBacklightController::ALS_HYST_IDLE;
  controller_->hysteresis_count_ = 0;

  // First Decrease, hysteresis not overcome.
  light_sensor_.set_values(0.0, kTestDecreaseLux);
  light_sensor_.NotifyObservers();
  CheckAlsStep(kTestStepIndex, kTestSyntheticLux);
  CheckHysteresisState(KeyboardBacklightController::ALS_HYST_DOWN, 1);

  // Second Decrease, hysteresis overcome.
  light_sensor_.set_values(0.0, kTestDecreaseLux);
  light_sensor_.NotifyObservers();
  EXPECT_EQ(kTestAlsPercents[kTestStepIndex - 1], backlight_.current_level());
  CheckAlsStep(kTestStepIndex - 1, kTestDecreaseLux);
  CheckHysteresisState(KeyboardBacklightController::ALS_HYST_DOWN, 1);
}

TEST_F(KeyboardBacklightControllerTest, Transitions) {
  // Request an instant transition to the maximum level.
  controller_->SetCurrentBrightnessPercent(
      kTestAlsPercentMax, BRIGHTNESS_CHANGE_AUTOMATED, TRANSITION_INSTANT);
  EXPECT_EQ(kTestAlsLevelMax, backlight_.current_level());
  EXPECT_EQ(0, backlight_.current_interval().InMilliseconds());

  // Start a slow transition to the dimmed level.
  controller_->SetCurrentBrightnessPercent(
      kTestAlsPercentDim, BRIGHTNESS_CHANGE_AUTOMATED, TRANSITION_SLOW);
  EXPECT_EQ(controller_->PercentToLevel(kTestAlsPercentDim),
            backlight_.current_level());
  EXPECT_EQ(KeyboardBacklightController::kSlowTransitionMs,
            backlight_.current_interval().InMilliseconds());

  // Start a fast transition back to the maximum level.
  controller_->SetCurrentBrightnessPercent(
      kTestAlsPercentMax, BRIGHTNESS_CHANGE_AUTOMATED, TRANSITION_FAST);
  EXPECT_EQ(kTestAlsLevelMax, backlight_.current_level());
  EXPECT_EQ(KeyboardBacklightController::kFastTransitionMs,
            backlight_.current_interval().InMilliseconds());

  // We shouldn't do anything in response to a request to go to the level that
  // we last requested.
  controller_->SetCurrentBrightnessPercent(
      kTestAlsPercentMax, BRIGHTNESS_CHANGE_AUTOMATED, TRANSITION_SLOW);
  EXPECT_EQ(kTestAlsLevelMax, backlight_.current_level());
}

TEST_F(KeyboardBacklightControllerTest, ReadLimitsPrefs) {
  double test_min = -1.0;
  double test_dim = -1.0;
  double test_max= -1.0;
  // Success case.
  controller_->ReadLimitsPrefs(kKeyboardBacklightAlsLimitsPref,
                               &test_min, &test_dim, &test_max);
  EXPECT_EQ(kTestAlsPercentMin, test_min);
  EXPECT_EQ(kTestAlsPercentDim, test_dim);
  EXPECT_EQ(kTestAlsPercentMax, test_max);

  test_min = -1.0;
  test_dim = -1.0;
  test_max= -1.0;
  // Failure to read file case.
  prefs_.Unset(kKeyboardBacklightAlsLimitsPref);
  controller_->ReadLimitsPrefs(kKeyboardBacklightAlsLimitsPref,
                               &test_min, &test_dim, &test_max);
  EXPECT_EQ(-1.0, test_min);
  EXPECT_EQ(-1.0, test_dim);
  EXPECT_EQ(-1.0, test_max);

  test_min = -1.0;
  test_dim = -1.0;
  test_max= -1.0;
  // Failure to parse input strings.
  prefs_.SetString(kKeyboardBacklightAlsLimitsPref, "");
  controller_->ReadLimitsPrefs(kKeyboardBacklightAlsLimitsPref,
                               &test_min, &test_dim, &test_max);
  EXPECT_EQ(-1.0, test_min);
  EXPECT_EQ(-1.0, test_dim);
  EXPECT_EQ(-1.0, test_max);
}

TEST_F(KeyboardBacklightControllerTest, ReadAlsStepsPref) {
  controller_->als_steps_.clear();
  // Success case.
  controller_->ReadAlsStepsPref(kKeyboardBacklightAlsStepsPref);
  EXPECT_EQ(kTestNumAlsSteps, controller_->als_steps_.size());
  for (size_t i = 0; i < controller_->als_steps_.size(); i++) {
    EXPECT_EQ(kTestAlsPercents[i], controller_->als_steps_[i].target_percent);
    EXPECT_EQ(kTestAlsDecreaseThresholds[i],
              controller_->als_steps_[i].decrease_threshold);
    EXPECT_EQ(kTestAlsIncreaseThresholds[i],
              controller_->als_steps_[i].increase_threshold);
  }

  controller_->als_steps_.clear();
  // Failure to read file case.
  prefs_.Unset(kKeyboardBacklightAlsStepsPref);
  controller_->ReadAlsStepsPref(kKeyboardBacklightAlsStepsPref);
  EXPECT_EQ(kDefaultNumAlsSteps, controller_->als_steps_.size());
  EXPECT_EQ(kDefaultAlsPercent, controller_->als_steps_[0].target_percent);
  EXPECT_EQ(kDefaultAlsDecreaseThreshold,
            controller_->als_steps_[0].decrease_threshold);
  EXPECT_EQ(kDefaultAlsIncreaseThreshold,
            controller_->als_steps_[0].increase_threshold);

  controller_->als_steps_.clear();
  // Bad lines in input.
  prefs_.SetString(kKeyboardBacklightAlsStepsPref,
                   kTestAlsStepsString + "\n" + "\n0.1" + "\nNot a number");
  controller_->ReadAlsStepsPref(kKeyboardBacklightAlsStepsPref);
  EXPECT_EQ(kTestNumAlsSteps, controller_->als_steps_.size());
  for (size_t i = 0; i < controller_->als_steps_.size(); i++) {
    EXPECT_EQ(kTestAlsPercents[i], controller_->als_steps_[i].target_percent);
    EXPECT_EQ(kTestAlsDecreaseThresholds[i],
              controller_->als_steps_[i].decrease_threshold);
    EXPECT_EQ(kTestAlsIncreaseThresholds[i],
              controller_->als_steps_[i].increase_threshold);
  }
}

TEST_F(KeyboardBacklightControllerTest, ReadUserStepsPref) {
  controller_->user_steps_.clear();
  // Success case.
  controller_->ReadUserStepsPref(kKeyboardBacklightUserStepsPref);
  EXPECT_EQ(kTestNumUserSteps, controller_->user_steps_.size());
  for (size_t i = 0; i < controller_->user_steps_.size(); i++)
    EXPECT_EQ(kTestUserPercents[i], controller_->user_steps_[i]);

  controller_->user_steps_.clear();
  // Failure to read file case.
  prefs_.Unset(kKeyboardBacklightUserStepsPref);
  controller_->ReadUserStepsPref(kKeyboardBacklightUserStepsPref);
  EXPECT_EQ(kDefaultNumUserSteps, controller_->user_steps_.size());
  for (size_t i = 0; i < controller_->user_steps_.size(); i++)
    EXPECT_EQ(kDefaultUserPercents[i], controller_->user_steps_[i]);

  controller_->user_steps_.clear();
  // Bad lines in input.
  prefs_.SetString(kKeyboardBacklightUserStepsPref,
                   kTestUserStepsString + "\nNot a number");
  controller_->ReadUserStepsPref(kKeyboardBacklightUserStepsPref);
  EXPECT_EQ(kTestNumUserSteps, controller_->user_steps_.size());
  for (size_t i = 0; i < controller_->user_steps_.size(); i++)
    EXPECT_EQ(kTestUserPercents[i], controller_->user_steps_[i]);
}

TEST_F(KeyboardBacklightControllerTest, InitializeUserStepIndex) {
  controller_->user_step_index_ = -1;
  // Normal usage case.
  controller_->current_level_ = kTestUserLevels[kTestFoundUserStepIndex] - 5;
  controller_->InitializeUserStepIndex();
  EXPECT_EQ(kTestFoundUserStepIndex, controller_->user_step_index_);
  EXPECT_EQ(kTestUserPercents[kTestFoundUserStepIndex],
            controller_->user_target_percent_);

  controller_->user_step_index_ = -1;
  // High current level case.
  controller_->current_level_ = kTestUserLevelMax + 10;
  controller_->InitializeUserStepIndex();
  EXPECT_EQ(kTestNumUserSteps - 1, controller_->user_step_index_);
  EXPECT_EQ(kTestUserPercents[kTestNumUserSteps - 1],
            controller_->user_target_percent_);

  // Calling again without clearing should cause an early exit and nothing
  // should change.
  controller_->current_level_ = -1;
  controller_->InitializeUserStepIndex();
  EXPECT_EQ(kTestNumUserSteps - 1, controller_->user_step_index_);
  EXPECT_EQ(kTestUserPercents[kTestNumUserSteps - 1],
            controller_->user_target_percent_);
}

TEST_F(KeyboardBacklightControllerTest, GetNewLevel) {
  controller_->als_target_percent_ = kTestAlsPercents[kTestStepIndex];
  controller_->user_target_percent_ = kTestUserPercents[kTestStepIndex];
  controller_->als_step_index_ = kTestStepIndex;
  controller_->current_level_ = kTestCurrentLevel;

  // Fullscreen video is playing and ALS control case.
  controller_->user_step_index_ = -1;
  controller_->video_enabled_ = false;
  EXPECT_EQ(kTestAlsLevelMin, controller_->GetNewLevel());
  controller_->video_enabled_ = true;

  controller_->state_ = BACKLIGHT_ACTIVE;
  // Backlight active and ALS control case.
  controller_->user_step_index_ = -1;
  EXPECT_EQ(kTestAlsLevels[kTestStepIndex], controller_->GetNewLevel());

  // Backlight active and user control case.
  controller_->user_step_index_ = kTestStepIndex;
  EXPECT_EQ(kTestUserLevels[kTestStepIndex], controller_->GetNewLevel());

  controller_->state_ = BACKLIGHT_DIM;
  // Backlight dim and ALS control case.
  controller_->user_step_index_ = -1;
  EXPECT_EQ(kTestAlsLevelDim, controller_->GetNewLevel());

  // Backlight dim and user control case.
  controller_->user_step_index_ = kTestStepIndex;
  EXPECT_EQ(kTestUserLevelDim, controller_->GetNewLevel());

  controller_->state_ = BACKLIGHT_IDLE_OFF;
  // Backlight idle off and ALS control case.
  controller_->user_step_index_ = -1;
  EXPECT_EQ(kTestAlsLevelMin, controller_->GetNewLevel());

  // Backlight idle and user control case.
  controller_->user_step_index_ = kTestStepIndex;
  EXPECT_EQ(kTestUserLevelMin, controller_->GetNewLevel());

  controller_->state_ = BACKLIGHT_SUSPENDED;
  // Backlight suspended and ALS control case.
  controller_->user_step_index_ = -1;
  EXPECT_EQ(kTestAlsLevelMin, controller_->GetNewLevel());

  // Backlight suspended and user control case.
  controller_->user_step_index_ = kTestStepIndex;
  EXPECT_EQ(kTestUserLevelMin, controller_->GetNewLevel());

  // All other backlight states cases.
  controller_->state_ = BACKLIGHT_ALREADY_DIMMED;
  controller_->user_step_index_ = -1;
  EXPECT_EQ(kTestCurrentLevel, controller_->GetNewLevel());
  controller_->user_step_index_ = kTestStepIndex;
  EXPECT_EQ(kTestCurrentLevel, controller_->GetNewLevel());
  controller_->state_ = BACKLIGHT_UNINITIALIZED;
  controller_->user_step_index_ = -1;
  EXPECT_EQ(kTestCurrentLevel, controller_->GetNewLevel());
  controller_->user_step_index_ = kTestStepIndex;
  EXPECT_EQ(kTestCurrentLevel, controller_->GetNewLevel());

}

TEST_F(KeyboardBacklightControllerTest, IncreaseBrightness) {
  controller_->state_ = BACKLIGHT_ACTIVE;
  controller_->num_user_adjustments_ = 0;
  controller_->current_level_ = kTestUserPercents[kTestFoundUserStepIndex] - 5;
  controller_->user_step_index_ = -1;

  // Non-user adjustment case.
  EXPECT_FALSE(controller_->IncreaseBrightness(BRIGHTNESS_CHANGE_AUTOMATED));
  EXPECT_EQ(0, controller_->num_user_adjustments_);
  EXPECT_EQ(kTestUserLevels[kTestFoundUserStepIndex] - 5,
            controller_->current_level_);
  EXPECT_EQ(-1, controller_->user_step_index_);

  // Non-initialized case.
  EXPECT_TRUE(controller_->IncreaseBrightness(
      BRIGHTNESS_CHANGE_USER_INITIATED));
  EXPECT_EQ(kTestUserLevels[kTestFoundUserStepIndex + 1],
            backlight_.current_level());
  EXPECT_EQ(1, controller_->num_user_adjustments_);
  EXPECT_EQ(kTestFoundUserStepIndex + 1, controller_->user_step_index_);
  EXPECT_EQ(kTestUserPercents[kTestFoundUserStepIndex + 1],
            controller_->user_target_percent_);
  EXPECT_EQ(kTestUserLevels[kTestFoundUserStepIndex + 1],
            controller_->current_level_);

  // Normal case.
  EXPECT_TRUE(controller_->IncreaseBrightness(
      BRIGHTNESS_CHANGE_USER_INITIATED));
  EXPECT_EQ(kTestUserLevels[kTestFoundUserStepIndex + 2],
            backlight_.current_level());
  EXPECT_EQ(2, controller_->num_user_adjustments_);
  EXPECT_EQ(kTestFoundUserStepIndex + 2, controller_->user_step_index_);
  EXPECT_EQ(kTestUserPercents[kTestFoundUserStepIndex + 2],
            controller_->user_target_percent_);
  EXPECT_EQ(kTestUserLevels[kTestFoundUserStepIndex + 2],
            controller_->current_level_);

  // Maxed out case.
  EXPECT_TRUE(controller_->IncreaseBrightness(
      BRIGHTNESS_CHANGE_USER_INITIATED));
  EXPECT_EQ(3, controller_->num_user_adjustments_);
  EXPECT_EQ(kTestFoundUserStepIndex + 2, controller_->user_step_index_);
  EXPECT_EQ(kTestUserPercents[kTestFoundUserStepIndex + 2],
            controller_->user_target_percent_);
  EXPECT_EQ(kTestUserLevels[kTestFoundUserStepIndex + 2],
            controller_->current_level_);
}

TEST_F(KeyboardBacklightControllerTest, DecreaseBrightness) {
  controller_->state_ = BACKLIGHT_ACTIVE;
  controller_->num_user_adjustments_ = 0;
  controller_->current_level_ = kTestUserPercents[kTestFoundUserStepIndex] - 5;
  controller_->user_step_index_ = -1;

  // Non-user adjustment case.
  EXPECT_FALSE(controller_->DecreaseBrightness(true,
                                               BRIGHTNESS_CHANGE_AUTOMATED));
  EXPECT_EQ(0, controller_->num_user_adjustments_);
  EXPECT_EQ(kTestUserLevels[kTestFoundUserStepIndex] - 5,
            controller_->current_level_);
  EXPECT_EQ(-1, controller_->user_step_index_);

  // Non-initialized case.
  EXPECT_TRUE(controller_->DecreaseBrightness(
      true,
      BRIGHTNESS_CHANGE_USER_INITIATED));
  EXPECT_EQ(kTestUserLevels[kTestFoundUserStepIndex - 1],
            backlight_.current_level());
  EXPECT_EQ(1, controller_->num_user_adjustments_);
  EXPECT_EQ(kTestFoundUserStepIndex - 1, controller_->user_step_index_);
  EXPECT_EQ(kTestUserPercents[kTestFoundUserStepIndex - 1],
            controller_->user_target_percent_);
  EXPECT_EQ(kTestUserLevels[kTestFoundUserStepIndex - 1],
            controller_->current_level_);

  // Normal case.
  EXPECT_TRUE(controller_->DecreaseBrightness(
      true,
      BRIGHTNESS_CHANGE_USER_INITIATED));
  EXPECT_EQ(kTestUserLevels[kTestFoundUserStepIndex - 2],
            backlight_.current_level());
  EXPECT_EQ(2, controller_->num_user_adjustments_);
  EXPECT_EQ(kTestFoundUserStepIndex - 2, controller_->user_step_index_);
  EXPECT_EQ(kTestUserPercents[kTestFoundUserStepIndex - 2],
            controller_->user_target_percent_);
  EXPECT_EQ(kTestUserLevels[kTestFoundUserStepIndex - 2],
            controller_->current_level_);

  // Already at 0 case.
  EXPECT_TRUE(controller_->DecreaseBrightness(
      true,
      BRIGHTNESS_CHANGE_USER_INITIATED));
  EXPECT_EQ(3, controller_->num_user_adjustments_);
  EXPECT_EQ(kTestFoundUserStepIndex - 2, controller_->user_step_index_);
  EXPECT_EQ(kTestUserPercents[kTestFoundUserStepIndex - 2],
            controller_->user_target_percent_);
  EXPECT_EQ(kTestUserLevels[kTestFoundUserStepIndex - 2],
            controller_->current_level_);
}

TEST_F(KeyboardBacklightControllerTest, TurnOffWhenShuttingDown) {
  controller_->SetPowerState(BACKLIGHT_SHUTTING_DOWN);
  EXPECT_EQ(0, backlight_.current_level());
  EXPECT_EQ(0, backlight_.current_interval().InMilliseconds());
}

}  // namespace power_manager
