// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/*
 * Tests for the CrosConfig library, which provides access to the Chrome OS
 * master configuration.
 */

#include <stdlib.h>

#include <base/files/file_path.h>
#include <base/logging.h>
#include <chromeos-config/libcros_config/cros_config.h>
#include <gtest/gtest.h>

#define TEST_FILE "test.json"
#define TEST_FILE_ARM "test_arm.json"

class CrosConfigTest : public testing::Test {
 protected:
  void InitConfig(const std::string name = "Another",
                  int sku_id = -1,
                  std::string whitelabel_name = "") {
    base::FilePath filepath(TEST_FILE);
    ASSERT_TRUE(
        cros_config_.InitForTestX86(filepath, name, sku_id, whitelabel_name));
  }

  void InitConfigArm(const std::string device_name = "google,some",
                     int sku_id = -1,
                     std::string whitelabel_name = "") {
    base::FilePath filepath(TEST_FILE_ARM);
    ASSERT_TRUE(
        cros_config_.InitForTestArm(filepath, device_name,
                                    sku_id, whitelabel_name));
  }

  brillo::CrosConfig cros_config_;
};

TEST_F(CrosConfigTest, CheckMissingFile) {
  base::FilePath filepath("invalid-file");
  ASSERT_FALSE(cros_config_.InitForTestX86(filepath, "Another", -1, ""));
}

TEST_F(CrosConfigTest, CheckUnknownModel) {
  base::FilePath filepath(TEST_FILE);
  ASSERT_FALSE(cros_config_.InitForTestX86(filepath, "no-model", -1, ""));
}

TEST_F(CrosConfigTest, Check111NoInit) {
  std::string val;
  ASSERT_FALSE(cros_config_.GetString("/", "wallpaper", &val));
}

TEST_F(CrosConfigTest, CheckWrongPath) {
  InitConfig();
  std::string val;
  ASSERT_FALSE(cros_config_.GetString("/wibble", "wallpaper", &val));
}

TEST_F(CrosConfigTest, CheckBadString) {
  InitConfig();
  std::string val;
  ASSERT_FALSE(cros_config_.GetString("/", "string-list", &val));
}

TEST_F(CrosConfigTest, CheckGoodStringRoot) {
  InitConfig();
  std::string val;
  ASSERT_TRUE(cros_config_.GetString("/", "wallpaper", &val));
  ASSERT_EQ("default", val);
}

TEST_F(CrosConfigTest, CheckGoodStringNonRoot) {
  InitConfig();
  std::string val;
  ASSERT_TRUE(cros_config_.GetString("/touch", "present", &val));
  ASSERT_EQ("probe", val);
}

TEST_F(CrosConfigTest, CheckEmptyPathError) {
  InitConfig();
  std::string val;
  ASSERT_FALSE(cros_config_.GetString("", "wallpaper", &val));
  ASSERT_EQ("", val);
}

TEST_F(CrosConfigTest, CheckPathWithoutSlashError) {
  InitConfig();
  std::string val;
  ASSERT_FALSE(cros_config_.GetString("noslash", "wallpaper", &val));
  ASSERT_EQ("", val);
}

TEST_F(CrosConfigTest, CheckUiPowerPosition) {
  InitConfig("Some", 1);
  std::string val;
  ASSERT_TRUE(cros_config_.GetString("/ui/power-button", "edge", &val));
  ASSERT_EQ("left", val);
  ASSERT_TRUE(cros_config_.GetString("/ui/power-button", "position", &val));
  ASSERT_EQ("0.3", val);
}

TEST_F(CrosConfigTest, CheckCameraCount) {
  InitConfig("Some", 0);
  std::string val;
  ASSERT_TRUE(cros_config_.GetString("/camera", "count", &val));
  ASSERT_EQ("1", val);
}

int main(int argc, char** argv) {
  int status = system("exec ./chromeos-config-test-setup.sh");
  if (status != 0)
    return EXIT_FAILURE;

  logging::LoggingSettings settings;
  settings.logging_dest = logging::LOG_TO_FILE;
  settings.log_file = "log.test";
  settings.lock_log = logging::DONT_LOCK_LOG_FILE;
  settings.delete_old = logging::DELETE_OLD_LOG_FILE;
  logging::InitLogging(settings);
  logging::SetMinLogLevel(-3);

  testing::InitGoogleTest(&argc, argv);

  return RUN_ALL_TESTS();
}
