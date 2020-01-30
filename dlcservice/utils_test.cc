// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "dlcservice/utils.h"

#include <string>

#include <base/bind.h>
#include <base/files/file_util.h>
#include <base/files/scoped_temp_dir.h>
#include <gtest/gtest.h>

namespace dlcservice {

namespace {
constexpr char kDlcRootPath[] = "/tmp/dlc/";
constexpr char kDlcId[] = "id";
constexpr char kDlcPackage[] = "package";
}  // namespace

class FixtureUtilsTest : public testing::Test {
 protected:
  void SetUp() override { CHECK(scoped_temp_dir_.CreateUniqueTempDir()); }

  void CheckPerms(const base::FilePath& path, const int& expected_perms) {
    int actual_perms = -1;
    EXPECT_TRUE(base::GetPosixFilePermissions(path, &actual_perms));
    EXPECT_EQ(actual_perms, expected_perms);
  }

  base::ScopedTempDir scoped_temp_dir_;
};

TEST_F(FixtureUtilsTest, WriteToFile) {
  auto path = JoinPaths(scoped_temp_dir_.GetPath(), "file");
  std::string expected_data1 = "hello", expected_data2 = "world", actual_data;
  EXPECT_FALSE(base::PathExists(path));

  // Write "hello".
  EXPECT_TRUE(WriteToFile(path, expected_data1));
  EXPECT_TRUE(base::ReadFileToString(path, &actual_data));
  EXPECT_EQ(actual_data, expected_data1);

  // Write "world".
  EXPECT_TRUE(WriteToFile(path, expected_data2));
  EXPECT_TRUE(base::ReadFileToString(path, &actual_data));
  EXPECT_EQ(actual_data, expected_data2);

  // Write "worldworld".
  EXPECT_TRUE(WriteToFile(path, expected_data2 + expected_data2));
  EXPECT_TRUE(base::ReadFileToString(path, &actual_data));
  EXPECT_EQ(actual_data, expected_data2 + expected_data2);

  // Write "hello", but file had "worldworld" -> "helloworld".
  EXPECT_TRUE(WriteToFile(path, expected_data1));
  EXPECT_TRUE(base::ReadFileToString(path, &actual_data));
  EXPECT_EQ(actual_data, expected_data1 + expected_data2);
}

TEST_F(FixtureUtilsTest, WriteToFilePermissionsCheck) {
  auto path = JoinPaths(scoped_temp_dir_.GetPath(), "file");
  EXPECT_FALSE(base::PathExists(path));
  EXPECT_TRUE(WriteToFile(path, ""));
  CheckPerms(path, kDlcFilePerms);
}

TEST_F(FixtureUtilsTest, CreateDir) {
  auto path = JoinPaths(scoped_temp_dir_.GetPath(), "dir");
  EXPECT_FALSE(base::DirectoryExists(path));
  EXPECT_TRUE(CreateDir(path));
  EXPECT_TRUE(base::DirectoryExists(path));
  CheckPerms(path, kDlcDirectoryPerms);
}

TEST_F(FixtureUtilsTest, CreateFile) {
  auto path = JoinPaths(scoped_temp_dir_.GetPath(), "file");
  EXPECT_FALSE(base::PathExists(path));
  EXPECT_TRUE(CreateFile(path, 0));
  EXPECT_TRUE(base::PathExists(path));
  CheckPerms(path, kDlcFilePerms);
}

TEST_F(FixtureUtilsTest, ResizeFile) {
  int64_t size = -1;
  auto path = JoinPaths(scoped_temp_dir_.GetPath(), "file");
  EXPECT_TRUE(CreateFile(path, 0));
  EXPECT_TRUE(base::GetFileSize(path, &size));
  EXPECT_EQ(0, size);

  EXPECT_TRUE(ResizeFile(path, 1));

  EXPECT_TRUE(base::GetFileSize(path, &size));
  EXPECT_EQ(1, size);
}

TEST_F(FixtureUtilsTest, CopyAndResizeFile) {
  int64_t src_size = -1, dst_size = -1;
  auto src_path = JoinPaths(scoped_temp_dir_.GetPath(), "src_file");
  auto dst_path = JoinPaths(scoped_temp_dir_.GetPath(), "dst_file");

  EXPECT_FALSE(base::PathExists(src_path));
  EXPECT_FALSE(base::PathExists(dst_path));
  EXPECT_TRUE(CreateFile(src_path, 0));
  EXPECT_TRUE(base::GetFileSize(src_path, &src_size));
  EXPECT_EQ(0, src_size);

  EXPECT_TRUE(CopyAndResizeFile(src_path, dst_path, 1));

  EXPECT_TRUE(base::PathExists(dst_path));
  EXPECT_TRUE(base::GetFileSize(dst_path, &dst_size));
  EXPECT_EQ(1, dst_size);

  CheckPerms(dst_path, kDlcFilePerms);
}

TEST(UtilsTest, JoinPathsTest) {
  EXPECT_EQ(JoinPaths(base::FilePath(kDlcRootPath), kDlcId).value(),
            "/tmp/dlc/id");
  EXPECT_EQ(
      JoinPaths(base::FilePath(kDlcRootPath), kDlcId, kDlcPackage).value(),
      "/tmp/dlc/id/package");
}

TEST(UtilsTest, GetDlcModuleImagePathA) {
  EXPECT_EQ(GetDlcImagePath(base::FilePath(kDlcRootPath), kDlcId, kDlcPackage,
                            BootSlot::Slot::A)
                .value(),
            "/tmp/dlc/id/package/dlc_a/dlc.img");
}

TEST(UtilsTest, GetDlcModuleImagePathB) {
  EXPECT_EQ(GetDlcImagePath(base::FilePath(kDlcRootPath), kDlcId, kDlcPackage,
                            BootSlot::Slot::B)
                .value(),
            "/tmp/dlc/id/package/dlc_b/dlc.img");
}

TEST(UtilsTest, GetDlcRootInModulePath) {
  base::FilePath path("foo-path");
  base::FilePath expected_path("foo-path/root");
  EXPECT_EQ(GetDlcRootInModulePath(path), expected_path);
}

TEST(UtilsTest, ScopedCleanupsTest) {
  bool flag = false;
  base::Callback<void()> cleanup =
      base::Bind([](bool* flag_ptr) { *flag_ptr = true; }, &flag);

  {
    ScopedCleanups<decltype(cleanup)> scoped_cleanups;
    scoped_cleanups.Insert(cleanup);
  }
  EXPECT_TRUE(flag);

  flag = false;
  {
    ScopedCleanups<decltype(cleanup)> scoped_cleanups;
    scoped_cleanups.Insert(cleanup);
    scoped_cleanups.Cancel();
  }
  EXPECT_FALSE(flag);
}

}  // namespace dlcservice
