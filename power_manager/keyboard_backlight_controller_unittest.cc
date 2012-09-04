// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/memory/scoped_ptr.h>
#include <gtest/gtest.h>

#include "power_manager/keyboard_backlight_controller.h"
#include "power_manager/mock_backlight.h"
#include "power_manager/mock_backlight_controller_observer.h"

using ::testing::Mock;
using ::testing::StrictMock;
using ::testing::Test;

namespace power_manager {

const int64 kTestMaxLevel = 100;
const int64 kTestCurrentLevel = 40;
const double kTestMaxPercent = 100.0;
const double kTestCurrentPercent = 40.0;
const int64 kTestTimeoutIntervalMs = 10000;  // Intentionally larger then the
                                             // timeout in the class.

class KeyboardBacklightControllerTest : public Test {
 public:
  KeyboardBacklightControllerTest() { }

  virtual void SetUp() {
    controller_.reset(new KeyboardBacklightController(&backlight_));
  }

  virtual void TearDown() {
    controller_.reset();
  }

  SIGNAL_CALLBACK_0(KeyboardBacklightControllerTest, gboolean, TestTimeout);

 protected:
  StrictMock<MockBacklight> backlight_;
  scoped_ptr<KeyboardBacklightController> controller_;
};  // class KeyboardBacklightControllerTest

gboolean KeyboardBacklightControllerTest::TestTimeout() {
  return false;
}

TEST_F(KeyboardBacklightControllerTest, Init) {
  // GetMaxBrightnessLevel fails.
  controller_->target_percent_ = 0.0;
  backlight_.ExpectGetMaxBrightnessLevel(kTestMaxLevel, false);
  EXPECT_FALSE(controller_->Init());
  EXPECT_EQ(0.0, controller_->target_percent_);
  Mock::VerifyAndClearExpectations(&backlight_);

  // GetCurrentBrightnessLevel fails.
  controller_->target_percent_ = 0.0;
  backlight_.ExpectGetMaxBrightnessLevel(kTestMaxLevel, true);
  backlight_.ExpectGetCurrentBrightnessLevel(kTestCurrentLevel, false);
  EXPECT_FALSE(controller_->Init());
  EXPECT_EQ(0.0, controller_->target_percent_);
  Mock::VerifyAndClearExpectations(&backlight_);

  // Operation succeeds.
  controller_->target_percent_ = 0.0;
  backlight_.ExpectGetMaxBrightnessLevel(kTestMaxLevel, true);
  backlight_.ExpectGetCurrentBrightnessLevel(kTestCurrentLevel, true);
  EXPECT_TRUE(controller_->Init());
  EXPECT_EQ(kTestCurrentPercent, controller_->target_percent_);
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
  controller_->max_level_ = kTestMaxLevel;
  controller_->current_level_ = kTestCurrentLevel;
  percent = 0.0;
  EXPECT_TRUE(controller_->GetCurrentBrightnessPercent(&percent));
  EXPECT_EQ(kTestCurrentPercent, percent);
}

TEST_F(KeyboardBacklightControllerTest, SetCurrentBrightnessPercent) {
  controller_->max_level_ = kTestMaxLevel;

  // Cause is user initiated.
  controller_->current_level_ = 0;
  controller_->target_percent_ = 0.0;
  controller_->backlight_enabled_ = true;
  controller_->observer_ = NULL;
  controller_->num_user_adjustments_ = 0;
  backlight_.ExpectSetBrightnessLevel(kTestCurrentLevel, true);
  EXPECT_TRUE(controller_->SetCurrentBrightnessPercent(
      kTestCurrentPercent,
      BRIGHTNESS_CHANGE_USER_INITIATED,
      TRANSITION_INSTANT));
  EXPECT_EQ(kTestCurrentLevel, controller_->current_level_);
  EXPECT_EQ(kTestCurrentPercent, controller_->target_percent_);
  EXPECT_EQ(1, controller_->num_user_adjustments_);
  Mock::VerifyAndClearExpectations(&backlight_);

  // Observer present.
 MockBacklightControllerObserver observer;
  controller_->current_level_ = 0;
  controller_->target_percent_ = 0.0;
  controller_->backlight_enabled_ = true;
  ASSERT_EQ(observer.changes().size(), 0);
  controller_->observer_ = &observer;
  controller_->num_user_adjustments_ = 0;
  backlight_.ExpectSetBrightnessLevel(kTestCurrentLevel, true);
  EXPECT_TRUE(controller_->SetCurrentBrightnessPercent(
      kTestCurrentPercent,
      BRIGHTNESS_CHANGE_USER_INITIATED,
      TRANSITION_INSTANT));
  EXPECT_EQ(kTestCurrentLevel, controller_->current_level_);
  EXPECT_EQ(kTestCurrentPercent, controller_->target_percent_);
  EXPECT_EQ(1, controller_->num_user_adjustments_);
  EXPECT_EQ(1, observer.changes().size());
}

TEST_F(KeyboardBacklightControllerTest, SetAlsBrightnessOffsetPercent) {
  controller_->max_level_ = kTestMaxLevel;

  // ALS turns on the backlight.
  controller_->current_level_ = 0;
  controller_->target_percent_ = 0.0;
  controller_->backlight_enabled_ = true;
  controller_->observer_ = NULL;
  controller_->num_als_adjustments_ = 0;
  controller_->num_user_adjustments_ = 0;
  backlight_.ExpectSetBrightnessLevel(kTestMaxLevel, true);
  controller_->SetAlsBrightnessOffsetPercent(0);
  EXPECT_EQ(kTestMaxLevel, controller_->current_level_);
  EXPECT_EQ(kTestMaxPercent, controller_->target_percent_);
  EXPECT_EQ(1, controller_->num_als_adjustments_);
  EXPECT_EQ(0, controller_->num_user_adjustments_);
  Mock::VerifyAndClearExpectations(&backlight_);

  // ALS turns off the backlight.
  controller_->current_level_ = kTestCurrentLevel;
  controller_->target_percent_ = kTestCurrentPercent;
  controller_->backlight_enabled_ = true;
  controller_->observer_ = NULL;
  controller_->num_als_adjustments_ = 0;
  controller_->num_user_adjustments_ = 0;
  backlight_.ExpectSetBrightnessLevel(0, true);
  controller_->SetAlsBrightnessOffsetPercent(kTestMaxPercent);
  EXPECT_EQ(0, controller_->current_level_);
  EXPECT_EQ(0.0, controller_->target_percent_);
  EXPECT_EQ(1, controller_->num_als_adjustments_);
  EXPECT_EQ(0, controller_->num_user_adjustments_);
}

TEST_F(KeyboardBacklightControllerTest, OnVideoDetectorEvent) {
  controller_->max_level_ = kTestMaxLevel;

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
  controller_->backlight_enabled_ = true;
  controller_->observer_ = NULL;
  controller_->current_level_ = kTestCurrentLevel;
  controller_->target_percent_ = kTestCurrentPercent;
  backlight_.ExpectSetBrightnessLevel(0, true);
  controller_->OnVideoDetectorEvent(base::TimeTicks::Now(), true);
  EXPECT_TRUE(controller_->is_fullscreen_);
  EXPECT_TRUE(controller_->is_video_playing_);
  EXPECT_FALSE(controller_->backlight_enabled_);
  EXPECT_EQ(0, controller_->current_level_);
  EXPECT_EQ(kTestCurrentPercent, controller_->target_percent_);
  ASSERT_GT(controller_->video_timeout_timer_id_, 0);
  g_source_remove(controller_->video_timeout_timer_id_);
}

TEST_F(KeyboardBacklightControllerTest, UpdateBacklightEnabled) {
  controller_->max_level_ = kTestMaxLevel;

  // No change in state.
  controller_->is_video_playing_ = false;
  controller_->is_fullscreen_ = false;
  controller_->backlight_enabled_ = true;
  controller_->UpdateBacklightEnabled();
  EXPECT_TRUE(controller_->backlight_enabled_);

  controller_->is_video_playing_ = true;
  controller_->is_fullscreen_ = true;
  controller_->backlight_enabled_ = false;
  controller_->UpdateBacklightEnabled();
  EXPECT_FALSE(controller_->backlight_enabled_);

  controller_->target_percent_ = kTestCurrentPercent;
  // Enabling the backlight due to video not playing.
  controller_->current_level_ = 0;
  controller_->is_video_playing_ = false;
  controller_->is_fullscreen_ = true;
  controller_->backlight_enabled_ = false;
  backlight_.ExpectSetBrightnessLevel(kTestCurrentLevel, true);
  controller_->UpdateBacklightEnabled();
  EXPECT_TRUE(controller_->backlight_enabled_);
  EXPECT_EQ(kTestCurrentLevel, controller_->current_level_);
  Mock::VerifyAndClearExpectations(&backlight_);

  // Enabling the backlight due to not fullscreen.
  controller_->current_level_ = 0;
  controller_->is_video_playing_ = true;
  controller_->is_fullscreen_ = false;
  controller_->backlight_enabled_ = false;
  backlight_.ExpectSetBrightnessLevel(kTestCurrentLevel, true);
  controller_->UpdateBacklightEnabled();
  EXPECT_TRUE(controller_->backlight_enabled_);
  EXPECT_EQ(kTestCurrentLevel, controller_->current_level_);
  Mock::VerifyAndClearExpectations(&backlight_);

  // Disabling the backlight.
  controller_->current_level_ = kTestCurrentLevel;
  controller_->is_video_playing_ = true;
  controller_->is_fullscreen_ = true;
  controller_->backlight_enabled_ = true;
  backlight_.ExpectSetBrightnessLevel(0, true);
  controller_->UpdateBacklightEnabled();
  EXPECT_FALSE(controller_->backlight_enabled_);
  EXPECT_EQ(0, controller_->current_level_);
}

TEST_F(KeyboardBacklightControllerTest, SetBrightnessLevel) {
  // Operation succeeds.
  controller_->current_level_ = 0;
  backlight_.ExpectSetBrightnessLevel(kTestCurrentLevel, true);
  controller_->SetBrightnessLevel(kTestCurrentLevel);
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
  controller_->max_level_ = kTestMaxLevel;

  // Timeout causes transition from disabled to enabled.
  controller_->target_percent_ = kTestCurrentPercent;
  controller_->current_level_ = 0;
  controller_->is_video_playing_ = true;
  controller_->is_fullscreen_ = true;
  controller_->backlight_enabled_ = false;
  backlight_.ExpectSetBrightnessLevel(kTestCurrentLevel, true);
  EXPECT_FALSE(controller_->VideoTimeout());
  EXPECT_FALSE(controller_->is_video_playing_);
  EXPECT_TRUE(controller_->backlight_enabled_);
  EXPECT_EQ(kTestCurrentLevel, controller_->current_level_);
  Mock::VerifyAndClearExpectations(&backlight_);

  // Timeout doesn't cause causes transition.
  controller_->target_percent_ = kTestCurrentPercent;
  controller_->current_level_ = kTestCurrentLevel;
  controller_->is_video_playing_ = true;
  controller_->is_fullscreen_ = false;
  controller_->backlight_enabled_ = true;
  EXPECT_FALSE(controller_->VideoTimeout());
  EXPECT_FALSE(controller_->is_video_playing_);
  EXPECT_TRUE(controller_->backlight_enabled_);
}

TEST_F(KeyboardBacklightControllerTest, LevelToPercent) {
  // |max_level_| is uninitialized.
  controller_->max_level_ = 0;
  EXPECT_EQ(-1.0, controller_->LevelToPercent(kTestCurrentLevel));

  // |max_level_| is initialized.
  controller_->max_level_ = kTestMaxLevel;
  EXPECT_EQ(kTestCurrentPercent,
            controller_->LevelToPercent(kTestCurrentLevel));

  // |level| is too high.
  EXPECT_EQ(controller_->LevelToPercent(kTestCurrentLevel + kTestMaxLevel),
            kTestMaxPercent);

  // |level| is too low.
  EXPECT_EQ(0.0,
            controller_->LevelToPercent(kTestCurrentLevel - kTestMaxLevel));
}

TEST_F(KeyboardBacklightControllerTest, PercentToLevel) {
  // |max_level_| is uninitialized.
  controller_->max_level_ = 0;
  EXPECT_EQ(-1, controller_->PercentToLevel(kTestCurrentPercent));

  // |max_level_| is initialized.
  controller_->max_level_ = kTestMaxLevel;
  EXPECT_EQ(kTestCurrentLevel,
            controller_->PercentToLevel(kTestCurrentPercent));

  // |percent| is too high.
  EXPECT_EQ(kTestMaxLevel,
            controller_->PercentToLevel(kTestCurrentPercent + 100.0));

  // |percent| is too low.
  EXPECT_EQ(0, controller_->PercentToLevel(kTestCurrentPercent - 100.0));
}

}  // namespace power_manager
