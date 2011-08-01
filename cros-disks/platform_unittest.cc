// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cros-disks/platform.h"

#include <sys/types.h>
#include <unistd.h>

#include <base/basictypes.h>
#include <base/file_util.h>
#include <base/memory/scoped_temp_dir.h>
#include <gtest/gtest.h>

using std::string;

namespace cros_disks {

class PlatformTest : public ::testing::Test {
 public:
  // Returns true if |path| is owned by |user_id| and |group_id|.
  static bool CheckOwnership(const string& path,
                             uid_t user_id, gid_t group_id) {
    struct stat buffer;
    if (stat(path.c_str(), &buffer) != 0)
      return false;
    return buffer.st_uid == user_id && buffer.st_gid == group_id;
  }

  // Returns true if |path| has its permissions set to |mode|.
  static bool CheckPermissions(const string& path, mode_t mode) {
    struct stat buffer;
    if (stat(path.c_str(), &buffer) != 0)
      return false;
    return (buffer.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO)) == mode;
  }

 protected:
  Platform platform_;
};

TEST_F(PlatformTest, CreateDirectory) {
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  // Nonexistent directory
  FilePath new_dir = temp_dir.path().Append("test");
  string path = new_dir.value();
  EXPECT_TRUE(platform_.CreateDirectory(path));

  // Existent but empty directory
  EXPECT_TRUE(platform_.CreateDirectory(path));

  // Existent and non-empty directory
  FilePath temp_file;
  ASSERT_TRUE(file_util::CreateTemporaryFileInDir(new_dir, &temp_file));
  EXPECT_TRUE(platform_.CreateDirectory(path));
}

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

TEST_F(PlatformTest, GetGroupIdOfRoot) {
  gid_t group_id;
  EXPECT_TRUE(platform_.GetGroupId("root", &group_id));
  EXPECT_EQ(0, group_id);
}

TEST_F(PlatformTest, GetGroupIdOfNonExistentGroup) {
  gid_t group_id;
  EXPECT_FALSE(platform_.GetGroupId("nonexistent-group", &group_id));
}

TEST_F(PlatformTest, GetUserAndGroupIdOfRoot) {
  uid_t user_id;
  gid_t group_id;
  EXPECT_TRUE(platform_.GetUserAndGroupId("root", &user_id, &group_id));
  EXPECT_EQ(0, user_id);
  EXPECT_EQ(0, group_id);
}

TEST_F(PlatformTest, GetUserAndGroupIdOfNonExistentUser) {
  uid_t user_id;
  gid_t group_id;
  EXPECT_FALSE(platform_.GetUserAndGroupId("nonexistent-user",
                                           &user_id, &group_id));
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

TEST_F(PlatformTest, SetOwnershipOfNonExistentPath) {
  EXPECT_FALSE(platform_.SetOwnership("/nonexistent-path", getuid(), getgid()));
}

TEST_F(PlatformTest, SetOwnershipOfExistentPath) {
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  string path = temp_dir.path().value();

  EXPECT_TRUE(platform_.SetOwnership(path, getuid(), getgid()));
  EXPECT_TRUE(CheckOwnership(path, getuid(), getgid()));
}

TEST_F(PlatformTest, SetPermissionsOfNonExistentPath) {
  EXPECT_FALSE(platform_.SetPermissions("/nonexistent-path", S_IRWXU));
}

TEST_F(PlatformTest, SetPermissionsOfExistentPath) {
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  string path = temp_dir.path().value();

  mode_t mode = S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;
  EXPECT_TRUE(platform_.SetPermissions(path, mode));
  EXPECT_TRUE(CheckPermissions(path, mode));

  mode = S_IRWXU | S_IRGRP | S_IXGRP;
  EXPECT_TRUE(platform_.SetPermissions(path, mode));
  EXPECT_TRUE(CheckPermissions(path, mode));

  mode = S_IRWXU;
  EXPECT_TRUE(platform_.SetPermissions(path, mode));
  EXPECT_TRUE(CheckPermissions(path, mode));
}

}  // namespace cros_disks
