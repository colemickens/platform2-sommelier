// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/system/suspend_configurator.h"

#include <stdint.h>
#include <string>

#include <base/files/file_util.h>
#include <base/files/scoped_temp_dir.h>
#include <gtest/gtest.h>

#include "power_manager/common/fake_prefs.h"
#include "power_manager/common/power_constants.h"

namespace power_manager {
namespace system {

class SuspendConfiguratorTest : public ::testing::Test {
 public:
  SuspendConfiguratorTest() {
    // Temporary directory mimicking a root directory.
    CHECK(temp_root_dir_.CreateUniqueTempDir());
    suspend_configurator_.set_prefix_path_for_testing(temp_root_dir_.GetPath());

    base::FilePath console_suspend_path = temp_root_dir_.GetPath().Append(
        SuspendConfigurator::kConsoleSuspendPath.value().substr(1));
    CHECK(base::CreateDirectory(console_suspend_path.DirName()));
    // Creates empty file.
    CHECK_EQ(base::WriteFile(console_suspend_path, "", 0), 0);
  }

  ~SuspendConfiguratorTest() override = default;

 protected:
  // Returns |orig| rooted within the temporary root dir created for testing.
  base::FilePath GetPath(const base::FilePath& orig) const {
    return temp_root_dir_.GetPath().Append(orig.value().substr(1));
  }

  base::ScopedTempDir temp_root_dir_;
  FakePrefs prefs_;
  SuspendConfigurator suspend_configurator_;
};

// Test console is enabled during supend to S3 by default.
TEST_F(SuspendConfiguratorTest, TestDefaultConsoleSuspendForS3) {
  base::FilePath console_suspend_path =
      GetPath(SuspendConfigurator::kConsoleSuspendPath);
  prefs_.SetInt64(kSuspendToIdlePref, 0);
  suspend_configurator_.Init(&prefs_);
  std::string file_contents;
  ASSERT_TRUE(base::ReadFileToString(console_suspend_path, &file_contents));
  // Make sure console is enabled if system suspends to S3.
  EXPECT_EQ("N", file_contents);
}

// Test console is disabled during supend to S0iX by default.
TEST_F(SuspendConfiguratorTest, TestDefaultConsoleSuspendForS0ix) {
  base::FilePath console_suspend_path =
      GetPath(SuspendConfigurator::kConsoleSuspendPath);
  prefs_.SetInt64(kSuspendToIdlePref, 1);
  suspend_configurator_.Init(&prefs_);
  std::string file_contents;
  ASSERT_TRUE(base::ReadFileToString(console_suspend_path, &file_contents));
  // Make sure console is disabled if S0ix is enabled.
  EXPECT_EQ("Y", file_contents);
}

// Test default value to suspend console is overwritten if
// |kEnableConsoleDuringSuspendPref| is set.
TEST_F(SuspendConfiguratorTest, TestDefaultConsoleSuspendOverwritten) {
  base::FilePath console_suspend_path =
      GetPath(SuspendConfigurator::kConsoleSuspendPath);
  prefs_.SetInt64(kSuspendToIdlePref, 1);
  prefs_.SetInt64(kEnableConsoleDuringSuspendPref, 1);
  suspend_configurator_.Init(&prefs_);
  std::string file_contents;
  ASSERT_TRUE(base::ReadFileToString(console_suspend_path, &file_contents));
  // Make sure console is not disabled though the default is to disable it.
  EXPECT_EQ("N", file_contents);
}

}  // namespace system
}  // namespace power_manager
