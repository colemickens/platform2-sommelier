// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Tests for cros_boot_mode::BootMode

#include "boot_mode.h"

#include <base/basictypes.h>
#include <base/file_util.h>
#include <base/stringprintf.h>
#include <gtest/gtest.h>
#include <string>

class BootModeTest : public ::testing::Test {
 public:
  void SetUp() {
    EXPECT_TRUE(file_util::CreateNewTempDirectory("", &temp_dir_));

    developer_switch_path_ = temp_dir_.value();
    developer_switch_path_.append("/CHSW");
    developer_switch_.set_platform_file_path(developer_switch_path_.c_str());
    boot_mode_.set_developer_switch(&developer_switch_);

    bootloader_type_path_ = temp_dir_.value();
    bootloader_type_path_.append("/cmdline");
    bootloader_type_.set_platform_file_path(bootloader_type_path_.c_str());
    boot_mode_.set_bootloader_type(&bootloader_type_);

    firmware_path_ = temp_dir_.value();
    firmware_path_.append("/BINF.1");
    active_main_firmware_.set_platform_file_path(firmware_path_.c_str());
    boot_mode_.set_active_main_firmware(&active_main_firmware_);
  }

  ~BootModeTest() {
    TearDown();
  }
  void TearDown() {
    if (!temp_dir_.empty())
      file_util::Delete(temp_dir_, true);
    temp_dir_.clear();
  }

  // In SetUp, the temporary files were created and the paths injected
  // into the underlying classes.  This helper function wraps updating
  // each file.  |chsw| contains the dev mode switch value. |fw| contains
  // the active firmware number, and the |cmdline| is the /proc/cmdline
  // that the "system" booted in.
  void UpdateFiles(int chsw, int fw, const char *cmdline) {
    std::string data = StringPrintf("%d", chsw);
    EXPECT_EQ(
      file_util::WriteFile(FilePath(developer_switch_.platform_file_path()),
                           data.c_str(),
                           data.length()), data.length());

    data = StringPrintf("%d", fw);
    EXPECT_EQ(
      file_util::WriteFile(FilePath(active_main_firmware_.platform_file_path()),
                           data.c_str(),
                           data.length()), data.length());

    data.assign(cmdline);
    EXPECT_EQ(
      file_util::WriteFile(FilePath(bootloader_type_.platform_file_path()),
                           data.c_str(),
                           data.length()), data.length());

  }

  static const char *kUnsupportedText;
  static const char *kNormalRecoveryText;
  static const char *kNormalText;
  static const char *kDeveloperText;
  static const char *kDeveloperRecoveryText;
  static const int kNormal = 0x0;
  static const int kDeveloper = 0x32;
  static const int kFwRecovery = 0;
  static const int kFwA = 1;
  static const int kFwB = 2;
  static const char *kCrosDebug;
  static const char *kCrosEfi;
  static const char *kCrosLegacy;
  static const char *kCrosSecure;
 protected:
   FilePath temp_dir_;
   cros_boot_mode::BootMode boot_mode_;
   cros_boot_mode::DeveloperSwitch developer_switch_;
   std::string developer_switch_path_;
   cros_boot_mode::ActiveMainFirmware active_main_firmware_;
   std::string firmware_path_;
   cros_boot_mode::BootloaderType bootloader_type_;
   std::string bootloader_type_path_;
};

const char *BootModeTest::kUnsupportedText = "unsupported";
const char *BootModeTest::kNormalText = "normal";
const char *BootModeTest::kNormalRecoveryText = "normal recovery";
const char *BootModeTest::kDeveloperText = "developer";
const char *BootModeTest::kDeveloperRecoveryText = "developer recovery";
const char *BootModeTest::kCrosSecure =
  "some nonsense dm=\"dacros_securesd ada\" cros_secure";
const char *BootModeTest::kCrosLegacy =
  "some nonsense dm=\"dacros_securesd ada\" cros_legacy";
const char *BootModeTest::kCrosEfi =
  "some nonsense dm=\"dacros_securesd ada\" cros_efi";
const char *BootModeTest::kCrosDebug =
  "some nonsense dm=\"dacros_securesd ada\" cros_debug BOOT=hello";

TEST_F(BootModeTest, NoFilesUseBootloader) {
  boot_mode_.Initialize(false, true);
  EXPECT_EQ(cros_boot_mode::BootMode::kUnsupported, boot_mode_.mode());
  EXPECT_STREQ(kUnsupportedText, boot_mode_.mode_text());
}

TEST_F(BootModeTest, UnsupportedAsDeveloper) {
  boot_mode_.Initialize(true, true);
  EXPECT_EQ(cros_boot_mode::BootMode::kDeveloper, boot_mode_.mode());
  EXPECT_STREQ(kDeveloperText, boot_mode_.mode_text());
}

TEST_F(BootModeTest, NormalRecoveryNothingIgnoreBootloader) {
  UpdateFiles(kNormal, kFwRecovery, " some kernel noise ");
  boot_mode_.Initialize(false, false);
  EXPECT_EQ(cros_boot_mode::BootMode::kNormalRecovery, boot_mode_.mode());
  EXPECT_STREQ(kNormalRecoveryText, boot_mode_.mode_text());
}

TEST_F(BootModeTest, NormalFwASecureUseBootloader) {
  UpdateFiles(kNormal, kFwA, kCrosSecure);
  boot_mode_.Initialize(false, true);
  EXPECT_EQ(cros_boot_mode::BootMode::kNormal, boot_mode_.mode());
  EXPECT_STREQ(kNormalText, boot_mode_.mode_text());
}

TEST_F(BootModeTest, NormalFwBSecureUseBootloader) {
  UpdateFiles(kNormal, kFwB, kCrosSecure);
  boot_mode_.Initialize(false, true);
  EXPECT_EQ(cros_boot_mode::BootMode::kNormal, boot_mode_.mode());
  EXPECT_STREQ(kNormalText, boot_mode_.mode_text());
}

TEST_F(BootModeTest, NormalFwASecureIgnoreBootloader) {
  UpdateFiles(kNormal, kFwA, kCrosSecure);
  boot_mode_.Initialize(false, false);
  EXPECT_EQ(cros_boot_mode::BootMode::kNormal, boot_mode_.mode());
  EXPECT_STREQ(kNormalText, boot_mode_.mode_text());
}

TEST_F(BootModeTest, NormalFwBSecureIgnoreBootloader) {
  UpdateFiles(kNormal, kFwB, kCrosSecure);
  boot_mode_.Initialize(false, false);
  EXPECT_EQ(cros_boot_mode::BootMode::kNormal, boot_mode_.mode());
  EXPECT_STREQ(kNormalText, boot_mode_.mode_text());
}

TEST_F(BootModeTest, BootloaderPreemptsHardwareUseBootloader) {
  UpdateFiles(kNormal, kFwA, " some kernel noise ");
  boot_mode_.Initialize(false, true);
  EXPECT_EQ(cros_boot_mode::BootMode::kUnsupported, boot_mode_.mode());
  EXPECT_STREQ(kUnsupportedText, boot_mode_.mode_text());
}

TEST_F(BootModeTest, BootloaderPreemptsHardwareUseBootloaderFwB) {
  UpdateFiles(kNormal, kFwB, " some kernel noise ");
  boot_mode_.Initialize(false, true);
  EXPECT_EQ(cros_boot_mode::BootMode::kUnsupported, boot_mode_.mode());
  EXPECT_STREQ(kUnsupportedText, boot_mode_.mode_text());
}

TEST_F(BootModeTest, NormalFwADebugUseBootloader) {
  UpdateFiles(kNormal, kFwA, kCrosDebug);
  boot_mode_.Initialize(false, true);
  EXPECT_EQ(cros_boot_mode::BootMode::kDeveloper, boot_mode_.mode());
  EXPECT_STREQ(kDeveloperText, boot_mode_.mode_text());
}

TEST_F(BootModeTest, NormalFwBDebugUseBootloader) {
  UpdateFiles(kNormal, kFwB, kCrosDebug);
  boot_mode_.Initialize(false, true);
  EXPECT_EQ(cros_boot_mode::BootMode::kDeveloper, boot_mode_.mode());
  EXPECT_STREQ(kDeveloperText, boot_mode_.mode_text());
}

TEST_F(BootModeTest, UnsupportedDebugUseBootloader) {
  UpdateFiles(-1, -1, kCrosDebug);
  boot_mode_.Initialize(false, true);
  EXPECT_EQ(cros_boot_mode::BootMode::kDeveloper, boot_mode_.mode());
  EXPECT_STREQ(kDeveloperText, boot_mode_.mode_text());
}

TEST_F(BootModeTest, EfiIsUnsuppportedUseBootloader) {
  UpdateFiles(-1, -1, kCrosEfi);
  boot_mode_.Initialize(false, true);
  EXPECT_EQ(cros_boot_mode::BootMode::kUnsupported, boot_mode_.mode());
  EXPECT_STREQ(kUnsupportedText, boot_mode_.mode_text());
}

TEST_F(BootModeTest, LegacyIsUnsuppportedUseBootloader) {
  UpdateFiles(-1, -1, kCrosLegacy);
  boot_mode_.Initialize(false, true);
  EXPECT_EQ(cros_boot_mode::BootMode::kUnsupported, boot_mode_.mode());
  EXPECT_STREQ(kUnsupportedText, boot_mode_.mode_text());
}

TEST_F(BootModeTest, DeveloperFwASecureUseBootloader) {
  UpdateFiles(kDeveloper, kFwA, kCrosSecure);
  boot_mode_.Initialize(false, true);
  EXPECT_EQ(cros_boot_mode::BootMode::kDeveloper, boot_mode_.mode());
  EXPECT_STREQ(kDeveloperText, boot_mode_.mode_text());
}

TEST_F(BootModeTest, DeveloperRecoverySecureUseBootloader) {
  UpdateFiles(kDeveloper, kFwRecovery, kCrosSecure);
  boot_mode_.Initialize(false, true);
  EXPECT_EQ(cros_boot_mode::BootMode::kDeveloperRecovery, boot_mode_.mode());
  EXPECT_STREQ(kDeveloperRecoveryText, boot_mode_.mode_text());
}
