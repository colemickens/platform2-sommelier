// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include <gtest/gtest.h>
#include <inttypes.h>

#include <iostream>
#include <fstream>

#include "base/file_util.h"
#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "power_manager/common/clock.h"
#include "power_manager/powerd/system/internal_backlight.h"

using ::testing::Test;

namespace power_manager {
namespace system {

class InternalBacklightTest : public Test {
 public:
  InternalBacklightTest() {}

  virtual void SetUp() {
    CHECK(file_util::CreateNewTempDirectory("backlight_unittest", &test_path_));
  }

  virtual void TearDown() {
    file_util::Delete(test_path_, true);
  }

  // Create files to make the given directory look like it is a sysfs backlight
  // dir.
  void PopulateBacklightDir(const base::FilePath& path,
                            int64 brightness,
                            int64 max_brightness,
                            int64 actual_brightness) {
    CHECK(file_util::CreateDirectory(path));

    FILE* brightness_file = file_util::OpenFile(
        path.Append(InternalBacklight::kBrightnessFilename), "w");
    fprintf(brightness_file, "%"PRId64"\n", brightness);
    file_util::CloseFile(brightness_file);

    FILE* max_brightness_file = file_util::OpenFile(
        path.Append(InternalBacklight::kMaxBrightnessFilename), "w");
    fprintf(max_brightness_file, "%"PRId64"\n", max_brightness);
    file_util::CloseFile(max_brightness_file);

    if (actual_brightness >= 0) {
      FILE* actual_brightness_file = file_util::OpenFile(
          path.Append(InternalBacklight::kActualBrightnessFilename), "w");
      fprintf(actual_brightness_file, "%"PRId64"\n", actual_brightness);
      file_util::CloseFile(actual_brightness_file);
    }
  }

  // Returns the value from the "brightness" file in |directory|.
  // -1 is returned on error.
  int64 ReadBrightness(const base::FilePath& directory) {
    std::string data;
    base::FilePath file =
        directory.Append(InternalBacklight::kBrightnessFilename);
    if (!file_util::ReadFileToString(file, &data)) {
      LOG(ERROR) << "Unable to read data from " << file.value();
      return -1;
    }
    int64 level = 0;
    TrimWhitespaceASCII(data, TRIM_TRAILING, &data);
    if (!base::StringToInt64(data, &level)) {
      LOG(ERROR) << "Unable to parse \"" << level << "\" from " << file.value();
      return -1;
    }
    return level;
  }

  // Calls |backlight|'s GetCurrentBrightnessMethod().  Returns the current
  // brightness or -1 on failure.
  int64 GetCurrentBrightness(InternalBacklight* backlight) {
    DCHECK(backlight);
    int64 level = 0;
    return backlight->GetCurrentBrightnessLevel(&level) ? level : -1;
  }

 protected:
  base::FilePath test_path_;
};

// A basic test of functionality
TEST_F(InternalBacklightTest, BasicTest) {
  base::FilePath this_test_path = test_path_.Append("basic_test");
  const int64 kBrightness = 128;
  const int64 kMaxBrightness = 255;
  const int64 kActualBrightness = 127;

  base::FilePath my_path = this_test_path.Append("pwm-backlight");
  PopulateBacklightDir(my_path, kBrightness, kMaxBrightness, kActualBrightness);

  InternalBacklight backlight;
  ASSERT_TRUE(backlight.Init(this_test_path, "*"));

  int64 level = 0;
  EXPECT_TRUE(backlight.GetCurrentBrightnessLevel(&level));
  EXPECT_EQ(kActualBrightness, level);

  int64 max_level = 0;
  EXPECT_TRUE(backlight.GetMaxBrightnessLevel(&max_level));
  EXPECT_EQ(kMaxBrightness, max_level);
}

// Make sure things work OK when there is no actual_brightness file.
TEST_F(InternalBacklightTest, NoActualBrightnessTest) {
  base::FilePath this_test_path =
      test_path_.Append("no_actual_brightness_test");
  const int64 kBrightness = 128;
  const int64 kMaxBrightness = 255;

  base::FilePath my_path = this_test_path.Append("pwm-backlight");
  PopulateBacklightDir(my_path, kBrightness, kMaxBrightness, -1);

  InternalBacklight backlight;
  ASSERT_TRUE(backlight.Init(this_test_path, "*"));

  int64 level = 0;
  EXPECT_TRUE(backlight.GetCurrentBrightnessLevel(&level));
  EXPECT_EQ(kBrightness, level);

  int64 max_level = 0;
  EXPECT_TRUE(backlight.GetMaxBrightnessLevel(&max_level));
  EXPECT_EQ(kMaxBrightness, max_level);
}

// Test that we pick the one with the greatest granularity
TEST_F(InternalBacklightTest, GranularityTest) {
  base::FilePath this_test_path = test_path_.Append("granularity_test");

  // Make sure the middle one is the most granular so we're not just
  // getting lucky.  Middle in terms of order created and alphabet, since I
  // don't know how enumaration might be happening.
  base::FilePath a_path = this_test_path.Append("a");
  PopulateBacklightDir(a_path, 10, 127, 11);
  base::FilePath b_path = this_test_path.Append("b");
  PopulateBacklightDir(b_path, 20, 255, 21);
  base::FilePath c_path = this_test_path.Append("c");
  PopulateBacklightDir(c_path, 30, 63, 31);

  InternalBacklight backlight;
  ASSERT_TRUE(backlight.Init(this_test_path, "*"));

  int64 level = 0;
  EXPECT_TRUE(backlight.GetCurrentBrightnessLevel(&level));
  EXPECT_EQ(21, level);

  int64 max_level = 0;
  EXPECT_TRUE(backlight.GetMaxBrightnessLevel(&max_level));
  EXPECT_EQ(255, max_level);
}

// Test ignore directories starting with a "."
TEST_F(InternalBacklightTest, NoDotDirsTest) {
  base::FilePath this_test_path = test_path_.Append("no_dot_dirs_test");

  // We'll just create one dir and it will have a dot in it.  Then, we can
  // be sure that we didn't just get luckly...
  base::FilePath my_path = this_test_path.Append(".pwm-backlight");
  PopulateBacklightDir(my_path, 128, 255, 127);

  InternalBacklight backlight;
  EXPECT_FALSE(backlight.Init(this_test_path, "*"));
}

// Test that the glob is working correctly for searching for backlight dirs.
TEST_F(InternalBacklightTest, GlobTest) {
  base::FilePath this_test_path = test_path_.Append("glob_test");

  // Purposely give my::kbd_backlight a lower "max_level" than
  // .no::kbd_backlight so that we know that dirs starting with a "." are
  // ignored.
  base::FilePath my_path = this_test_path.Append("my::kbd_backlight");
  PopulateBacklightDir(my_path, 1, 2, -1);

  base::FilePath ignore1_path = this_test_path.Append("ignore1");
  PopulateBacklightDir(ignore1_path, 3, 4, -1);

  base::FilePath ignore2_path = this_test_path.Append(".no::kbd_backlight");
  PopulateBacklightDir(ignore2_path, 5, 6, -1);

  InternalBacklight backlight;
  ASSERT_TRUE(backlight.Init(this_test_path, "*:kbd_backlight"));

  int64 level = 0;
  EXPECT_TRUE(backlight.GetCurrentBrightnessLevel(&level));
  EXPECT_EQ(1, level);

  int64 max_level = 0;
  EXPECT_TRUE(backlight.GetMaxBrightnessLevel(&max_level));
  EXPECT_EQ(2, max_level);
}

TEST_F(InternalBacklightTest, Transitions) {
  const int kMaxBrightness = 100;
  base::FilePath backlight_dir = test_path_.Append("transitions_test");
  PopulateBacklightDir(backlight_dir, 50, kMaxBrightness, 50);

  InternalBacklight backlight;
  const base::TimeTicks kStartTime = base::TimeTicks::FromInternalValue(10000);
  backlight.clock()->set_current_time_for_testing(kStartTime);
  ASSERT_TRUE(backlight.Init(test_path_, "*"));

  // An instant transition to the maximum level shouldn't use a timer.
  backlight.SetBrightnessLevel(kMaxBrightness, base::TimeDelta());
  EXPECT_FALSE(backlight.transition_timeout_is_set());
  EXPECT_EQ(kMaxBrightness, ReadBrightness(backlight_dir));
  EXPECT_EQ(kMaxBrightness, GetCurrentBrightness(&backlight));

  // Start a transition to the backlight's halfway point.
  const int64 kHalfBrightness = kMaxBrightness / 2;
  const base::TimeDelta kDuration = base::TimeDelta::FromMilliseconds(1000);
  backlight.SetBrightnessLevel(kHalfBrightness, kDuration);

  // If the timeout fires at this point, we should still be at the maximum
  // level.
  EXPECT_TRUE(backlight.transition_timeout_is_set());
  EXPECT_TRUE(backlight.TriggerTransitionTimeoutForTesting());
  EXPECT_EQ(kMaxBrightness, ReadBrightness(backlight_dir));
  EXPECT_EQ(kMaxBrightness, GetCurrentBrightness(&backlight));

  // Let half of the transition duration pass.
  const base::TimeTicks kMidpointTime = kStartTime + kDuration / 2;
  backlight.clock()->set_current_time_for_testing(kMidpointTime);
  EXPECT_TRUE(backlight.TriggerTransitionTimeoutForTesting());
  const int64 kMidpointBrightness = (kMaxBrightness + kHalfBrightness) / 2;
  EXPECT_EQ(kMidpointBrightness, ReadBrightness(backlight_dir));
  EXPECT_EQ(kMidpointBrightness, GetCurrentBrightness(&backlight));

  // At the end of the transition, we should return false to cancel the timeout.
  const base::TimeTicks kEndTime = kStartTime + kDuration;
  backlight.clock()->set_current_time_for_testing(kEndTime);
  EXPECT_FALSE(backlight.TriggerTransitionTimeoutForTesting());
  EXPECT_FALSE(backlight.transition_timeout_is_set());
  EXPECT_EQ(kHalfBrightness, ReadBrightness(backlight_dir));
  EXPECT_EQ(kHalfBrightness, GetCurrentBrightness(&backlight));
}

}  // namespace system
}  // namespace power_manager
