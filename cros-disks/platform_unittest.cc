// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cros-disks/platform.h"

#include <base/basictypes.h>
#include <base/file_util.h>
#include <base/memory/scoped_temp_dir.h>
#include <gtest/gtest.h>

using std::string;

namespace cros_disks {

class PlatformTest : public ::testing::Test {
 protected:
  Platform platform_;
};

TEST_F(PlatformTest, CreateOrReuseEmptyDirectory) {
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  // Nonexistent directory
  FilePath new_dir = temp_dir.path().Append("test");
  string path = new_dir.value();
  EXPECT_TRUE(platform_.CreateOrReuseEmptyDirectory(path));

  // Existent but empty directory
  EXPECT_TRUE(platform_.CreateOrReuseEmptyDirectory(path));

  // Existent and non-empty directory
  FilePath temp_file;
  ASSERT_TRUE(file_util::CreateTemporaryFileInDir(new_dir, &temp_file));
  EXPECT_FALSE(platform_.CreateOrReuseEmptyDirectory(path));
}

TEST_F(PlatformTest, CreateOrReuseEmptyDirectoryWithFallback) {
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  // Nonexistent directory
  FilePath new_dir = temp_dir.path().Append("test");
  string path = new_dir.value();
  EXPECT_TRUE(platform_.CreateOrReuseEmptyDirectoryWithFallback(&path, 10));
  EXPECT_EQ(new_dir.value(), path);

  // Existent but empty directory
  path = new_dir.value();
  EXPECT_TRUE(platform_.CreateOrReuseEmptyDirectoryWithFallback(&path, 10));
  EXPECT_EQ(new_dir.value(), path);

  // Existent and non-empty directory
  FilePath temp_file;
  ASSERT_TRUE(file_util::CreateTemporaryFileInDir(new_dir, &temp_file));
  path = new_dir.value();
  EXPECT_FALSE(platform_.CreateOrReuseEmptyDirectoryWithFallback(&path, 0));
  EXPECT_TRUE(platform_.CreateOrReuseEmptyDirectoryWithFallback(&path, 1));
  FilePath new_dir1 = temp_dir.path().Append("test (1)");
  EXPECT_EQ(new_dir1.value(), path);

  ASSERT_TRUE(file_util::CreateTemporaryFileInDir(new_dir1, &temp_file));
  path = new_dir.value();
  EXPECT_FALSE(platform_.CreateOrReuseEmptyDirectoryWithFallback(&path, 0));
  EXPECT_FALSE(platform_.CreateOrReuseEmptyDirectoryWithFallback(&path, 1));
  EXPECT_TRUE(platform_.CreateOrReuseEmptyDirectoryWithFallback(&path, 2));
  FilePath new_dir2 = temp_dir.path().Append("test (2)");
  EXPECT_EQ(new_dir2.value(), path);
}

TEST_F(PlatformTest, GetUserAndGroupIdForRoot) {
  uid_t uid;
  gid_t gid;
  EXPECT_TRUE(platform_.GetUserAndGroupId("root", &uid, &gid));
  EXPECT_EQ(0, uid);
  EXPECT_EQ(0, gid);
}

TEST_F(PlatformTest, GetUserAndGroupIdForNonExistentUser) {
  uid_t uid;
  gid_t gid;
  EXPECT_FALSE(platform_.GetUserAndGroupId("non-existent-user", &uid, &gid));
}

TEST_F(PlatformTest, RemoveEmptyDirectory) {
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  // Nonexistent directory
  FilePath new_dir = temp_dir.path().Append("test");
  string path = new_dir.value();
  EXPECT_FALSE(platform_.RemoveEmptyDirectory(path));

  // Existent but empty directory
  EXPECT_TRUE(platform_.CreateOrReuseEmptyDirectory(path));
  EXPECT_TRUE(platform_.RemoveEmptyDirectory(path));

  // Existent and non-empty directory
  EXPECT_TRUE(platform_.CreateOrReuseEmptyDirectory(path));
  FilePath temp_file;
  ASSERT_TRUE(file_util::CreateTemporaryFileInDir(new_dir, &temp_file));
  EXPECT_FALSE(platform_.RemoveEmptyDirectory(path));
}

}  // namespace cros_disks
