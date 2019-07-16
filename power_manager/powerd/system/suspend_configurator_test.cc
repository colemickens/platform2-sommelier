// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/system/suspend_configurator.h"

#include <stdint.h>
#include <string>

#include <base/files/file_util.h>
#include <base/files/scoped_temp_dir.h>
#include <base/strings/string_number_conversions.h>
#include <gtest/gtest.h>

#include "power_manager/common/fake_prefs.h"
#include "power_manager/common/power_constants.h"

namespace power_manager {
namespace system {

namespace {
// Path to write to configure system suspend mode.
static constexpr char kSuspendModePath[] = "/sys/power/mem_sleep";

// suspend to idle (S0iX) suspend mode
static constexpr char kSuspendModeFreeze[] = "s2idle";

// shallow/standby(S1) suspend mode
static constexpr char kSuspendModeShallow[] = "shallow";

// deep sleep(S3) suspend mode
static constexpr char kSuspendModeDeep[] = "deep";

static constexpr char kECLastResumeResultPath[] =
    "/sys/kernel/debug/cros_ec/last_resume_result";

static constexpr char kECResumeResultHang[] = "0x80000001";
static constexpr char kECResumeResultNoHang[] = "0x7FFFFFFF";

// Creates an empty sysfs file rooted in |temp_root_dir|. For example if
// |temp_root_dir| is "/tmp/xxx" and |sys_path| is "/sys/power/temp", creates
// "/tmp/xxx/sys/power/temp" with all necessary root directories.
void CreateSysfsFileInTempRootDir(const base::FilePath& temp_root_dir,
                                  const std::string& sys_path) {
  base::FilePath path = temp_root_dir.Append(sys_path.substr(1));
  ASSERT_TRUE(base::CreateDirectory(path.DirName()));
  CHECK_EQ(base::WriteFile(path, "", 0), 0);
}

}  // namespace

class SuspendConfiguratorTest : public ::testing::Test {
 public:
  SuspendConfiguratorTest() {
    // Temporary directory mimicking a root directory.
    CHECK(temp_root_dir_.CreateUniqueTempDir());
    base::FilePath temp_root_dir_path = temp_root_dir_.GetPath();
    suspend_configurator_.set_prefix_path_for_testing(temp_root_dir_path);

    CreateSysfsFileInTempRootDir(
        temp_root_dir_path, SuspendConfigurator::kConsoleSuspendPath.value());
    CreateSysfsFileInTempRootDir(temp_root_dir_path, kSuspendModePath);
  }

  ~SuspendConfiguratorTest() override = default;

 protected:
  // Returns |orig| rooted within the temporary root dir created for testing.
  base::FilePath GetPath(const base::FilePath& orig) const {
    return temp_root_dir_.GetPath().Append(orig.value().substr(1));
  }

  std::string ReadFile(const base::FilePath& file) {
    std::string file_contents;
    EXPECT_TRUE(base::ReadFileToString(file, &file_contents));
    return file_contents;
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
  // Make sure console is enabled if system suspends to S3.
  EXPECT_EQ("N", ReadFile(console_suspend_path));
}

// Test console is disabled during supend to S0iX by default.
TEST_F(SuspendConfiguratorTest, TestDefaultConsoleSuspendForS0ix) {
  base::FilePath console_suspend_path =
      GetPath(SuspendConfigurator::kConsoleSuspendPath);
  prefs_.SetInt64(kSuspendToIdlePref, 1);
  suspend_configurator_.Init(&prefs_);
  // Make sure console is disabled if S0ix is enabled.
  EXPECT_EQ("Y", ReadFile(console_suspend_path));
}

// Test default value to suspend console is overwritten if
// |kEnableConsoleDuringSuspendPref| is set.
TEST_F(SuspendConfiguratorTest, TestDefaultConsoleSuspendOverwritten) {
  base::FilePath console_suspend_path =
      GetPath(SuspendConfigurator::kConsoleSuspendPath);
  prefs_.SetInt64(kSuspendToIdlePref, 1);
  prefs_.SetInt64(kEnableConsoleDuringSuspendPref, 1);
  suspend_configurator_.Init(&prefs_);
  // Make sure console is not disabled though the default is to disable it.
  EXPECT_EQ("N", ReadFile(console_suspend_path));
}

// Test that suspend mode is set to |kSuspendModeFreeze| if suspend_to_idle is
// enabled.
TEST_F(SuspendConfiguratorTest, TestSuspendModeIdle) {
  base::FilePath suspend_mode_path = GetPath(base::FilePath(kSuspendModePath));
  // Suspend mode should be configured to |kSuspendModeFreeze| even when
  // |kSuspendModePref| is configured to something else.
  prefs_.SetInt64(kSuspendToIdlePref, 1);
  prefs_.SetString(kSuspendModePref, kSuspendModeShallow);
  suspend_configurator_.Init(&prefs_);

  suspend_configurator_.PrepareForSuspend(base::TimeDelta());
  EXPECT_EQ(kSuspendModeFreeze, ReadFile(suspend_mode_path));
}

// Test that suspend mode is set to |kSuspendModeShallow| if |kSuspendModePref|
// is set to same when s0ix is not enabled.
TEST_F(SuspendConfiguratorTest, TestSuspendModeShallow) {
  base::FilePath suspend_mode_path = GetPath(base::FilePath(kSuspendModePath));
  prefs_.SetInt64(kSuspendToIdlePref, 0);
  prefs_.SetString(kSuspendModePref, kSuspendModeShallow);
  suspend_configurator_.Init(&prefs_);

  suspend_configurator_.PrepareForSuspend(base::TimeDelta());
  EXPECT_EQ(kSuspendModeShallow, ReadFile(suspend_mode_path));
}

// Test that suspend mode is set to |kSuspendModeDeep| if |kSuspendModePref|
// is invalid .
TEST_F(SuspendConfiguratorTest, TestSuspendModeDeep) {
  base::FilePath suspend_mode_path = GetPath(base::FilePath(kSuspendModePath));
  prefs_.SetInt64(kSuspendToIdlePref, 0);
  prefs_.SetString(kSuspendModePref, "Junk");
  suspend_configurator_.Init(&prefs_);

  suspend_configurator_.PrepareForSuspend(base::TimeDelta());
  EXPECT_EQ(kSuspendModeDeep, ReadFile(suspend_mode_path));
}

// Test that UndoPrepareForSuspend() returns success when
// |kECLastResumeResultPath| does not exist .
TEST_F(SuspendConfiguratorTest, TestNokECLastResumeResultPath) {
  EXPECT_TRUE(suspend_configurator_.UndoPrepareForSuspend());
}

// Test that UndoPrepareForSuspend() returns success/failure based on value in
// |kECLastResumeResultPath|.
TEST_F(SuspendConfiguratorTest, TestkECLastResumeResultPathExist) {
  CreateSysfsFileInTempRootDir(temp_root_dir_.GetPath(),
                               kECLastResumeResultPath);
  // Empty |kECLastResumeResultPath| file should not fail suspend.
  EXPECT_TRUE(suspend_configurator_.UndoPrepareForSuspend());

  // Write a value that indicates hang to |kECLastResumeResultPath| and test
  // UndoPrepareForSuspend() returns false.
  std::string last_resume_result_string = kECResumeResultHang;
  ASSERT_EQ(base::WriteFile(GetPath(base::FilePath(kECLastResumeResultPath)),
                            last_resume_result_string.c_str(),
                            last_resume_result_string.length()),
            last_resume_result_string.length());
  EXPECT_FALSE(suspend_configurator_.UndoPrepareForSuspend());

  // Write a value that does not indicate hang to |kECLastResumeResultPath| and
  // test UndoPrepareForSuspend() returns true.
  last_resume_result_string = kECResumeResultNoHang;
  ASSERT_EQ(base::WriteFile(GetPath(base::FilePath(kECLastResumeResultPath)),
                            last_resume_result_string.c_str(),
                            last_resume_result_string.length()),
            last_resume_result_string.length());
  EXPECT_TRUE(suspend_configurator_.UndoPrepareForSuspend());
}

}  // namespace system
}  // namespace power_manager
