// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cryptohome/dircrypto-data-migrator/migration-helper.h"

#include <string>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/xattr.h>
#include <unistd.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/files/scoped_temp_dir.h>
#include <base/rand_util.h>

#include "cryptohome/mock_platform.h"
#include "cryptohome/platform.h"

extern "C" {
#include <linux/fs.h>
}

using base::FilePath;
using base::ScopedTempDir;

namespace cryptohome {
namespace dircrypto_data_migrator {

namespace {
constexpr uint64_t kDefaultChunkSize = 128;
constexpr char kMtimeXattrName[] = "user.mtime";
constexpr char kAtimeXattrName[] = "user.atime";
}  // namespace

class MigrationHelperTest : public ::testing::Test {
 public:
  virtual ~MigrationHelperTest() {}

  void SetUp() override {
    ASSERT_TRUE(status_files_dir_.CreateUniqueTempDir());
    ASSERT_TRUE(from_dir_.CreateUniqueTempDir());
    ASSERT_TRUE(to_dir_.CreateUniqueTempDir());
  }

  void TearDown() override {
    EXPECT_TRUE(status_files_dir_.Delete());
    EXPECT_TRUE(from_dir_.Delete());
    EXPECT_TRUE(to_dir_.Delete());
  }

 protected:
  ScopedTempDir status_files_dir_;
  ScopedTempDir from_dir_;
  ScopedTempDir to_dir_;
};

TEST_F(MigrationHelperTest, EmptyTest) {
  Platform platform;
  MigrationHelper helper(
      &platform, status_files_dir_.path(), kDefaultChunkSize);
  helper.set_namespaced_mtime_xattr_name_for_testing(kMtimeXattrName);
  helper.set_namespaced_atime_xattr_name_for_testing(kAtimeXattrName);

  ASSERT_TRUE(base::IsDirectoryEmpty(from_dir_.path()));
  ASSERT_TRUE(base::IsDirectoryEmpty(to_dir_.path()));

  EXPECT_TRUE(helper.Migrate(from_dir_.path(), to_dir_.path()));
}

TEST_F(MigrationHelperTest, CopyAttributesDirectory) {
  Platform platform;
  MigrationHelper helper(
      &platform, status_files_dir_.path(), kDefaultChunkSize);
  helper.set_namespaced_mtime_xattr_name_for_testing(kMtimeXattrName);
  helper.set_namespaced_atime_xattr_name_for_testing(kAtimeXattrName);

  constexpr char kDirectory[] = "directory";
  const FilePath kFromDirPath = from_dir_.path().Append(kDirectory);
  ASSERT_TRUE(platform.CreateDirectory(kFromDirPath));

  // Set some attributes to this directory.

  mode_t mode = S_ISVTX | S_IRUSR | S_IWUSR | S_IXUSR;
  ASSERT_TRUE(platform.SetPermissions(kFromDirPath, mode));
  // GetPermissions call is needed because some bits to mode are applied
  // automatically, so our original |mode| value is not what the resulting file
  // actually has.
  ASSERT_TRUE(platform.GetPermissions(kFromDirPath, &mode));

  constexpr char kAttrName[] = "user.attr";
  constexpr char kValue[] = "value";
  ASSERT_EQ(0,
            lsetxattr(kFromDirPath.value().c_str(),
                      kAttrName,
                      kValue,
                      sizeof(kValue),
                      XATTR_CREATE));

  // Set ext2 attributes
  base::ScopedFD from_fd(
      HANDLE_EINTR(::open(kFromDirPath.value().c_str(), O_RDONLY)));
  ASSERT_TRUE(from_fd.is_valid());
  int ext2_attrs = FS_SYNC_FL | FS_NODUMP_FL;
  ASSERT_EQ(0, ::ioctl(from_fd.get(), FS_IOC_SETFLAGS, &ext2_attrs));

  struct stat from_stat;
  ASSERT_TRUE(platform.Stat(kFromDirPath, &from_stat));
  EXPECT_TRUE(helper.Migrate(from_dir_.path(), to_dir_.path()));

  const FilePath kToDirPath = to_dir_.path().Append(kDirectory);
  struct stat to_stat;
  ASSERT_TRUE(platform.Stat(kToDirPath, &to_stat));
  EXPECT_TRUE(platform.DirectoryExists(kToDirPath));

  // Verify timestamps were coppied
  EXPECT_EQ(from_stat.st_mtim.tv_sec, to_stat.st_mtim.tv_sec);
  EXPECT_EQ(from_stat.st_mtim.tv_nsec, to_stat.st_mtim.tv_nsec);
  EXPECT_EQ(from_stat.st_atim.tv_sec, to_stat.st_atim.tv_sec);
  EXPECT_EQ(from_stat.st_atim.tv_nsec, to_stat.st_atim.tv_nsec);

  // Verify Permissions and xattrs were copied
  mode_t to_mode;
  ASSERT_TRUE(platform.GetPermissions(kToDirPath, &to_mode));
  EXPECT_EQ(mode, to_mode);
  char value[sizeof(kValue) + 1];
  EXPECT_EQ(
      sizeof(kValue),
      lgetxattr(kToDirPath.value().c_str(), kAttrName, &value, sizeof(kValue)));
  value[sizeof(kValue)] = '\0';
  EXPECT_STREQ(kValue, value);

  // Verify ext2 flags were copied
  base::ScopedFD to_fd(
      HANDLE_EINTR(::open(kToDirPath.value().c_str(), O_RDONLY)));
  ASSERT_TRUE(to_fd.is_valid());
  int new_ext2_attrs;
  ASSERT_EQ(0, ::ioctl(to_fd.get(), FS_IOC_GETFLAGS, &new_ext2_attrs));
  EXPECT_EQ(FS_SYNC_FL | FS_NODUMP_FL, new_ext2_attrs);
}

TEST_F(MigrationHelperTest, DirectoryPartiallyMigrated) {
  Platform platform;
  MigrationHelper helper(
      &platform, status_files_dir_.path(), kDefaultChunkSize);
  helper.set_namespaced_mtime_xattr_name_for_testing(kMtimeXattrName);
  helper.set_namespaced_atime_xattr_name_for_testing(kAtimeXattrName);

  constexpr char kDirectory[] = "directory";
  const FilePath kFromDirPath = from_dir_.path().Append(kDirectory);
  ASSERT_TRUE(platform.CreateDirectory(kFromDirPath));
  constexpr struct timespec kMtime = {123, 456};
  constexpr struct timespec kAtime = {234, 567};
  ASSERT_EQ(0,
            lsetxattr(to_dir_.path().value().c_str(),
                      kMtimeXattrName,
                      &kMtime,
                      sizeof(kMtime),
                      XATTR_CREATE));
  ASSERT_EQ(0,
            lsetxattr(to_dir_.path().value().c_str(),
                      kAtimeXattrName,
                      &kAtime,
                      sizeof(kAtime),
                      XATTR_CREATE));

  EXPECT_TRUE(helper.Migrate(from_dir_.path(), to_dir_.path()));
  struct stat to_stat;

  // Verify that stored timestamps for in-progress migrations are respected
  ASSERT_TRUE(platform.Stat(to_dir_.path(), &to_stat));
  EXPECT_EQ(kMtime.tv_sec, to_stat.st_mtim.tv_sec);
  EXPECT_EQ(kMtime.tv_nsec, to_stat.st_mtim.tv_nsec);
  EXPECT_EQ(kAtime.tv_sec, to_stat.st_atim.tv_sec);
  EXPECT_EQ(kAtime.tv_nsec, to_stat.st_atim.tv_nsec);

  // Verify subdirectory was migrated
  const FilePath kToDirPath = to_dir_.path().Append(kDirectory);
  EXPECT_TRUE(platform.DirectoryExists(kToDirPath));
}

TEST_F(MigrationHelperTest, CopySymlink) {
  Platform platform;
  MigrationHelper helper(
      &platform, status_files_dir_.path(), kDefaultChunkSize);
  helper.set_namespaced_mtime_xattr_name_for_testing(kMtimeXattrName);
  helper.set_namespaced_atime_xattr_name_for_testing(kAtimeXattrName);
  FilePath target;

  constexpr char kFileName[] = "file";
  constexpr char kAbsLinkTarget[] = "/dev/null";
  const FilePath kTargetInMigrationDirAbsLinkTarget =
      from_dir_.path().Append(kFileName);
  const FilePath kRelLinkTarget = base::FilePath(kFileName);
  constexpr char kRelLinkName[] = "link1";
  constexpr char kAbsLinkName[] = "link2";
  constexpr char kTargetInMigrationDirAbsLinkName[] = "link3";
  const FilePath kFromRelLinkPath = from_dir_.path().Append(kRelLinkName);
  const FilePath kFromAbsLinkPath = from_dir_.path().Append(kAbsLinkName);
  const FilePath kFromTargetInMigrationDirAbsLinkPath =
      from_dir_.path().Append(kTargetInMigrationDirAbsLinkName);
  ASSERT_TRUE(base::CreateSymbolicLink(kRelLinkTarget, kFromRelLinkPath));
  ASSERT_TRUE(base::CreateSymbolicLink(base::FilePath(kAbsLinkTarget),
                                       kFromAbsLinkPath));
  ASSERT_TRUE(base::CreateSymbolicLink(kTargetInMigrationDirAbsLinkTarget,
                                       kFromTargetInMigrationDirAbsLinkPath));
  struct stat from_stat;
  ASSERT_TRUE(platform.Stat(kFromRelLinkPath, &from_stat));

  EXPECT_TRUE(helper.Migrate(from_dir_.path(), to_dir_.path()));

  const FilePath kToFilePath = to_dir_.path().Append(kFileName);
  const FilePath kToRelLinkPath = to_dir_.path().Append(kRelLinkName);
  const FilePath kToAbsLinkPath = to_dir_.path().Append(kAbsLinkName);
  const FilePath kToTargetInMigrationDirAbsLinkPath =
      to_dir_.path().Append(kTargetInMigrationDirAbsLinkName);
  const FilePath kExpectedTargetInMigrationDirAbsLinkTarget =
      to_dir_.path().Append(kFileName);

  // Verify that timestamps were updated appropriately.
  struct stat to_stat;
  ASSERT_TRUE(platform.Stat(kToRelLinkPath, &to_stat));
  EXPECT_EQ(from_stat.st_atim.tv_sec, to_stat.st_atim.tv_sec);
  EXPECT_EQ(from_stat.st_atim.tv_nsec, to_stat.st_atim.tv_nsec);
  EXPECT_EQ(from_stat.st_mtim.tv_sec, to_stat.st_mtim.tv_sec);
  EXPECT_EQ(from_stat.st_mtim.tv_nsec, to_stat.st_mtim.tv_nsec);

  // Verify that all links have been copied correctly
  EXPECT_TRUE(base::IsLink(kToRelLinkPath));
  EXPECT_TRUE(base::IsLink(kToAbsLinkPath));
  EXPECT_TRUE(base::IsLink(kToTargetInMigrationDirAbsLinkPath));
  EXPECT_TRUE(base::ReadSymbolicLink(kToRelLinkPath, &target));
  EXPECT_EQ(kRelLinkTarget.value(), target.value());
  EXPECT_TRUE(base::ReadSymbolicLink(kToAbsLinkPath, &target));
  EXPECT_EQ(kAbsLinkTarget, target.value());
  EXPECT_TRUE(
      base::ReadSymbolicLink(kToTargetInMigrationDirAbsLinkPath, &target));
  EXPECT_EQ(kExpectedTargetInMigrationDirAbsLinkTarget.value(), target.value());
}

}  // namespace dircrypto_data_migrator
}  // namespace cryptohome
