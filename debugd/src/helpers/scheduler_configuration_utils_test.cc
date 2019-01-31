// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "debugd/src/helpers/scheduler_configuration_utils.h"

#include <fcntl.h>
#include <stdio.h>

#include <string>

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/files/scoped_temp_dir.h>
#include <gtest/gtest.h>

namespace debugd {

class SchedulerConfigurationHelperTest : public testing::Test {
 public:
  void CreateSysInterface(const base::FilePath& cpu_root_dir) {
    // Set up a fake tempdir mimicking a performance mode CPU.
    ASSERT_TRUE(base::CreateDirectory(cpu_root_dir));

    // Create CPUs 0-3, and turn them all on.
    for (const std::string& cpu_num : {"0", "1", "2", "3"}) {
      base::FilePath cpu_subroot = cpu_root_dir.Append("cpu" + cpu_num);
      ASSERT_TRUE(base::CreateDirectory(cpu_subroot));
      std::string flag = "1";
      ASSERT_TRUE(base::WriteFile(cpu_subroot.Append("online"), flag.c_str(),
                                  flag.size()));

      // Establish odd CPUs as virtual siblings.
      base::FilePath topology = cpu_subroot.Append("topology");
      ASSERT_TRUE(base::CreateDirectory(topology));
      std::string topology_str;
      if (cpu_num == "0" || cpu_num == "1") {
        topology_str = "0-1";
      } else if (cpu_num == "2" || cpu_num == "3") {
        topology_str = "2-3";
      }
      ASSERT_TRUE(base::WriteFile(topology.Append("thread_siblings_list"),
                                  topology_str.c_str(), topology_str.size()));
    }

    // Establish the control files.
    base::FilePath online_cpus_file = cpu_root_dir.Append("online");
    const std::string online_cpus = "0-3";
    ASSERT_TRUE(base::WriteFile(online_cpus_file, online_cpus.c_str(),
                                online_cpus.size()));

    // Establish the offline CPUs.
    base::FilePath offline_cpus_file = cpu_root_dir.Append("offline");
    const char terminator = 0xa;
    ASSERT_TRUE(base::WriteFile(offline_cpus_file, &terminator, 1));
  }

  void CheckPerformanceMode(const base::FilePath& cpu_root_dir) {
    for (const std::string& cpu_num : {"0", "1", "2", "3"}) {
      base::FilePath cpu_control =
          cpu_root_dir.Append("cpu" + cpu_num).Append("online");
      std::string control_contents;
      ASSERT_TRUE(base::ReadFileToString(cpu_control, &control_contents));
      EXPECT_EQ("1", control_contents);
    }
  }

  void CheckConservativeMode(const base::FilePath& cpu_root_dir) {
    for (const std::string& cpu_num : {"0", "1", "2", "3"}) {
      base::FilePath cpu_control =
          cpu_root_dir.Append("cpu" + cpu_num).Append("online");
      std::string control_contents;
      ASSERT_TRUE(base::ReadFileToString(cpu_control, &control_contents));
      if (cpu_num == "0" || cpu_num == "2") {
        EXPECT_EQ("1", control_contents);
      } else if (cpu_num == "1" || cpu_num == "3") {
        EXPECT_EQ("0", control_contents);
      }
    }
  }
};

TEST_F(SchedulerConfigurationHelperTest, ParseCPUs) {
  // Note the usual security principle in this test: the kernel shouldn't return
  // any of these crazy invalid sequences ("0-?", etc.), but it's important to
  // handle unexpected input gracefully.
  debugd::SchedulerConfigurationUtils utils{base::FilePath("/sys")};

  std::vector<std::string> raw_num;
  EXPECT_TRUE(utils.ParseCPUNumbers("1", &raw_num));
  EXPECT_EQ(std::vector<std::string>({"1"}), raw_num);

  // Test a simple range.
  std::vector<std::string> range;
  EXPECT_TRUE(utils.ParseCPUNumbers("0-3", &range));
  EXPECT_EQ(std::vector<std::string>({"0", "1", "2", "3"}), range);

  // Test a comma separated list.
  std::vector<std::string> list;
  EXPECT_TRUE(utils.ParseCPUNumbers("0,3,4,7", &list));
  EXPECT_EQ(std::vector<std::string>({"0", "3", "4", "7"}), list);

  // Test a one way range.
  std::vector<std::string> one_way_range;
  EXPECT_TRUE(utils.ParseCPUNumbers("0-", &one_way_range));
  EXPECT_EQ(std::vector<std::string>({"0"}), one_way_range);

  std::vector<std::string> one_way_range2;
  EXPECT_TRUE(utils.ParseCPUNumbers("-9", &one_way_range2));
  EXPECT_EQ(std::vector<std::string>({"9"}), one_way_range2);

  // Invalid ranges.
  std::vector<std::string> invalid;
  EXPECT_FALSE(utils.ParseCPUNumbers("-", &invalid));
  EXPECT_FALSE(utils.ParseCPUNumbers(",", &invalid));
  EXPECT_FALSE(utils.ParseCPUNumbers("?", &invalid));
  EXPECT_FALSE(utils.ParseCPUNumbers("0-?", &invalid));
  EXPECT_FALSE(utils.ParseCPUNumbers("1,?", &invalid));
  EXPECT_FALSE(utils.ParseCPUNumbers("1,", &invalid));
  EXPECT_FALSE(utils.ParseCPUNumbers(",1", &invalid));
  EXPECT_FALSE(utils.ParseCPUNumbers("0,1-3", &invalid));
  EXPECT_FALSE(utils.ParseCPUNumbers("0-3,1-3", &invalid));
  EXPECT_FALSE(utils.ParseCPUNumbers("0,2,1-3", &invalid));
  EXPECT_FALSE(utils.ParseCPUNumbers("0-2,1", &invalid));
}

TEST_F(SchedulerConfigurationHelperTest, WriteFlag) {
  debugd::SchedulerConfigurationUtils utils{base::FilePath("/sys")};
  base::FilePath target_file;

  ASSERT_TRUE(base::CreateTemporaryFile(&target_file));

  base::ScopedFD fd(open(target_file.value().c_str(), O_RDWR | O_CLOEXEC));
  ASSERT_LE(0, fd.get());

  ASSERT_TRUE(utils.WriteFlagToCPUControlFile(fd, "test"));

  std::string file_contents;
  ASSERT_TRUE(base::ReadFileToString(target_file, &file_contents));

  EXPECT_EQ("test", file_contents);
}

TEST_F(SchedulerConfigurationHelperTest, TestSchedulers) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  const base::FilePath cpu_root_dir =
      temp_dir.GetPath().Append("devices").Append("system").Append("cpu");
  CreateSysInterface(cpu_root_dir);

  debugd::SchedulerConfigurationUtils utils(temp_dir.GetPath());
  ASSERT_TRUE(utils.GetControlFDs());
  ASSERT_TRUE(utils.EnablePerformanceConfiguration());

  CheckPerformanceMode(cpu_root_dir);

  // Now enable conservative mode.
  SchedulerConfigurationUtils utils2(temp_dir.GetPath());
  ASSERT_TRUE(utils2.GetControlFDs());
  ASSERT_TRUE(utils2.EnableConservativeConfiguration());

  CheckConservativeMode(cpu_root_dir);

  // Before going back to performance mode, update the control files to mimick
  // the kernel's actions.
  base::FilePath online_cpus_file = cpu_root_dir.Append("online");
  base::FilePath offline_cpus_file = cpu_root_dir.Append("offline");
  std::string online_now = "0,2";
  std::string offline_now = "1,3";
  ASSERT_TRUE(
      base::WriteFile(online_cpus_file, online_now.c_str(), online_now.size()));
  ASSERT_TRUE(base::WriteFile(offline_cpus_file, offline_now.c_str(),
                              offline_now.size()));

  // Re-enable performance and test.
  SchedulerConfigurationUtils utils3(temp_dir.GetPath());
  ASSERT_TRUE(utils3.GetControlFDs());
  ASSERT_TRUE(utils3.EnablePerformanceConfiguration());

  CheckPerformanceMode(cpu_root_dir);
}

}  // namespace debugd
