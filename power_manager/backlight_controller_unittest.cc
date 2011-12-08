// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "base/logging.h"
#include "power_manager/backlight_controller.h"
#include "power_manager/mock_backlight.h"
#include "power_manager/power_constants.h"
#include "power_manager/power_prefs.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::SetArgumentPointee;
using std::make_pair;
using std::pair;
using std::vector;

namespace power_manager {

namespace {

const int64 kDefaultBrightness = 50;
const int64 kMaxBrightness = 100;
const int64 kPluggedBrightness = 70;
const int64 kUnpluggedBrightness = 30;
const int64 kAlsBrightness = 0;

// Repeating either increase or decrease brightness this many times should
// always leave the brightness at a limit.
const int kStepsToHitLimit = 20;

// Simple helper class that logs brightness changes for the NotifyObserver test.
class MockObserver : public BacklightControllerObserver {
 public:
  MockObserver() {}
  virtual ~MockObserver() {}

  vector<pair<double, BrightnessChangeCause> > changes() const {
    return changes_;
  }

  void Clear() {
    changes_.clear();
  }

  // BacklightControllerObserver implementation:
  virtual void OnBrightnessChanged(double brightness_level,
                                   BrightnessChangeCause cause) {
    changes_.push_back(make_pair(brightness_level, cause));
  }

 private:
  // Received changes, in oldest-to-newest order.
  vector<pair<double, BrightnessChangeCause> > changes_;

  DISALLOW_COPY_AND_ASSIGN(MockObserver);
};

}  // namespace

class BacklightControllerTest : public ::testing::Test {
 public:
  BacklightControllerTest()
      : prefs_(FilePath("."), FilePath(".")),
        controller_(&backlight_, &prefs_) {
  }

  virtual void SetUp() {
    EXPECT_CALL(backlight_, GetBrightness(NotNull(), NotNull()))
        .WillRepeatedly(DoAll(SetArgumentPointee<0>(kDefaultBrightness),
                              SetArgumentPointee<1>(kMaxBrightness),
                              Return(true)));
    prefs_.SetInt64(kPluggedBrightnessOffset, kPluggedBrightness);
    prefs_.SetInt64(kUnpluggedBrightnessOffset, kUnpluggedBrightness);
    prefs_.SetInt64(kAlsBrightnessLevel, kAlsBrightness);
    CHECK(controller_.Init());
  }

 protected:
  ::testing::StrictMock<MockBacklight> backlight_;
  PowerPrefs prefs_;
  BacklightController controller_;
};

TEST_F(BacklightControllerTest, IncreaseBrightness) {
  ASSERT_TRUE(controller_.SetPowerState(BACKLIGHT_ACTIVE));
  ASSERT_TRUE(controller_.OnPlugEvent(false));
#ifdef HAS_ALS
  EXPECT_EQ(kDefaultBrightness, controller_.local_brightness());
#else
  EXPECT_EQ(kUnpluggedBrightness, controller_.local_brightness());
#endif // defined(HAS_ALS)

  double old_local_brightness = controller_.local_brightness();
  controller_.IncreaseBrightness(BRIGHTNESS_CHANGE_AUTOMATED);
  // Check that the first step increases the brightness; within the loop
  // will just ensure that the brightness never decreases.
  EXPECT_GT(controller_.local_brightness(), old_local_brightness);

  for (int i = 0; i < kStepsToHitLimit; ++i) {
    old_local_brightness = controller_.local_brightness();
    controller_.IncreaseBrightness(BRIGHTNESS_CHANGE_USER_INITIATED);
    EXPECT_GE(controller_.local_brightness(), old_local_brightness);
  }

  EXPECT_EQ(kMaxBrightness, controller_.local_brightness());
}

TEST_F(BacklightControllerTest, DecreaseBrightness) {
  ASSERT_TRUE(controller_.SetPowerState(BACKLIGHT_ACTIVE));
  ASSERT_TRUE(controller_.OnPlugEvent(true));
#ifdef HAS_ALS
  EXPECT_EQ(kDefaultBrightness, controller_.local_brightness());
#else
  EXPECT_EQ(kPluggedBrightness, controller_.local_brightness());
#endif // defined(HAS_ALS)

  double old_local_brightness = controller_.local_brightness();
  controller_.DecreaseBrightness(true, BRIGHTNESS_CHANGE_AUTOMATED);
  // Check that the first step decreases the brightness; within the loop
  // will just ensure that the brightness never increases.
  EXPECT_LT(controller_.local_brightness(), old_local_brightness);

  for (int i = 0; i < kStepsToHitLimit; ++i) {
    old_local_brightness = controller_.local_brightness();
    controller_.DecreaseBrightness(true, BRIGHTNESS_CHANGE_USER_INITIATED);
    EXPECT_LE(controller_.local_brightness(), old_local_brightness);
  }

  // Backlight should now be off.
  EXPECT_EQ(0, controller_.local_brightness());
}

TEST_F(BacklightControllerTest, DecreaseBrightnessDisallowOff) {
  ASSERT_TRUE(controller_.SetPowerState(BACKLIGHT_ACTIVE));
  ASSERT_TRUE(controller_.OnPlugEvent(true));
#ifdef HAS_ALS
  EXPECT_EQ(kDefaultBrightness, controller_.local_brightness());
#else
  EXPECT_EQ(kPluggedBrightness, controller_.local_brightness());
#endif // defined(HAS_ALS)

  for (int i = 0; i < kStepsToHitLimit; ++i)
    controller_.DecreaseBrightness(false, BRIGHTNESS_CHANGE_USER_INITIATED);

  // Backlight must still be on.
  EXPECT_GT(controller_.local_brightness(), 0);
}

// Test that BacklightController notifies its observer in response to brightness
// changes.
TEST_F(BacklightControllerTest, NotifyObserver) {
  // Set an initial state.
  ASSERT_TRUE(controller_.SetPowerState(BACKLIGHT_ACTIVE));
  ASSERT_TRUE(controller_.OnPlugEvent(false));
  controller_.SetAlsBrightnessLevel(16);

  MockObserver observer;
  controller_.set_observer(&observer);

  // Increase the brightness and check that the observer is notified.
  observer.Clear();
  controller_.IncreaseBrightness(BRIGHTNESS_CHANGE_AUTOMATED);
  ASSERT_EQ(1, static_cast<int>(observer.changes().size()));
  EXPECT_EQ(controller_.local_brightness(), observer.changes()[0].first);
  EXPECT_EQ(BRIGHTNESS_CHANGE_AUTOMATED, observer.changes()[0].second);

  // Decrease the brightness.
  observer.Clear();
  controller_.DecreaseBrightness(true, BRIGHTNESS_CHANGE_USER_INITIATED);
  ASSERT_EQ(1, static_cast<int>(observer.changes().size()));
  EXPECT_EQ(controller_.local_brightness(), observer.changes()[0].first);
  EXPECT_EQ(BRIGHTNESS_CHANGE_USER_INITIATED, observer.changes()[0].second);

  // Send enough ambient light sensor samples to trigger a brightness change.
  observer.Clear();
  double old_brightness = controller_.local_brightness();
  for (int i = 0; i < 10; ++i)
    controller_.SetAlsBrightnessLevel(32);
  ASSERT_NE(old_brightness, controller_.local_brightness());
  ASSERT_EQ(1, static_cast<int>(observer.changes().size()));
  EXPECT_EQ(controller_.local_brightness(), observer.changes()[0].first);
  EXPECT_EQ(BRIGHTNESS_CHANGE_AUTOMATED, observer.changes()[0].second);

  // Plug the device in.
  observer.Clear();
  ASSERT_TRUE(controller_.OnPlugEvent(true));
  ASSERT_EQ(1, static_cast<int>(observer.changes().size()));
  EXPECT_EQ(controller_.local_brightness(), observer.changes()[0].first);
  EXPECT_EQ(BRIGHTNESS_CHANGE_AUTOMATED, observer.changes()[0].second);

  // Dim the backlight.
  observer.Clear();
  ASSERT_TRUE(controller_.SetPowerState(BACKLIGHT_DIM));
  ASSERT_EQ(1, static_cast<int>(observer.changes().size()));
  EXPECT_EQ(controller_.local_brightness(), observer.changes()[0].first);
  EXPECT_EQ(BRIGHTNESS_CHANGE_AUTOMATED, observer.changes()[0].second);
}

}  // namespace power_manager
