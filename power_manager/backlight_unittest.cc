// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>
#include <fstream>

#include <gtest/gtest.h>

#include "base/file_util.h"
#include "base/logging.h"
#include "power_manager/backlight.h"

using ::testing::Test;

namespace power_manager {

class BacklightTest : public Test {
 public:
  BacklightTest() {}

  virtual void SetUp() {
    file_util::CreateNewTempDirectory("backlight_unittest", &test_path_);
  }

  virtual void TearDown() {
    file_util::Delete(test_path_, true);
  }

  // Create files to make the given directory look like it is a sysfs backlight
  // dir.
  void PopulateBacklightDir(const FilePath &path, int64 brightness,
                                   int64 max_brightness,
                                   int64 actual_brightness=-1)
  {
    FILE* brightness_file = file_util::OpenFile(path.Append("brightness"), "w");
    fprintf(brightness_file, "%lld\n", brightness);
    file_util::CloseFile(brightness_file);

    FILE* max_brightness_file =
        file_util::OpenFile(path.Append("max_brightness"), "w");
    fprintf(max_brightness_file, "%lld\n", max_brightness);
    file_util::CloseFile(max_brightness_file);

    if (actual_brightness != -1) {
      FILE* actual_brightness_file =
          file_util::OpenFile(path.Append("actual_brightness"), "w");
      fprintf(actual_brightness_file, "%lld\n", actual_brightness);
      file_util::CloseFile(actual_brightness_file);
    }
  }

 protected:
  FilePath test_path_;
};

// A basic test of functionality
TEST_F(BacklightTest, BasicTest) {
  bool success;
  FilePath this_test_path = test_path_.Append("basic_test");

  // Just one backlight file
  FilePath my_path = this_test_path.Append("pwm-backlight");
  file_util::CreateDirectory(my_path);
  PopulateBacklightDir(my_path, 128, 255, 127);

  Backlight backlight;
  success = backlight.Init(this_test_path, "*");
  EXPECT_TRUE(success);
  int64 level, max_level;
  success = backlight.GetBrightness(&level, &max_level);

  EXPECT_EQ(level, 127);
  EXPECT_EQ(max_level, 255);
  EXPECT_TRUE(success);
}

// Make sure things work OK when there is no actual_brightness file.
TEST_F(BacklightTest, NoActualBrightnessTest) {
  bool success;
  FilePath this_test_path = test_path_.Append("no_actual_brightness_test");

  // Just one backlight file
  FilePath my_path = this_test_path.Append("pwm-backlight");
  file_util::CreateDirectory(my_path);
  PopulateBacklightDir(my_path, 128, 255, -1);

  Backlight backlight;
  success = backlight.Init(this_test_path, "*");
  EXPECT_TRUE(success);
  int64 level, max_level;
  success = backlight.GetBrightness(&level, &max_level);

  EXPECT_EQ(level, 128);
  EXPECT_EQ(max_level, 255);
  EXPECT_TRUE(success);
}

// Test that we pick the one with the greatest granularity
TEST_F(BacklightTest, GranulatiryTest) {
  bool success;
  FilePath this_test_path = test_path_.Append("granularity_test");

  // Make sure the middle one is the most granular so we're not just
  // getting lucky.  Middle in terms of order created and alphabet, since I
  // don't know how enumaration might be happening.
  FilePath a_path = this_test_path.Append("a");
  file_util::CreateDirectory(a_path);
  PopulateBacklightDir(a_path, 10, 127, 11);
  FilePath b_path = this_test_path.Append("b");
  file_util::CreateDirectory(b_path);
  PopulateBacklightDir(b_path, 20, 255, 21);
  FilePath c_path = this_test_path.Append("c");
  file_util::CreateDirectory(c_path);
  PopulateBacklightDir(c_path, 30, 63, 31);

  Backlight backlight;
  success = backlight.Init(this_test_path, "*");
  EXPECT_TRUE(success);
  int64 level, max_level;
  success = backlight.GetBrightness(&level, &max_level);

  EXPECT_EQ(level, 21);
  EXPECT_EQ(max_level, 255);
  EXPECT_TRUE(success);
}

// Test ignore directories starting with a "."
TEST_F(BacklightTest, NoDotDirsTest) {
  bool success;
  FilePath this_test_path = test_path_.Append("no_dot_dirs_test");

  // We'll just create one dir and it will have a dot in it.  Then, we can
  // be sure that we didn't just get luckly...
  FilePath my_path = this_test_path.Append(".pwm-backlight");
  file_util::CreateDirectory(my_path);
  PopulateBacklightDir(my_path, 128, 255, 127);

  Backlight backlight;
  success = backlight.Init(this_test_path, "*");
  EXPECT_FALSE(success);
}

// Test that the glob is working correctly for searching for backlight dirs.
TEST_F(BacklightTest, GlobTest) {
  bool success;
  FilePath this_test_path = test_path_.Append("glob_test");

  // Purposely give my::kbd_backlight a lower "max_level" than
  // .no::kbd_backlight so that we know that dirs starting with a "." are
  // ignored.
  FilePath my_path = this_test_path.Append("my::kbd_backlight");
  file_util::CreateDirectory(my_path);
  PopulateBacklightDir(my_path, 1, 2, -1);

  FilePath ignore1_path = this_test_path.Append("ignore1");
  file_util::CreateDirectory(ignore1_path);
  PopulateBacklightDir(ignore1_path, 3, 4, -1);

  FilePath ignore2_path = this_test_path.Append(".no::kbd_backlight");
  file_util::CreateDirectory(ignore2_path);
  PopulateBacklightDir(ignore2_path, 5, 6, -1);

  Backlight backlight;
  success = backlight.Init(this_test_path, "*:kbd_backlight");
  EXPECT_TRUE(success);
  int64 level, max_level;
  success = backlight.GetBrightness(&level, &max_level);

  EXPECT_EQ(level, 1);
  EXPECT_EQ(max_level, 2);
  EXPECT_TRUE(success);
}

}  // namespace power_manager
