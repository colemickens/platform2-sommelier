// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Tests for cros_boot_mode::BootloaderType default behavior.
#include "bootloader_type.h"

#include <base/basictypes.h>
#include <base/file_util.h>
#include <base/stringprintf.h>
#include <gtest/gtest.h>
#include <string>

class BootloaderTypeTest : public ::testing::Test {
 public:
  void SetUp() {
    EXPECT_TRUE(file_util::CreateNewTempDirectory("", &temp_dir_));
    type_file_path_ = temp_dir_.value();
    type_file_path_.append("cmdline");
    type_.set_platform_file_path(type_file_path_.c_str());
  }
  void TearDown() {
    file_util::Delete(temp_dir_, true);
  }

  void ExpectType(int type) {
    static const char *kUnsupported = "unsupported";
    EXPECT_EQ(type, type_.value());
    // If the position is -1, it is does not index.
    const char *expected_c_str = kUnsupported;
    if (type >= 0)
      expected_c_str =
        cros_boot_mode::BootloaderType::kBootloaderTypeText[type];
    EXPECT_STREQ(expected_c_str, type_.c_str());
  }
 protected:
   std::string type_file_path_;
   FilePath temp_dir_;
   cros_boot_mode::BootloaderType type_;
};

TEST_F(BootloaderTypeTest, ChromeOSBare) {
  std::string contents = StringPrintf("%s",
    cros_boot_mode::BootloaderType::kSupportedBootloaders[
      cros_boot_mode::BootloaderType::kChromeOS]);

  EXPECT_EQ(file_util::WriteFile(FilePath(type_file_path_),
                                 contents.c_str(), contents.length()),
            contents.length());
  type_.Initialize();
  ExpectType(cros_boot_mode::BootloaderType::kChromeOS);
}

TEST_F(BootloaderTypeTest, ChromeOSSpaces) {
  std::string contents = StringPrintf(" %s ",
    cros_boot_mode::BootloaderType::kSupportedBootloaders[
      cros_boot_mode::BootloaderType::kChromeOS]);

  EXPECT_EQ(file_util::WriteFile(FilePath(type_file_path_),
                                 contents.c_str(), contents.length()),
            contents.length());
  type_.Initialize();
  ExpectType(cros_boot_mode::BootloaderType::kChromeOS);
}

TEST_F(BootloaderTypeTest, NoBoundaries) {
  std::string contents = StringPrintf("x%sx",
    cros_boot_mode::BootloaderType::kSupportedBootloaders[
      cros_boot_mode::BootloaderType::kChromeOS]);

  EXPECT_EQ(file_util::WriteFile(FilePath(type_file_path_),
                                 contents.c_str(), contents.length()),
            contents.length());
  type_.Initialize();
  ExpectType(cros_boot_mode::BootloaderType::kUnsupported);
}

TEST_F(BootloaderTypeTest, NoStartingBoundary) {
  std::string contents = StringPrintf("x%s",
    cros_boot_mode::BootloaderType::kSupportedBootloaders[
      cros_boot_mode::BootloaderType::kChromeOS]);

  EXPECT_EQ(file_util::WriteFile(FilePath(type_file_path_),
                                 contents.c_str(), contents.length()),
            contents.length());
  type_.Initialize();
  ExpectType(cros_boot_mode::BootloaderType::kUnsupported);
}

TEST_F(BootloaderTypeTest, NoTrailingBoundary) {
  std::string contents = StringPrintf("%sx",
    cros_boot_mode::BootloaderType::kSupportedBootloaders[
      cros_boot_mode::BootloaderType::kChromeOS]);

  EXPECT_EQ(file_util::WriteFile(FilePath(type_file_path_),
                                 contents.c_str(), contents.length()),
            contents.length());
  type_.Initialize();
  ExpectType(cros_boot_mode::BootloaderType::kUnsupported);
}

TEST_F(BootloaderTypeTest, FirstMatchInEnumOrderIsUsed) {
  std::string contents = StringPrintf(" %s %s ",
    cros_boot_mode::BootloaderType::kSupportedBootloaders[
      cros_boot_mode::BootloaderType::kChromeOS],
    cros_boot_mode::BootloaderType::kSupportedBootloaders[
      cros_boot_mode::BootloaderType::kDebug]);

  EXPECT_EQ(file_util::WriteFile(FilePath(type_file_path_),
                                 contents.c_str(), contents.length()),
            contents.length());
  type_.Initialize();
  ExpectType(cros_boot_mode::BootloaderType::kDebug);
}

