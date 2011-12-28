// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>
#include <fstream>

#include <gtest/gtest.h>
#include <inttypes.h>

#include "base/file_util.h"
#include "base/logging.h"
#include "power_manager/backlight.h"

using ::testing::Test;

namespace power_manager {

class BacklightTest : public Test {
 public:
  BacklightTest() {}

  virtual void SetUp() {
    CHECK(file_util::CreateNewTempDirectory("backlight_unittest", &test_path_));
  }

  virtual void TearDown() {
    file_util::Delete(test_path_, true);
  }

  // Create files to make the given directory look like it is a sysfs backlight
  // dir.
  void PopulateBacklightDir(const FilePath &path,
                            int64 brightness,
                            int64 max_brightness,
                            int64 actual_brightness) {
    CHECK(file_util::CreateDirectory(path));

    FILE* brightness_file = file_util::OpenFile(path.Append("brightness"), "w");
    fprintf(brightness_file, "%"PRId64"\n", brightness);
    file_util::CloseFile(brightness_file);

    FILE* max_brightness_file =
        file_util::OpenFile(path.Append("max_brightness"), "w");
    fprintf(max_brightness_file, "%"PRId64"\n", max_brightness);
    file_util::CloseFile(max_brightness_file);

    if (actual_brightness >= 0) {
      FILE* actual_brightness_file =
          file_util::OpenFile(path.Append("actual_brightness"), "w");
      fprintf(actual_brightness_file, "%"PRId64"\n", actual_brightness);
      file_util::CloseFile(actual_brightness_file);
    }
  }

 protected:
  FilePath test_path_;
};

// A basic test of functionality
TEST_F(BacklightTest, BasicTest) {
  FilePath this_test_path = test_path_.Append("basic_test");
  const int64 kBrightness = 128;
  const int64 kMaxBrightness = 255;
  const int64 kActualBrightness = 127;

  FilePath my_path = this_test_path.Append("pwm-backlight");
  PopulateBacklightDir(my_path, kBrightness, kMaxBrightness, kActualBrightness);

  Backlight backlight;
  ASSERT_TRUE(backlight.Init(this_test_path, "*"));

  int64 level = 0;
  EXPECT_TRUE(backlight.GetCurrentBrightnessLevel(&level));
  EXPECT_EQ(kActualBrightness, level);

  int64 max_level = 0;
  EXPECT_TRUE(backlight.GetMaxBrightnessLevel(&max_level));
  EXPECT_EQ(kMaxBrightness, max_level);
}

// Make sure things work OK when there is no actual_brightness file.
TEST_F(BacklightTest, NoActualBrightnessTest) {
  FilePath this_test_path = test_path_.Append("no_actual_brightness_test");
  const int64 kBrightness = 128;
  const int64 kMaxBrightness = 255;

  FilePath my_path = this_test_path.Append("pwm-backlight");
  PopulateBacklightDir(my_path, kBrightness, kMaxBrightness, -1);

  Backlight backlight;
  ASSERT_TRUE(backlight.Init(this_test_path, "*"));

  int64 level = 0;
  EXPECT_TRUE(backlight.GetCurrentBrightnessLevel(&level));
  EXPECT_EQ(kBrightness, level);

  int64 max_level = 0;
  EXPECT_TRUE(backlight.GetMaxBrightnessLevel(&max_level));
  EXPECT_EQ(kMaxBrightness, max_level);
}

// Test that we pick the one with the greatest granularity
TEST_F(BacklightTest, GranularityTest) {
  FilePath this_test_path = test_path_.Append("granularity_test");

  // Make sure the middle one is the most granular so we're not just
  // getting lucky.  Middle in terms of order created and alphabet, since I
  // don't know how enumaration might be happening.
  FilePath a_path = this_test_path.Append("a");
  PopulateBacklightDir(a_path, 10, 127, 11);
  FilePath b_path = this_test_path.Append("b");
  PopulateBacklightDir(b_path, 20, 255, 21);
  FilePath c_path = this_test_path.Append("c");
  PopulateBacklightDir(c_path, 30, 63, 31);

  Backlight backlight;
  ASSERT_TRUE(backlight.Init(this_test_path, "*"));

  int64 level = 0;
  EXPECT_TRUE(backlight.GetCurrentBrightnessLevel(&level));
  EXPECT_EQ(21, level);

  int64 max_level = 0;
  EXPECT_TRUE(backlight.GetMaxBrightnessLevel(&max_level));
  EXPECT_EQ(255, max_level);
}

// Test ignore directories starting with a "."
TEST_F(BacklightTest, NoDotDirsTest) {
  FilePath this_test_path = test_path_.Append("no_dot_dirs_test");

  // We'll just create one dir and it will have a dot in it.  Then, we can
  // be sure that we didn't just get luckly...
  FilePath my_path = this_test_path.Append(".pwm-backlight");
  PopulateBacklightDir(my_path, 128, 255, 127);

  Backlight backlight;
  EXPECT_FALSE(backlight.Init(this_test_path, "*"));
}

// Test that the glob is working correctly for searching for backlight dirs.
TEST_F(BacklightTest, GlobTest) {
  FilePath this_test_path = test_path_.Append("glob_test");

  // Purposely give my::kbd_backlight a lower "max_level" than
  // .no::kbd_backlight so that we know that dirs starting with a "." are
  // ignored.
  FilePath my_path = this_test_path.Append("my::kbd_backlight");
  PopulateBacklightDir(my_path, 1, 2, -1);

  FilePath ignore1_path = this_test_path.Append("ignore1");
  PopulateBacklightDir(ignore1_path, 3, 4, -1);

  FilePath ignore2_path = this_test_path.Append(".no::kbd_backlight");
  PopulateBacklightDir(ignore2_path, 5, 6, -1);

  Backlight backlight;
  ASSERT_TRUE(backlight.Init(this_test_path, "*:kbd_backlight"));

  int64 level = 0;
  EXPECT_TRUE(backlight.GetCurrentBrightnessLevel(&level));
  EXPECT_EQ(1, level);

  int64 max_level = 0;
  EXPECT_TRUE(backlight.GetMaxBrightnessLevel(&max_level));
  EXPECT_EQ(2, max_level);
}

}  // namespace power_manager
