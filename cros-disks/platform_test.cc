// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cros-disks/platform.h"

#include <sys/types.h>
#include <unistd.h>

#include <base/files/file_util.h>
#include <base/files/scoped_temp_dir.h>
#include <gtest/gtest.h>

namespace cros_disks {

class PlatformTest : public ::testing::Test {
 public:
  // Returns true if |path| is owned by |user_id| and |group_id|.
  static bool CheckOwnership(const std::string& path,
                             uid_t user_id,
                             gid_t group_id) {
    struct stat buffer;
    if (stat(path.c_str(), &buffer) != 0)
      return false;
    return buffer.st_uid == user_id && buffer.st_gid == group_id;
  }

  // Returns true if |path| has its permissions set to |mode|.
  static bool CheckPermissions(const std::string& path, mode_t mode) {
    struct stat buffer;
    if (stat(path.c_str(), &buffer) != 0)
      return false;
    return (buffer.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO)) == mode;
  }

 protected:
  Platform platform_;
};

TEST_F(PlatformTest, GetRealPath) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath subdir_path;
  ASSERT_TRUE(
      base::CreateTemporaryDirInDir(temp_dir.GetPath(), "test", &subdir_path));
  base::FilePath file_path;
  ASSERT_TRUE(base::CreateTemporaryFileInDir(subdir_path, &file_path));
  base::FilePath file_symlink = temp_dir.GetPath().Append("file_symlink");
  ASSERT_TRUE(base::CreateSymbolicLink(file_path, file_symlink));
  base::FilePath subdir_symlink = temp_dir.GetPath().Append("subdir_symlink");
  ASSERT_TRUE(base::CreateSymbolicLink(subdir_path, subdir_symlink));

  std::string real_path;
  EXPECT_FALSE(platform_.GetRealPath("", &real_path));
  EXPECT_FALSE(platform_.GetRealPath("/nonexistent", &real_path));

  EXPECT_TRUE(platform_.GetRealPath(temp_dir.GetPath().value(), &real_path));
  EXPECT_EQ(temp_dir.GetPath().value(), real_path);

  EXPECT_TRUE(platform_.GetRealPath(file_path.value(), &real_path));
  EXPECT_EQ(file_path.value(), real_path);

  EXPECT_TRUE(platform_.GetRealPath(file_symlink.value(), &real_path));
  EXPECT_EQ(file_path.value(), real_path);

  EXPECT_TRUE(platform_.GetRealPath(subdir_symlink.value(), &real_path));
  EXPECT_EQ(subdir_path.value(), real_path);

  base::FilePath relative_path = subdir_path.Append("..");
  EXPECT_TRUE(platform_.GetRealPath(relative_path.value(), &real_path));
  EXPECT_EQ(temp_dir.GetPath().value(), real_path);

  relative_path = subdir_path.Append("..")
                      .Append(subdir_path.BaseName())
                      .Append(file_path.BaseName());
  EXPECT_TRUE(platform_.GetRealPath(relative_path.value(), &real_path));
  EXPECT_EQ(file_path.value(), real_path);

  relative_path = subdir_path.Append("..")
                      .Append(subdir_symlink.BaseName())
                      .Append(file_path.BaseName());
  EXPECT_TRUE(platform_.GetRealPath(relative_path.value(), &real_path));
  EXPECT_EQ(file_path.value(), real_path);
}

TEST_F(PlatformTest, CreateDirectory) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  // Nonexistent directory
  base::FilePath new_dir = temp_dir.GetPath().Append("test");
  std::string path = new_dir.value();
  EXPECT_TRUE(platform_.CreateDirectory(path));

  // Existent but empty directory
  EXPECT_TRUE(platform_.CreateDirectory(path));

  // Existent and non-empty directory
  base::FilePath temp_file;
  ASSERT_TRUE(base::CreateTemporaryFileInDir(new_dir, &temp_file));
  EXPECT_TRUE(platform_.CreateDirectory(path));
}

TEST_F(PlatformTest, CreateOrReuseEmptyDirectory) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  // Nonexistent directory
  base::FilePath new_dir = temp_dir.GetPath().Append("test");
  std::string path = new_dir.value();
  EXPECT_TRUE(platform_.CreateOrReuseEmptyDirectory(path));

  // Existent but empty directory
  EXPECT_TRUE(platform_.CreateOrReuseEmptyDirectory(path));

  // Existent and non-empty directory
  base::FilePath temp_file;
  ASSERT_TRUE(base::CreateTemporaryFileInDir(new_dir, &temp_file));
  EXPECT_FALSE(platform_.CreateOrReuseEmptyDirectory(path));
}

TEST_F(PlatformTest, CreateOrReuseEmptyDirectoryWithFallback) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  std::set<std::string> reserved_paths;

  // Nonexistent directory
  base::FilePath new_dir = temp_dir.GetPath().Append("test1");
  std::string path = new_dir.value();
  EXPECT_TRUE(platform_.CreateOrReuseEmptyDirectoryWithFallback(
      &path, 10, reserved_paths));
  EXPECT_EQ(new_dir.value(), path);

  // Existent but empty directory
  path = new_dir.value();
  EXPECT_TRUE(platform_.CreateOrReuseEmptyDirectoryWithFallback(
      &path, 10, reserved_paths));
  EXPECT_EQ(new_dir.value(), path);

  // Existent and non-empty directory
  base::FilePath temp_file;
  ASSERT_TRUE(base::CreateTemporaryFileInDir(new_dir, &temp_file));
  path = new_dir.value();
  EXPECT_FALSE(platform_.CreateOrReuseEmptyDirectoryWithFallback(
      &path, 0, reserved_paths));
  EXPECT_TRUE(platform_.CreateOrReuseEmptyDirectoryWithFallback(
      &path, 1, reserved_paths));
  base::FilePath new_dir1 = temp_dir.GetPath().Append("test1 (1)");
  EXPECT_EQ(new_dir1.value(), path);

  ASSERT_TRUE(base::CreateTemporaryFileInDir(new_dir1, &temp_file));
  path = new_dir.value();
  EXPECT_FALSE(platform_.CreateOrReuseEmptyDirectoryWithFallback(
      &path, 0, reserved_paths));
  EXPECT_FALSE(platform_.CreateOrReuseEmptyDirectoryWithFallback(
      &path, 1, reserved_paths));
  EXPECT_TRUE(platform_.CreateOrReuseEmptyDirectoryWithFallback(
      &path, 2, reserved_paths));
  base::FilePath new_dir2 = temp_dir.GetPath().Append("test1 (2)");
  EXPECT_EQ(new_dir2.value(), path);
}

TEST_F(PlatformTest, CreateOrReuseEmptyDirectoryWithFallbackAndReservedPaths) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  std::set<std::string> reserved_paths;

  base::FilePath new_dir = temp_dir.GetPath().Append("test");
  std::string path = new_dir.value();
  reserved_paths.insert(path);
  EXPECT_FALSE(platform_.CreateOrReuseEmptyDirectoryWithFallback(
      &path, 0, reserved_paths));
  EXPECT_EQ(new_dir.value(), path);

  reserved_paths.insert(temp_dir.GetPath().Append("test 1").value());
  reserved_paths.insert(temp_dir.GetPath().Append("test 2").value());
  EXPECT_FALSE(platform_.CreateOrReuseEmptyDirectoryWithFallback(
      &path, 2, reserved_paths));
  EXPECT_EQ(new_dir.value(), path);

  base::FilePath expected_dir = temp_dir.GetPath().Append("test 3");
  EXPECT_TRUE(platform_.CreateOrReuseEmptyDirectoryWithFallback(
      &path, 3, reserved_paths));
  EXPECT_EQ(expected_dir.value(), path);
}

TEST_F(PlatformTest, CreateTemporaryDirInDir) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  std::string dir = temp_dir.GetPath().value();

  std::string path;
  EXPECT_TRUE(platform_.CreateTemporaryDirInDir(dir, "foo", &path));
  EXPECT_TRUE(base::DirectoryExists(base::FilePath(path)));
  EXPECT_EQ(dir, base::FilePath(path).DirName().value());
  base::FilePath foo1 = base::FilePath(path).BaseName();
  EXPECT_EQ("foo", foo1.value().substr(0, 3));

  EXPECT_TRUE(platform_.CreateTemporaryDirInDir(dir, "foo", &path));
  EXPECT_TRUE(base::DirectoryExists(base::FilePath(path)));
  EXPECT_EQ(dir, base::FilePath(path).DirName().value());
  base::FilePath foo2 = base::FilePath(path).BaseName();
  EXPECT_EQ("foo", foo2.value().substr(0, 3));

  EXPECT_NE(foo1.value(), foo2.value());
}

TEST_F(PlatformTest, GetDirectoryFallbackName) {
  EXPECT_EQ("test 1", platform_.GetDirectoryFallbackName("test", 1));
  EXPECT_EQ("test1 (1)", platform_.GetDirectoryFallbackName("test1", 1));
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
  EXPECT_FALSE(
      platform_.GetUserAndGroupId("nonexistent-user", &user_id, &group_id));
}

TEST_F(PlatformTest, GetOwnershipOfDirectory) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  std::string path = temp_dir.GetPath().value();

  uid_t user_id;
  gid_t group_id;
  EXPECT_TRUE(platform_.GetOwnership(path, &user_id, &group_id));
  EXPECT_EQ(getuid(), user_id);
  EXPECT_EQ(getgid(), group_id);
}

TEST_F(PlatformTest, GetOwnershipOfFile) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath temp_file;
  ASSERT_TRUE(base::CreateTemporaryFileInDir(temp_dir.GetPath(), &temp_file));
  std::string path = temp_file.value();

  uid_t user_id;
  gid_t group_id;
  EXPECT_TRUE(platform_.GetOwnership(path, &user_id, &group_id));
  EXPECT_EQ(getuid(), user_id);
  EXPECT_EQ(getgid(), group_id);
}

TEST_F(PlatformTest, GetOwnershipOfSymbolicLink) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath temp_file;
  ASSERT_TRUE(base::CreateTemporaryFileInDir(temp_dir.GetPath(), &temp_file));
  std::string file_path = temp_file.value();
  std::string symlink_path = file_path + "-symlink";
  base::FilePath temp_symlink(symlink_path);
  ASSERT_TRUE(base::CreateSymbolicLink(temp_file, temp_symlink));

  uid_t user_id;
  gid_t group_id;
  EXPECT_TRUE(platform_.GetOwnership(symlink_path, &user_id, &group_id));
  EXPECT_EQ(getuid(), user_id);
  EXPECT_EQ(getgid(), group_id);
}

TEST_F(PlatformTest, GetOwnershipOfNonexistentPath) {
  uid_t user_id;
  gid_t group_id;
  EXPECT_FALSE(
      platform_.GetOwnership("/nonexistent-path", &user_id, &group_id));
}

TEST_F(PlatformTest, GetPermissionsOfDirectory) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  std::string path = temp_dir.GetPath().value();

  mode_t mode = 0;
  EXPECT_TRUE(platform_.GetPermissions(path, &mode));
  mode_t expected_mode = (mode & ~S_IRWXG & ~S_IRWXO) | S_IRWXU;
  EXPECT_TRUE(platform_.SetPermissions(path, expected_mode));
  EXPECT_TRUE(platform_.GetPermissions(path, &mode));
  EXPECT_EQ(expected_mode, mode);

  mode = 0;
  expected_mode |= S_IRWXG;
  EXPECT_TRUE(platform_.SetPermissions(path, expected_mode));
  EXPECT_TRUE(platform_.GetPermissions(path, &mode));
  EXPECT_EQ(expected_mode, mode);
}

TEST_F(PlatformTest, GetPermissionsOfFile) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath temp_file;
  ASSERT_TRUE(base::CreateTemporaryFileInDir(temp_dir.GetPath(), &temp_file));
  std::string path = temp_file.value();

  mode_t mode = 0;
  EXPECT_TRUE(platform_.GetPermissions(path, &mode));
  mode_t expected_mode = (mode & ~S_IRWXG & ~S_IRWXO) | S_IRWXU;
  EXPECT_TRUE(platform_.SetPermissions(path, expected_mode));
  EXPECT_TRUE(platform_.GetPermissions(path, &mode));
  EXPECT_EQ(expected_mode, mode);

  mode = 0;
  expected_mode |= S_IRWXG;
  EXPECT_TRUE(platform_.SetPermissions(path, expected_mode));
  EXPECT_TRUE(platform_.GetPermissions(path, &mode));
  EXPECT_EQ(expected_mode, mode);
}

TEST_F(PlatformTest, GetPermissionsOfSymbolicLink) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath temp_file;
  ASSERT_TRUE(base::CreateTemporaryFileInDir(temp_dir.GetPath(), &temp_file));
  std::string file_path = temp_file.value();
  std::string symlink_path = file_path + "-symlink";
  base::FilePath temp_symlink(symlink_path);
  ASSERT_TRUE(base::CreateSymbolicLink(temp_file, temp_symlink));

  mode_t mode = 0;
  EXPECT_TRUE(platform_.GetPermissions(file_path, &mode));
  mode_t expected_mode = (mode & ~S_IRWXG & ~S_IRWXO) | S_IRWXU;
  EXPECT_TRUE(platform_.SetPermissions(file_path, expected_mode));
  EXPECT_TRUE(platform_.GetPermissions(symlink_path, &mode));
  EXPECT_EQ(expected_mode, mode);

  mode = 0;
  expected_mode |= S_IRWXG;
  EXPECT_TRUE(platform_.SetPermissions(file_path, expected_mode));
  EXPECT_TRUE(platform_.GetPermissions(symlink_path, &mode));
  EXPECT_EQ(expected_mode, mode);
}

TEST_F(PlatformTest, GetPermissionsOfNonexistentPath) {
  mode_t mode;
  EXPECT_FALSE(platform_.GetPermissions("/nonexistent-path", &mode));
}

TEST_F(PlatformTest, RemoveEmptyDirectory) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  // Nonexistent directory
  base::FilePath new_dir = temp_dir.GetPath().Append("test");
  std::string path = new_dir.value();
  EXPECT_FALSE(platform_.RemoveEmptyDirectory(path));

  // Existent but empty directory
  EXPECT_TRUE(platform_.CreateOrReuseEmptyDirectory(path));
  EXPECT_TRUE(platform_.RemoveEmptyDirectory(path));

  // Existent and non-empty directory
  EXPECT_TRUE(platform_.CreateOrReuseEmptyDirectory(path));
  base::FilePath temp_file;
  ASSERT_TRUE(base::CreateTemporaryFileInDir(new_dir, &temp_file));
  EXPECT_FALSE(platform_.RemoveEmptyDirectory(path));
}

TEST_F(PlatformTest, SetMountUserToRoot) {
  EXPECT_TRUE(platform_.SetMountUser("root"));
  EXPECT_EQ(0, platform_.mount_user_id());
  EXPECT_EQ(0, platform_.mount_group_id());
  EXPECT_EQ("root", platform_.mount_user());
}

TEST_F(PlatformTest, SetMountUserToNonexistentUser) {
  uid_t user_id = platform_.mount_user_id();
  gid_t group_id = platform_.mount_group_id();
  std::string user = platform_.mount_user();
  EXPECT_FALSE(platform_.SetMountUser("nonexistent-user"));
  EXPECT_EQ(user_id, platform_.mount_user_id());
  EXPECT_EQ(group_id, platform_.mount_group_id());
  EXPECT_EQ(user, platform_.mount_user());
}

TEST_F(PlatformTest, SetOwnershipOfNonExistentPath) {
  EXPECT_FALSE(platform_.SetOwnership("/nonexistent-path", getuid(), getgid()));
}

TEST_F(PlatformTest, SetOwnershipOfExistentPath) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  std::string path = temp_dir.GetPath().value();

  EXPECT_TRUE(platform_.SetOwnership(path, getuid(), getgid()));
  EXPECT_TRUE(CheckOwnership(path, getuid(), getgid()));
}

TEST_F(PlatformTest, SetPermissionsOfNonExistentPath) {
  EXPECT_FALSE(platform_.SetPermissions("/nonexistent-path", S_IRWXU));
}

TEST_F(PlatformTest, SetPermissionsOfExistentPath) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  std::string path = temp_dir.GetPath().value();

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
