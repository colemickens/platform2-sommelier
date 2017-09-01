// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/* Libary to provide access to the Chrome OS master configuration */

#include <base/files/file_path.h>
#include <base/logging.h>
#include <chromeos-config/libcros_config/cros_config.h>
#include <gtest/gtest.h>

class CrosConfigTest : public testing::Test {
 protected:
  void InitConfig(const std::string model = "pyro") {
    base::FilePath filepath("test.dtb");
    ASSERT_TRUE(cros_config_.InitForTest(filepath, model));
  }

  brillo::CrosConfig cros_config_;
};

TEST_F(CrosConfigTest, CheckMissingFile) {
  base::FilePath filepath("invalid-file");
  ASSERT_FALSE(cros_config_.InitForTest(filepath, "pyro"));
  ASSERT_FALSE(cros_config_.InitForHost(filepath, "pyro"));
}

TEST_F(CrosConfigTest, CheckBadFile) {
  base::FilePath filepath("test.dts");
  ASSERT_FALSE(cros_config_.InitForTest(filepath, "pyro"));
  ASSERT_FALSE(cros_config_.InitForHost(filepath, "pyro"));
}

TEST_F(CrosConfigTest, CheckBadStruct) {
  base::FilePath filepath("test_bad_struct.dtb");
  ASSERT_FALSE(cros_config_.InitForTest(filepath, "pyto"));
  ASSERT_FALSE(cros_config_.InitForHost(filepath, "pyto"));
}

TEST_F(CrosConfigTest, CheckUnknownModel) {
  base::FilePath filepath("test.dtb");
  ASSERT_FALSE(cros_config_.InitForTest(filepath, "no-model"));
  ASSERT_FALSE(cros_config_.InitForHost(filepath, "no-model"));
}

TEST_F(CrosConfigTest, Check111NoInit) {
  std::string val;
  ASSERT_FALSE(cros_config_.GetString("/", "wallpaper", &val));
}

TEST_F(CrosConfigTest, CheckModelNamesNoInit) {
  std::string val;
  ASSERT_EQ(cros_config_.GetModelNames().size(), 0);
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
  ASSERT_TRUE(cros_config_.GetString("/firmware", "bcs-overlay", &val));
  ASSERT_EQ("overlay-reef-private", val);
}

TEST_F(CrosConfigTest, CheckGetModelNames) {
  InitConfig();
  std::vector<std::string> models = cros_config_.GetModelNames();
  ASSERT_EQ(models.size(), 7);
  ASSERT_EQ(models[0], "pyro");
  ASSERT_EQ(models[1], "caroline");
  ASSERT_EQ(models[2], "reef");
  ASSERT_EQ(models[3], "broken");
  ASSERT_EQ(models[4], "whitetip");
  ASSERT_EQ(models[5], "whitetip1");
  ASSERT_EQ(models[6], "whitetip2");
}

TEST_F(CrosConfigTest, CheckWhiteLabel) {
  // These mirror the tests in libcros_config_host_unittest testWhitelabel()
  InitConfig("whitetip1");
  std::string val;

  // These are defined by whitetip1 itself.
  ASSERT_TRUE(cros_config_.GetString("/", "wallpaper", &val));
  ASSERT_EQ("shark", val);
  ASSERT_TRUE(cros_config_.GetString("/firmware", "key-id", &val));
  ASSERT_EQ("WHITETIP1", val);

  // This is in a subnode defined by whitetip.
  ASSERT_TRUE(cros_config_.GetString("/touch", "present", &val));
  ASSERT_EQ("yes", val);

  // This is in the main node, but defined by whitetip
  ASSERT_TRUE(cros_config_.GetString("/", "powerd-prefs", &val));
  ASSERT_EQ("whitetip", val);

  // This is defined by whitetip's shared firmware. We don't have access to this
  // at run-time since we don't follow the 'shares' phandles.
  ASSERT_FALSE(cros_config_.GetString("/firmware/build-targets", "coreboot",
                                      &val));
}

int main(int argc, char **argv) {
  int status = system("exec ./chromeos-config-test-setup.sh");
  if (status != 0)
    return EXIT_FAILURE;

  logging::LoggingSettings settings;
  settings.logging_dest = logging::LOG_TO_FILE;
  settings.log_file = "log.test";
  settings.lock_log = logging::DONT_LOCK_LOG_FILE;
  settings.delete_old = logging::APPEND_TO_OLD_LOG_FILE;
  logging::InitLogging(settings);

  testing::InitGoogleTest(&argc, argv);

  return RUN_ALL_TESTS();
}
