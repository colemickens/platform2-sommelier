/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <errno.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/files/scoped_temp_dir.h>
#include <base/logging.h>
#include <base/strings/string_split.h>

#include "libcontainer/test_harness.h"

#include "libcontainer/cgroup.h"

namespace {

constexpr char kCgroupName[] = "testcg";
constexpr char kCgroupParentName[] = "testparentcg";

bool CreateFile(const base::FilePath& path) {
  return base::WriteFile(path, "", 0) == 0;
}

bool FileHasString(const base::FilePath& path, const std::string& expected) {
  std::string contents;
  if (!base::ReadFileToString(path, &contents))
    return false;

  return contents.find(expected) != std::string::npos;
}

bool FileHasLine(const base::FilePath& path, const std::string& expected) {
  std::string contents;
  if (!base::ReadFileToString(path, &contents))
    return false;

  for (const auto& line : base::SplitString(
           contents, "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
    if (line == expected)
      return true;
  }
  return false;
}

}  // namespace

TEST(cgroup_new_with_parent) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath cgroup_root = temp_dir.path();

  for (const char* subsystem :
       {"cpu", "cpuacct", "cpuset", "devices", "freezer", "schedtune"}) {
    base::FilePath path = cgroup_root.Append(subsystem);
    EXPECT_EQ(0, mkdir(path.value().c_str(), S_IRWXU | S_IRWXG));
    path = path.Append(kCgroupParentName);
    EXPECT_EQ(0, mkdir(path.value().c_str(), S_IRWXU | S_IRWXG));
  }

  ASSERT_TRUE(base::WriteFile(
      cgroup_root.Append("cpuset").Append(kCgroupParentName).Append("cpus"),
      "0-3",
      3));
  ASSERT_TRUE(base::WriteFile(
      cgroup_root.Append("cpuset").Append(kCgroupParentName).Append("mems"),
      "0",
      1));

  std::unique_ptr<libcontainer::Cgroup> ccg =
      libcontainer::Cgroup::Create(kCgroupName,
                                   cgroup_root,
                                   base::FilePath(kCgroupParentName),
                                   getuid(),
                                   getgid());
  ASSERT_NE(nullptr, ccg.get());

  EXPECT_TRUE(base::DirectoryExists(
      cgroup_root.Append("cpu").Append(kCgroupParentName).Append(kCgroupName)));
  EXPECT_TRUE(base::DirectoryExists(cgroup_root.Append("cpuacct")
                                        .Append(kCgroupParentName)
                                        .Append(kCgroupName)));
  EXPECT_TRUE(base::DirectoryExists(cgroup_root.Append("cpuset")
                                        .Append(kCgroupParentName)
                                        .Append(kCgroupName)));
  EXPECT_TRUE(base::DirectoryExists(cgroup_root.Append("devices")
                                        .Append(kCgroupParentName)
                                        .Append(kCgroupName)));
  EXPECT_TRUE(base::DirectoryExists(cgroup_root.Append("freezer")
                                        .Append(kCgroupParentName)
                                        .Append(kCgroupName)));
  EXPECT_TRUE(base::DirectoryExists(cgroup_root.Append("schedtune")
                                        .Append(kCgroupParentName)
                                        .Append(kCgroupName)));

  EXPECT_TRUE(temp_dir.Delete());
}

// TODO(lhchavez): Remove once we use gtest.
struct BasicCgroupManipulationTest {
  std::unique_ptr<libcontainer::Cgroup> ccg;

  base::FilePath cpu_cg;
  base::FilePath cpuacct_cg;
  base::FilePath cpuset_cg;
  base::FilePath devices_cg;
  base::FilePath freezer_cg;
  base::FilePath schedtune_cg;

  base::ScopedTempDir temp_dir;
};

FIXTURE(basic_manipulation) {
  BasicCgroupManipulationTest* state;
};

FIXTURE_SETUP(basic_manipulation) {
  self->state = new BasicCgroupManipulationTest();
  ASSERT_TRUE(self->state->temp_dir.CreateUniqueTempDir());

  base::FilePath cgroup_root;
  ASSERT_TRUE(base::CreateTemporaryDirInDir(
      self->state->temp_dir.path(), FILE_PATH_LITERAL("cgtest"), &cgroup_root));

  for (const char* subsystem :
       {"cpu", "cpuacct", "cpuset", "devices", "freezer", "schedtune"}) {
    base::FilePath path = cgroup_root.Append(subsystem);
    ASSERT_EQ(0, mkdir(path.value().c_str(), S_IRWXU | S_IRWXG));
  }

  ASSERT_TRUE(base::WriteFile(cgroup_root.Append("cpuset/cpus"), "0-3", 3));
  ASSERT_TRUE(base::WriteFile(cgroup_root.Append("cpuset/mems"), "0", 1));

  self->state->ccg = libcontainer::Cgroup::Create(
      kCgroupName, cgroup_root, base::FilePath(), 0, 0);
  ASSERT_NE(nullptr, self->state->ccg.get());

  self->state->cpu_cg = cgroup_root.Append("cpu").Append(kCgroupName);
  self->state->cpuacct_cg = cgroup_root.Append("cpuacct").Append(kCgroupName);
  self->state->cpuset_cg = cgroup_root.Append("cpuset").Append(kCgroupName);
  self->state->devices_cg = cgroup_root.Append("devices").Append(kCgroupName);
  self->state->freezer_cg = cgroup_root.Append("freezer").Append(kCgroupName);
  self->state->schedtune_cg =
      cgroup_root.Append("schedtune").Append(kCgroupName);

  ASSERT_TRUE(base::DirectoryExists(self->state->cpu_cg));
  ASSERT_TRUE(base::DirectoryExists(self->state->cpuacct_cg));
  ASSERT_TRUE(base::DirectoryExists(self->state->cpuset_cg));
  ASSERT_TRUE(base::DirectoryExists(self->state->devices_cg));
  ASSERT_TRUE(base::DirectoryExists(self->state->freezer_cg));
  ASSERT_TRUE(base::DirectoryExists(self->state->schedtune_cg));

  ASSERT_TRUE(CreateFile(self->state->cpu_cg.Append("tasks")));
  ASSERT_TRUE(CreateFile(self->state->cpu_cg.Append("cpu.shares")));
  ASSERT_TRUE(CreateFile(self->state->cpu_cg.Append("cpu.cfs_quota_us")));
  ASSERT_TRUE(CreateFile(self->state->cpu_cg.Append("cpu.cfs_period_us")));
  ASSERT_TRUE(CreateFile(self->state->cpu_cg.Append("cpu.rt_runtime_us")));
  ASSERT_TRUE(CreateFile(self->state->cpu_cg.Append("cpu.rt_period_us")));
  ASSERT_TRUE(CreateFile(self->state->cpuacct_cg.Append("tasks")));
  ASSERT_TRUE(CreateFile(self->state->cpuset_cg.Append("tasks")));
  ASSERT_TRUE(CreateFile(self->state->devices_cg.Append("tasks")));
  ASSERT_TRUE(CreateFile(self->state->devices_cg.Append("devices.allow")));
  ASSERT_TRUE(CreateFile(self->state->devices_cg.Append("devices.deny")));
  ASSERT_TRUE(CreateFile(self->state->freezer_cg.Append("tasks")));
  ASSERT_TRUE(CreateFile(self->state->freezer_cg.Append("freezer.state")));
  ASSERT_TRUE(CreateFile(self->state->schedtune_cg.Append("tasks")));
}

FIXTURE_TEARDOWN(basic_manipulation) {
  self->state->ccg.reset();
  ASSERT_TRUE(self->state->temp_dir.Delete());
  delete self->state;
}

TEST_F(basic_manipulation, freeze) {
  EXPECT_EQ(0, self->state->ccg->Freeze());
  EXPECT_TRUE(
      FileHasString(self->state->freezer_cg.Append("freezer.state"), "FROZEN"));
}

TEST_F(basic_manipulation, thaw) {
  EXPECT_EQ(0, self->state->ccg->Thaw());
  EXPECT_TRUE(
      FileHasString(self->state->freezer_cg.Append("freezer.state"), "THAWED"));
}

TEST_F(basic_manipulation, default_all_devs_disallow) {
  ASSERT_EQ(0, self->state->ccg->DenyAllDevices());
  EXPECT_TRUE(FileHasLine(self->state->devices_cg.Append("devices.deny"), "a"));
}

TEST_F(basic_manipulation, add_device_invalid_type) {
  EXPECT_NE(0, self->state->ccg->AddDevice(1, 14, 3, 1, 1, 0, 'x'));
}

TEST_F(basic_manipulation, add_device_no_perms) {
  EXPECT_NE(0, self->state->ccg->AddDevice(1, 14, 3, 0, 0, 0, 'c'));
}

TEST_F(basic_manipulation, add_device_rw) {
  EXPECT_EQ(0, self->state->ccg->AddDevice(1, 14, 3, 1, 1, 0, 'c'));
  EXPECT_TRUE(FileHasLine(self->state->devices_cg.Append("devices.allow"),
                          "c 14:3 rw"));
}

TEST_F(basic_manipulation, add_device_rwm) {
  EXPECT_EQ(0, self->state->ccg->AddDevice(1, 14, 3, 1, 1, 1, 'c'));
  EXPECT_TRUE(FileHasLine(self->state->devices_cg.Append("devices.allow"),
                          "c 14:3 rwm"));
}

TEST_F(basic_manipulation, add_device_ro) {
  EXPECT_EQ(0, self->state->ccg->AddDevice(1, 14, 3, 1, 0, 0, 'c'));
  EXPECT_TRUE(
      FileHasLine(self->state->devices_cg.Append("devices.allow"), "c 14:3 r"));
}

TEST_F(basic_manipulation, add_device_wo) {
  EXPECT_EQ(0, self->state->ccg->AddDevice(1, 14, 3, 0, 1, 0, 'c'));
  EXPECT_TRUE(
      FileHasLine(self->state->devices_cg.Append("devices.allow"), "c 14:3 w"));
}

TEST_F(basic_manipulation, add_device_major_wide) {
  EXPECT_EQ(0, self->state->ccg->AddDevice(1, 14, -1, 0, 1, 0, 'c'));
  EXPECT_TRUE(
      FileHasLine(self->state->devices_cg.Append("devices.allow"), "c 14:* w"));
}

TEST_F(basic_manipulation, add_device_major_minor_wildcard) {
  EXPECT_EQ(0, self->state->ccg->AddDevice(1, -1, -1, 0, 1, 0, 'c'));
  EXPECT_TRUE(
      FileHasLine(self->state->devices_cg.Append("devices.allow"), "c *:* w"));
}

TEST_F(basic_manipulation, add_device_deny_all) {
  EXPECT_EQ(0, self->state->ccg->AddDevice(0, -1, -1, 1, 1, 1, 'a'));
  EXPECT_TRUE(
      FileHasLine(self->state->devices_cg.Append("devices.deny"), "a *:* rwm"));
}

TEST_F(basic_manipulation, add_device_block) {
  EXPECT_EQ(0, self->state->ccg->AddDevice(1, 14, 3, 1, 1, 0, 'b'));
  EXPECT_TRUE(FileHasLine(self->state->devices_cg.Append("devices.allow"),
                          "b 14:3 rw"));
}

TEST_F(basic_manipulation, set_cpu_shares) {
  EXPECT_EQ(0, self->state->ccg->SetCpuShares(500));
  EXPECT_TRUE(FileHasString(self->state->cpu_cg.Append("cpu.shares"), "500"));
}

TEST_F(basic_manipulation, set_cpu_quota) {
  EXPECT_EQ(0, self->state->ccg->SetCpuQuota(200000));
  EXPECT_TRUE(
      FileHasString(self->state->cpu_cg.Append("cpu.cfs_quota_us"), "200000"));
}

TEST_F(basic_manipulation, set_cpu_period) {
  EXPECT_EQ(0, self->state->ccg->SetCpuPeriod(800000));
  EXPECT_TRUE(
      FileHasString(self->state->cpu_cg.Append("cpu.cfs_period_us"), "800000"));
}

TEST_F(basic_manipulation, set_cpu_rt_runtime) {
  EXPECT_EQ(0, self->state->ccg->SetCpuRtRuntime(100000));
  EXPECT_TRUE(
      FileHasString(self->state->cpu_cg.Append("cpu.rt_runtime_us"), "100000"));
}

TEST_F(basic_manipulation, set_cpu_rt_period) {
  EXPECT_EQ(0, self->state->ccg->SetCpuRtPeriod(500000));
  EXPECT_TRUE(
      FileHasString(self->state->cpu_cg.Append("cpu.rt_period_us"), "500000"));
}

TEST_HARNESS_MAIN
