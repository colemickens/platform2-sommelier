// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Tests for cros_boot_mode::DeveloperSwitch default behavior.
// This also acts as the canonical unittest for PlatformSwitch default
// behavior.
#include "developer_switch.h"

#include <base/basictypes.h>
#include <base/file_util.h>
#include <gtest/gtest.h>
#include <string>

class DeveloperSwitchTest : public ::testing::Test {
 public:
  void SetUp() {
    EXPECT_TRUE(file_util::CreateNewTempDirectory("", &temp_dir_));
    switch_file_path_ = temp_dir_.value();
    switch_file_path_.append("chsw");
    switch_.set_platform_file_path(switch_file_path_.c_str());
  }

  void TearDown() {
    file_util::Delete(temp_dir_, true);
  }

  void ExpectPosition(int pos) {
    static const char *kUnsupported = "unsupported";
    EXPECT_EQ(pos, switch_.value());
    // If the position is -1, it is does not index.
    const char *expected_c_str = kUnsupported;
    if (pos >= 0)
      expected_c_str = cros_boot_mode::PlatformSwitch::kPositionText[pos];
    EXPECT_STREQ(expected_c_str, switch_.c_str());
  }
 protected:
   std::string switch_file_path_;
   FilePath temp_dir_;
   cros_boot_mode::DeveloperSwitch switch_;
};


TEST_F(DeveloperSwitchTest, Disabled) {
  EXPECT_EQ(file_util::WriteFile(FilePath(switch_file_path_), "0", 2), 2);
  switch_.Initialize();
  ExpectPosition(cros_boot_mode::PlatformSwitch::kDisabled);
}

TEST_F(DeveloperSwitchTest, DisabledWithOtherSwitches) {
  EXPECT_EQ(file_util::WriteFile(FilePath(switch_file_path_), "528", 4), 4);
  switch_.Initialize();
  ExpectPosition(cros_boot_mode::PlatformSwitch::kDisabled);
}

TEST_F(DeveloperSwitchTest, Enabled) {
  EXPECT_EQ(file_util::WriteFile(FilePath(switch_file_path_), "32", 3), 3);
  switch_.Initialize();
  ExpectPosition(cros_boot_mode::PlatformSwitch::kEnabled);
}

TEST_F(DeveloperSwitchTest, EnabledWithOtherSwitches) {
  EXPECT_EQ(file_util::WriteFile(FilePath(switch_file_path_), "544", 4), 4);
  switch_.Initialize();
  ExpectPosition(cros_boot_mode::PlatformSwitch::kEnabled);
}

TEST_F(DeveloperSwitchTest, EnabledWithOtherSwitches2) {
  EXPECT_EQ(file_util::WriteFile(FilePath(switch_file_path_), "4128", 5), 5);
  switch_.Initialize();
  ExpectPosition(cros_boot_mode::PlatformSwitch::kEnabled);
}

TEST_F(DeveloperSwitchTest, EnabledWithOtherSwitches3) {
  EXPECT_EQ(file_util::WriteFile(FilePath(switch_file_path_), "65535", 6), 6);
  switch_.Initialize();
  ExpectPosition(cros_boot_mode::PlatformSwitch::kEnabled);
}

TEST_F(DeveloperSwitchTest, BadTruncationMakesUnsupported) {
  EXPECT_EQ(file_util::WriteFile(FilePath(switch_file_path_), "100000", 7), 7);
  switch_.Initialize();
  ExpectPosition(cros_boot_mode::PlatformSwitch::kUnsupported);
}

TEST_F(DeveloperSwitchTest, MissingFile) {
  switch_.Initialize();
  ExpectPosition(cros_boot_mode::PlatformSwitch::kUnsupported);
}

TEST_F(DeveloperSwitchTest, UnsupportedPlatform) {
  EXPECT_EQ(file_util::WriteFile(FilePath(switch_file_path_), "-1", 3), 3);
  switch_.Initialize();
  ExpectPosition(cros_boot_mode::PlatformSwitch::kUnsupported);
}

TEST_F(DeveloperSwitchTest, EmptyFile) {
  EXPECT_EQ(file_util::WriteFile(FilePath(switch_file_path_), "", 0), 0);
  switch_.Initialize();
  ExpectPosition(cros_boot_mode::PlatformSwitch::kUnsupported);
}
