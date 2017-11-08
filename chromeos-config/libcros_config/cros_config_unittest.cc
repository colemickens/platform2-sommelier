// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/*
 * Tests for the CrosConfig library, which provides access to the Chrome OS
 * master configuration.
 */

#include <base/files/file_path.h>
#include <base/logging.h>
#include <chromeos-config/libcros_config/cros_config.h>
#include <gtest/gtest.h>

class CrosConfigTest : public testing::Test {
 protected:
  void InitConfig(const std::string name = "Pyro", int sku_id = -1,
    std::string whitelabel_name = "") {
    base::FilePath filepath("test.dtb");
    ASSERT_TRUE(cros_config_.InitForTest(filepath, name, sku_id,
                                         whitelabel_name));
  }
  void CheckWhiteLabelAlternateSku(int sku_id);

  brillo::CrosConfig cros_config_;
};

TEST_F(CrosConfigTest, CheckMissingFile) {
  base::FilePath filepath("invalid-file");
  ASSERT_FALSE(cros_config_.InitForTest(filepath, "Pyro", -1, ""));
}

TEST_F(CrosConfigTest, CheckBadFile) {
  base::FilePath filepath("test.dts");
  ASSERT_FALSE(cros_config_.InitForTest(filepath, "Pyro", -1, ""));
}

TEST_F(CrosConfigTest, CheckBadStruct) {
  base::FilePath filepath("test_bad_struct.dtb");
  ASSERT_FALSE(cros_config_.InitForTest(filepath, "pyto", -1, ""));
}

TEST_F(CrosConfigTest, CheckUnknownModel) {
  base::FilePath filepath("test.dtb");
  ASSERT_FALSE(cros_config_.InitForTest(filepath, "no-model", -1, ""));
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
  ASSERT_TRUE(cros_config_.GetString("/firmware", "bcs-overlay", &val));
  ASSERT_EQ("overlay-pyro-private", val);
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

TEST_F(CrosConfigTest, CheckWhiteLabel) {
  // These mirror the tests in libcros_config_host_unittest testWhitelabel()
  InitConfig("Reef", 9);
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

  // We should get the same result using the base whitetip and a whitelabel tag.
  InitConfig("Reef", 8, "whitetip1");
  ASSERT_TRUE(cros_config_.GetString("/", "wallpaper", &val));
  ASSERT_EQ("shark", val);
}

TEST_F(CrosConfigTest, CheckAbsPath) {
  InitConfig("Pyro");
  std::string val;

  ASSERT_TRUE(cros_config_.GetAbsPath("/thermal", "dptf-dv", &val));
  ASSERT_EQ("/etc/dptf/pyro/dptf.dv", val);

  // This is not a PropFile.
  ASSERT_FALSE(cros_config_.GetAbsPath("/", "wallpaper", &val));

  // GetString() should still return the raw value.
  ASSERT_TRUE(cros_config_.GetString("/thermal", "dptf-dv", &val));
  ASSERT_EQ("pyro/dptf.dv", val);
}

TEST_F(CrosConfigTest, CheckDefault) {
  // These mirror the tests in libcros_config_host_unittest testWhitelabel()
  InitConfig("Reef", 20);
  std::string val;

  // These are defined by caroline itself.
  ASSERT_TRUE(cros_config_.GetString("/", "wallpaper", &val));
  ASSERT_EQ("caroline", val);
  ASSERT_TRUE(cros_config_.GetString("/audio/main", "cras-config-dir", &val));
  ASSERT_EQ("caroline", val);

  // This relies on a default property
  ASSERT_TRUE(cros_config_.GetString("/audio/main", "ucm-suffix", &val));
  ASSERT_EQ("pyro", val);
}

TEST_F(CrosConfigTest, CheckSubmodel) {
  InitConfig("Reef", 4);
  std::string val;
  ASSERT_TRUE(cros_config_.GetString("/touch", "present", &val));
  ASSERT_EQ("yes", val);
  ASSERT_TRUE(cros_config_.GetString("/audio/main", "ucm-suffix", &val));
  ASSERT_EQ("1mic", val);

  InitConfig("Reef", 5);
  ASSERT_TRUE(cros_config_.GetString("/touch", "present", &val));
  ASSERT_EQ("no", val);
  ASSERT_TRUE(cros_config_.GetString("/audio/main", "ucm-suffix", &val));
  ASSERT_EQ("2mic", val);
}

// Check a particular SKU ID can return information from the whitelabels {} node
// @sku_id: SKU ID to check
void CrosConfigTest::CheckWhiteLabelAlternateSku(int sku_id) {
  // Check values defined by blacktop1.
  InitConfig("Reef", sku_id, "blacktip1");
  std::string val;
  ASSERT_TRUE(cros_config_.GetString("/", "wallpaper", &val));
  ASSERT_EQ("dark", val);
  ASSERT_TRUE(cros_config_.GetString("/firmware", "key-id", &val));
  ASSERT_EQ("BLACKTIP1", val);
  ASSERT_TRUE(cros_config_.GetString("/", "brand-code", &val));
  ASSERT_EQ("HBBN", val);

  // Check values defined by blacktop2.
  InitConfig("Reef", sku_id, "blacktip2");
  ASSERT_TRUE(cros_config_.GetString("/", "wallpaper", &val));
  ASSERT_EQ("darker", val);
  ASSERT_TRUE(cros_config_.GetString("/firmware", "key-id", &val));
  ASSERT_EQ("BLACKTIP2", val);
  ASSERT_TRUE(cros_config_.GetString("/", "brand-code", &val));
  ASSERT_EQ("HBBO", val);
}

TEST_F(CrosConfigTest, CheckWhiteLabelAlternate) {
  InitConfig("Reef", 10);
  std::string val;

  // Check values defined by blacktop itself.
  ASSERT_FALSE(cros_config_.GetString("/", "wallpaper", &val));
  ASSERT_FALSE(cros_config_.GetString("/firmware", "key-id", &val));

  // Check that we can find whitelabel values using the model.
  CheckWhiteLabelAlternateSku(10);

  // Check the same thing with the two submodels. This should work since they
  // are orthogonal to the information in the whitelabels node.
  CheckWhiteLabelAlternateSku(11);
  CheckWhiteLabelAlternateSku(12);

  // Check that submodel values are unaffected by the alternative schema.
  InitConfig("Reef", 11, "blacktip1");
  ASSERT_TRUE(cros_config_.GetString("/touch", "present", &val));
  ASSERT_EQ("yes", val);

  InitConfig("Reef", 12, "blacktip1");
  ASSERT_TRUE(cros_config_.GetString("/touch", "present", &val));
  ASSERT_EQ("no", val);
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
