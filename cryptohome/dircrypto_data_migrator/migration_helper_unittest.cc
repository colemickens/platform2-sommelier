// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cryptohome/dircrypto_data_migrator/migration_helper.h"

#include <string>
#include <vector>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/xattr.h>
#include <unistd.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <base/bind.h>
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

// Connects all used calls on MockPlatform to the concrete implementations in
// real_platform by default.  Tests wishing to mock out only some methods from
// platform may call this initially and then set mock expectations for only the
// methods they care about.
void PassThroughPlatformMethods(MockPlatform* mock_platform,
                                Platform* real_platform) {
  ON_CALL(*mock_platform, TouchFileDurable(testing::_))
      .WillByDefault(
          testing::Invoke(real_platform, &Platform::TouchFileDurable));
  ON_CALL(*mock_platform, DeleteFile(testing::_, testing::_))
      .WillByDefault(testing::Invoke(real_platform, &Platform::DeleteFile));
  ON_CALL(*mock_platform, SyncDirectory(testing::_))
      .WillByDefault(testing::Invoke(real_platform, &Platform::SyncDirectory));
  ON_CALL(*mock_platform, DataSyncFile(testing::_))
      .WillByDefault(testing::Invoke(real_platform, &Platform::DataSyncFile));
  ON_CALL(*mock_platform, SyncFile(testing::_))
      .WillByDefault(testing::Invoke(real_platform, &Platform::SyncFile));
  ON_CALL(*mock_platform, GetFileEnumerator(testing::_, testing::_, testing::_))
      .WillByDefault(
          testing::Invoke(real_platform, &Platform::GetFileEnumerator));
  ON_CALL(*mock_platform, SetPermissions(testing::_, testing::_))
      .WillByDefault(testing::Invoke(real_platform, &Platform::SetPermissions));
  ON_CALL(*mock_platform, GetPermissions(testing::_, testing::_))
      .WillByDefault(testing::Invoke(real_platform, &Platform::GetPermissions));
  ON_CALL(*mock_platform, FileExists(testing::_))
      .WillByDefault(testing::Invoke(real_platform, &Platform::FileExists));
  ON_CALL(*mock_platform, CreateDirectory(testing::_))
      .WillByDefault(
          testing::Invoke(real_platform, &Platform::CreateDirectory));
  ON_CALL(*mock_platform, HasExtendedFileAttribute(testing::_, testing::_))
      .WillByDefault(
          testing::Invoke(real_platform, &Platform::HasExtendedFileAttribute));
  ON_CALL(*mock_platform, ListExtendedFileAttributes(testing::_, testing::_))
      .WillByDefault(testing::Invoke(real_platform,
                                     &Platform::ListExtendedFileAttributes));
  ON_CALL(
      *mock_platform,
      SetExtendedFileAttribute(testing::_, testing::_, testing::_, testing::_))
      .WillByDefault(
          testing::Invoke(real_platform, &Platform::SetExtendedFileAttribute));
  ON_CALL(
      *mock_platform,
      GetExtendedFileAttribute(testing::_, testing::_, testing::_, testing::_))
      .WillByDefault(
          testing::Invoke(real_platform, &Platform::GetExtendedFileAttribute));
  ON_CALL(*mock_platform,
          GetExtendedFileAttributeAsString(testing::_, testing::_, testing::_))
      .WillByDefault(testing::Invoke(
          real_platform, &Platform::GetExtendedFileAttributeAsString));
  ON_CALL(*mock_platform, GetExtFileAttributes(testing::_, testing::_))
      .WillByDefault(
          testing::Invoke(real_platform, &Platform::GetExtFileAttributes));
  ON_CALL(*mock_platform, SetExtFileAttributes(testing::_, testing::_))
      .WillByDefault(
          testing::Invoke(real_platform, &Platform::SetExtFileAttributes));
  ON_CALL(*mock_platform,
          GetOwnership(testing::_, testing::_, testing::_, testing::_))
      .WillByDefault(testing::Invoke(real_platform, &Platform::GetOwnership));
  ON_CALL(*mock_platform,
          SetOwnership(testing::_, testing::_, testing::_, testing::_))
      .WillByDefault(testing::Invoke(real_platform, &Platform::SetOwnership));
  ON_CALL(*mock_platform,
          SetFileTimes(testing::_, testing::_, testing::_, testing::_))
      .WillByDefault(testing::Invoke(real_platform, &Platform::SetFileTimes));
  ON_CALL(*mock_platform, Stat(testing::_, testing::_))
      .WillByDefault(testing::Invoke(real_platform, &Platform::Stat));
  ON_CALL(*mock_platform,
          SendFile(testing::_, testing::_, testing::_, testing::_))
      .WillByDefault(testing::Invoke(real_platform, &Platform::SendFile));
  ON_CALL(*mock_platform, AmountOfFreeDiskSpace(testing::_))
      .WillByDefault(
          testing::Invoke(real_platform, &Platform::AmountOfFreeDiskSpace));
  ON_CALL(*mock_platform, InitializeFile(testing::_, testing::_, testing::_))
      .WillByDefault(testing::Invoke(real_platform, &Platform::InitializeFile));
}

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

  void ProgressCaptor(DircryptoMigrationStatus status,
                      uint64_t migrated,
                      uint64_t total) {
    migrated_values_.push_back(migrated);
    total_values_.push_back(total);
    status_values_.push_back(status);
  }

 protected:
  ScopedTempDir status_files_dir_;
  ScopedTempDir from_dir_;
  ScopedTempDir to_dir_;
  std::vector<uint64_t> migrated_values_;
  std::vector<uint64_t> total_values_;
  std::vector<DircryptoMigrationStatus> status_values_;
};

TEST_F(MigrationHelperTest, EmptyTest) {
  Platform platform;
  MigrationHelper helper(
      &platform, from_dir_.path(), to_dir_.path(), status_files_dir_.path(),
      kDefaultChunkSize);
  helper.set_namespaced_mtime_xattr_name_for_testing(kMtimeXattrName);
  helper.set_namespaced_atime_xattr_name_for_testing(kAtimeXattrName);

  ASSERT_TRUE(base::IsDirectoryEmpty(from_dir_.path()));
  ASSERT_TRUE(base::IsDirectoryEmpty(to_dir_.path()));

  EXPECT_TRUE(helper.Migrate(base::Bind(&MigrationHelperTest::ProgressCaptor,
                                        base::Unretained(this))));
}

TEST_F(MigrationHelperTest, CopyAttributesDirectory) {
  // This test only covers permissions and xattrs.  Ownership copying requires
  // more extensive mocking and is covered in CopyOwnership test.
  Platform platform;
  MigrationHelper helper(
      &platform, from_dir_.path(), to_dir_.path(), status_files_dir_.path(),
      kDefaultChunkSize);
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
  EXPECT_TRUE(helper.Migrate(base::Bind(&MigrationHelperTest::ProgressCaptor,
                                        base::Unretained(this))));

  const FilePath kToDirPath = to_dir_.path().Append(kDirectory);
  struct stat to_stat;
  ASSERT_TRUE(platform.Stat(kToDirPath, &to_stat));
  EXPECT_TRUE(platform.DirectoryExists(kToDirPath));

  // Verify mtime was coppied.  atime for directories is not
  // well-preserved because we have to traverse the directories to determine
  // migration size.
  EXPECT_EQ(from_stat.st_mtim.tv_sec, to_stat.st_mtim.tv_sec);
  EXPECT_EQ(from_stat.st_mtim.tv_nsec, to_stat.st_mtim.tv_nsec);

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
      &platform, from_dir_.path(), to_dir_.path(), status_files_dir_.path(),
      kDefaultChunkSize);
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

  EXPECT_TRUE(helper.Migrate(base::Bind(&MigrationHelperTest::ProgressCaptor,
                                        base::Unretained(this))));
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
  // This test does not cover setting ownership values as that requires more
  // extensive mocking.  Ownership copying instead is covered by the
  // CopyOwnership test.
  Platform platform;
  MigrationHelper helper(
      &platform, from_dir_.path(), to_dir_.path(), status_files_dir_.path(),
      kDefaultChunkSize);
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

  EXPECT_TRUE(helper.Migrate(base::Bind(&MigrationHelperTest::ProgressCaptor,
                                        base::Unretained(this))));

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

TEST_F(MigrationHelperTest, OneEmptyFile) {
  Platform platform;
  MigrationHelper helper(
      &platform, from_dir_.path(), to_dir_.path(), status_files_dir_.path(),
      kDefaultChunkSize);
  helper.set_namespaced_mtime_xattr_name_for_testing(kMtimeXattrName);
  helper.set_namespaced_atime_xattr_name_for_testing(kAtimeXattrName);

  constexpr char kFileName[] = "empty_file";

  ASSERT_TRUE(platform.TouchFileDurable(from_dir_.path().Append(kFileName)));
  ASSERT_TRUE(base::IsDirectoryEmpty(to_dir_.path()));

  EXPECT_TRUE(helper.Migrate(base::Bind(&MigrationHelperTest::ProgressCaptor,
                                        base::Unretained(this))));

  // The file is moved.
  EXPECT_FALSE(platform.FileExists(from_dir_.path().Append(kFileName)));
  EXPECT_TRUE(platform.FileExists(to_dir_.path().Append(kFileName)));
}

TEST_F(MigrationHelperTest, OneEmptyFileInDirectory) {
  Platform platform;
  MigrationHelper helper(
      &platform, from_dir_.path(), to_dir_.path(), status_files_dir_.path(),
      kDefaultChunkSize);
  helper.set_namespaced_mtime_xattr_name_for_testing(kMtimeXattrName);
  helper.set_namespaced_atime_xattr_name_for_testing(kAtimeXattrName);

  constexpr char kDir1[] = "directory1";
  constexpr char kDir2[] = "directory2";
  constexpr char kFileName[] = "empty_file";

  // Create directory1/directory2/empty_file in from_dir_.
  ASSERT_TRUE(
      platform.CreateDirectory(from_dir_.path().Append(kDir1).Append(kDir2)));
  ASSERT_TRUE(platform.TouchFileDurable(
      from_dir_.path().Append(kDir1).Append(kDir2).Append(kFileName)));
  ASSERT_TRUE(base::IsDirectoryEmpty(to_dir_.path()));

  EXPECT_TRUE(helper.Migrate(base::Bind(&MigrationHelperTest::ProgressCaptor,
                                        base::Unretained(this))));

  // The file is moved.
  EXPECT_FALSE(platform.FileExists(
      from_dir_.path().Append(kDir1).Append(kDir2).Append(kFileName)));
  EXPECT_TRUE(base::IsDirectoryEmpty(from_dir_.path().Append(kDir1)));
  EXPECT_TRUE(platform.FileExists(
      to_dir_.path().Append(kDir1).Append(kDir2).Append(kFileName)));
}

TEST_F(MigrationHelperTest, UnreadableFile) {
  Platform platform;
  MigrationHelper helper(
      &platform, from_dir_.path(), to_dir_.path(), status_files_dir_.path(),
      kDefaultChunkSize);
  helper.set_namespaced_mtime_xattr_name_for_testing(kMtimeXattrName);
  helper.set_namespaced_atime_xattr_name_for_testing(kAtimeXattrName);

  constexpr char kDir1[] = "directory1";
  constexpr char kDir2[] = "directory2";
  constexpr char kFileName[] = "empty_file";

  // Create directory1/directory2/empty_file in from_dir_.  File will be
  // unreadable to test failure case.
  ASSERT_TRUE(
      platform.CreateDirectory(from_dir_.path().Append(kDir1).Append(kDir2)));
  ASSERT_TRUE(platform.TouchFileDurable(
      from_dir_.path().Append(kDir1).Append(kDir2).Append(kFileName)));
  ASSERT_TRUE(base::IsDirectoryEmpty(to_dir_.path()));
  ASSERT_TRUE(platform.SetPermissions(
      from_dir_.path().Append(kDir1).Append(kDir2).Append(kFileName), S_IWUSR));

  EXPECT_FALSE(helper.Migrate(base::Bind(&MigrationHelperTest::ProgressCaptor,
                                         base::Unretained(this))));

  // The file is not moved.
  EXPECT_TRUE(platform.FileExists(
      from_dir_.path().Append(kDir1).Append(kDir2).Append(kFileName)));
}

TEST_F(MigrationHelperTest, CopyAttributesFile) {
  // This test does not cover setting ownership values as that requires more
  // extensive mocking.  Ownership copying instead is covered by the
  // CopyOwnership test.
  Platform platform;
  MigrationHelper helper(
      &platform, from_dir_.path(), to_dir_.path(), status_files_dir_.path(),
      kDefaultChunkSize);
  helper.set_namespaced_mtime_xattr_name_for_testing(kMtimeXattrName);
  helper.set_namespaced_atime_xattr_name_for_testing(kAtimeXattrName);

  constexpr char kFileName[] = "file";
  const FilePath kFromFilePath = from_dir_.path().Append(kFileName);
  const FilePath kToFilePath = to_dir_.path().Append(kFileName);

  ASSERT_TRUE(platform.TouchFileDurable(from_dir_.path().Append(kFileName)));
  // Set some attributes to this file.

  mode_t mode = S_ISVTX | S_IRUSR | S_IWUSR | S_IXUSR;
  ASSERT_TRUE(platform.SetPermissions(kFromFilePath, mode));
  // GetPermissions call is needed because some bits to mode are applied
  // automatically, so our original |mode| value is not what the resulting file
  // actually has.
  ASSERT_TRUE(platform.GetPermissions(kFromFilePath, &mode));

  constexpr char kAttrName[] = "user.attr";
  constexpr char kValue[] = "value";
  ASSERT_EQ(0,
            lsetxattr(kFromFilePath.value().c_str(),
                      kAttrName,
                      kValue,
                      sizeof(kValue),
                      XATTR_CREATE));

  // Set ext2 attributes
  base::ScopedFD from_fd(
      HANDLE_EINTR(::open(kFromFilePath.value().c_str(), O_RDONLY)));
  ASSERT_TRUE(from_fd.is_valid());
  int ext2_attrs = FS_SYNC_FL | FS_NODUMP_FL;
  EXPECT_EQ(0, ::ioctl(from_fd.get(), FS_IOC_SETFLAGS, &ext2_attrs));

  struct stat from_stat;
  ASSERT_TRUE(platform.Stat(kFromFilePath, &from_stat));
  EXPECT_TRUE(helper.Migrate(base::Bind(&MigrationHelperTest::ProgressCaptor,
                                        base::Unretained(this))));

  struct stat to_stat;
  ASSERT_TRUE(platform.Stat(kToFilePath, &to_stat));
  EXPECT_EQ(from_stat.st_atim.tv_sec, to_stat.st_atim.tv_sec);
  EXPECT_EQ(from_stat.st_atim.tv_nsec, to_stat.st_atim.tv_nsec);
  EXPECT_EQ(from_stat.st_mtim.tv_sec, to_stat.st_mtim.tv_sec);
  EXPECT_EQ(from_stat.st_mtim.tv_nsec, to_stat.st_mtim.tv_nsec);

  EXPECT_TRUE(platform.FileExists(kToFilePath));

  mode_t permission;
  ASSERT_TRUE(platform.GetPermissions(kToFilePath, &permission));
  EXPECT_EQ(mode, permission);
  char value[sizeof(kValue) + 1];
  EXPECT_EQ(
      sizeof(kValue),
      lgetxattr(
          kToFilePath.value().c_str(), kAttrName, &value, sizeof(kValue)));
  value[sizeof(kValue)] = '\0';
  EXPECT_STREQ(kValue, value);

  base::ScopedFD to_fd(
      HANDLE_EINTR(::open(kToFilePath.value().c_str(), O_RDONLY)));
  EXPECT_TRUE(to_fd.is_valid());
  int new_ext2_attrs;
  EXPECT_EQ(0, ::ioctl(to_fd.get(), FS_IOC_GETFLAGS, &new_ext2_attrs));
  EXPECT_EQ(FS_SYNC_FL | FS_NODUMP_FL, new_ext2_attrs);
}

TEST_F(MigrationHelperTest, CopyOwnership) {
  // Ownership changes for regular files and symlinks can't be tested normally
  // due to how we get ownership information via file enumerator.  Instead we
  // directly test CopyAttributes with modified FileInfo arguments.
  testing::NiceMock<MockPlatform> mock_platform;
  Platform real_platform;
  PassThroughPlatformMethods(&mock_platform, &real_platform);
  MigrationHelper helper(
      &mock_platform, from_dir_.path(), to_dir_.path(),
      status_files_dir_.path(), kDefaultChunkSize);
  helper.set_namespaced_mtime_xattr_name_for_testing(kMtimeXattrName);
  helper.set_namespaced_atime_xattr_name_for_testing(kAtimeXattrName);

  const base::FilePath kLinkTarget = base::FilePath("foo");
  const base::FilePath kLink(FILE_PATH_LITERAL("link"));
  const base::FilePath kFile(FILE_PATH_LITERAL("file"));
  const base::FilePath kDir(FILE_PATH_LITERAL("dir"));
  const base::FilePath kFromLink = from_dir_.path().Append(kLink);
  const base::FilePath kFromFile = from_dir_.path().Append(kFile);
  const base::FilePath kFromDir = from_dir_.path().Append(kDir);
  const base::FilePath kToLink = to_dir_.path().Append(kLink);
  const base::FilePath kToFile = to_dir_.path().Append(kFile);
  const base::FilePath kToDir = to_dir_.path().Append(kDir);
  uid_t file_uid = 1;
  gid_t file_gid = 2;
  uid_t link_uid = 3;
  gid_t link_gid = 4;
  uid_t dir_uid = 5;
  gid_t dir_gid = 6;
  ASSERT_TRUE(real_platform.TouchFileDurable(kFromFile));
  ASSERT_TRUE(base::CreateSymbolicLink(kLinkTarget, kFromLink));
  ASSERT_TRUE(real_platform.CreateDirectory(kFromDir));
  ASSERT_TRUE(real_platform.TouchFileDurable(kToFile));
  ASSERT_TRUE(base::CreateSymbolicLink(kLinkTarget, kToLink));
  ASSERT_TRUE(real_platform.CreateDirectory(kToDir));

  struct stat stat;
  ASSERT_TRUE(real_platform.Stat(kFromFile, &stat));
  stat.st_uid = file_uid;
  stat.st_gid = file_gid;
  EXPECT_CALL(mock_platform, SetOwnership(kToFile, file_uid, file_gid, false))
      .WillOnce(testing::Return(true));
  EXPECT_TRUE(helper.CopyAttributes(
      kFile, FileEnumerator::FileInfo(kFromFile, stat)));

  ASSERT_TRUE(real_platform.Stat(kFromLink, &stat));
  stat.st_uid = link_uid;
  stat.st_gid = link_gid;
  EXPECT_CALL(mock_platform, SetOwnership(kToLink, link_uid, link_gid, false))
      .WillOnce(testing::Return(true));
  EXPECT_TRUE(helper.CopyAttributes(
      kLink, FileEnumerator::FileInfo(kFromLink, stat)));

  ASSERT_TRUE(real_platform.Stat(kFromDir, &stat));
  stat.st_uid = dir_uid;
  stat.st_gid = dir_gid;
  EXPECT_CALL(mock_platform, SetOwnership(kToDir, dir_uid, dir_gid, false))
      .WillOnce(testing::Return(true));
  EXPECT_TRUE(helper.CopyAttributes(
      kDir, FileEnumerator::FileInfo(kFromDir, stat)));
}

TEST_F(MigrationHelperTest, MigrateNestedDir) {
  Platform platform;
  MigrationHelper helper(
      &platform, from_dir_.path(), to_dir_.path(), status_files_dir_.path(),
      kDefaultChunkSize);
  helper.set_namespaced_mtime_xattr_name_for_testing(kMtimeXattrName);
  helper.set_namespaced_atime_xattr_name_for_testing(kAtimeXattrName);

  constexpr char kDir1[] = "directory1";
  constexpr char kDir2[] = "directory2";
  constexpr char kFileName[] = "empty_file";

  // Create directory1/directory2/empty_file in from_dir_.
  ASSERT_TRUE(
      platform.CreateDirectory(from_dir_.path().Append(kDir1).Append(kDir2)));
  ASSERT_TRUE(platform.TouchFileDurable(
      from_dir_.path().Append(kDir1).Append(kDir2).Append(kFileName)));
  ASSERT_TRUE(base::IsDirectoryEmpty(to_dir_.path()));

  EXPECT_TRUE(helper.Migrate(base::Bind(&MigrationHelperTest::ProgressCaptor,
                                        base::Unretained(this))));

  // The file is moved.
  EXPECT_TRUE(platform.FileExists(
      to_dir_.path().Append(kDir1).Append(kDir2).Append(kFileName)));
  EXPECT_FALSE(platform.FileExists(
      from_dir_.path().Append(kDir1).Append(kDir2).Append(kFileName)));
  EXPECT_TRUE(base::IsDirectoryEmpty(from_dir_.path().Append(kDir1)));
}

TEST_F(MigrationHelperTest, MigrateInProgress) {
  // Test the case where the migration was interrupted part way through, but in
  // a clean way such that the two directory trees are consistent (files are
  // only present in one or the other)
  Platform platform;
  MigrationHelper helper(
      &platform, from_dir_.path(), to_dir_.path(), status_files_dir_.path(),
      kDefaultChunkSize);
  helper.set_namespaced_mtime_xattr_name_for_testing(kMtimeXattrName);
  helper.set_namespaced_atime_xattr_name_for_testing(kAtimeXattrName);

  constexpr char kFile1[] = "kFile1";
  constexpr char kFile2[] = "kFile2";
  ASSERT_TRUE(platform.TouchFileDurable(from_dir_.path().Append(kFile1)));
  ASSERT_TRUE(platform.TouchFileDurable(to_dir_.path().Append(kFile2)));
  EXPECT_TRUE(helper.Migrate(base::Bind(&MigrationHelperTest::ProgressCaptor,
                                        base::Unretained(this))));

  // Both files have been moved to to_dir_
  EXPECT_TRUE(platform.FileExists(to_dir_.path().Append(kFile1)));
  EXPECT_TRUE(platform.FileExists(to_dir_.path().Append(kFile2)));
  EXPECT_FALSE(platform.FileExists(from_dir_.path().Append(kFile1)));
  EXPECT_FALSE(platform.FileExists(from_dir_.path().Append(kFile2)));
}

TEST_F(MigrationHelperTest, MigrateInProgressDuplicateFile) {
  // Test the case where the migration was interrupted part way through,
  // resulting in files that were successfully written to destination but not
  // yet removed from the source.
  Platform platform;
  MigrationHelper helper(
      &platform, from_dir_.path(), to_dir_.path(), status_files_dir_.path(),
      kDefaultChunkSize);
  helper.set_namespaced_mtime_xattr_name_for_testing(kMtimeXattrName);
  helper.set_namespaced_atime_xattr_name_for_testing(kAtimeXattrName);

  constexpr char kFile1[] = "kFile1";
  constexpr char kFile2[] = "kFile2";
  ASSERT_TRUE(platform.TouchFileDurable(from_dir_.path().Append(kFile1)));
  ASSERT_TRUE(platform.TouchFileDurable(to_dir_.path().Append(kFile1)));
  ASSERT_TRUE(platform.TouchFileDurable(to_dir_.path().Append(kFile2)));
  EXPECT_TRUE(helper.Migrate(base::Bind(&MigrationHelperTest::ProgressCaptor,
                                        base::Unretained(this))));

  // Both files have been moved to to_dir_
  EXPECT_TRUE(platform.FileExists(to_dir_.path().Append(kFile1)));
  EXPECT_TRUE(platform.FileExists(to_dir_.path().Append(kFile2)));
  EXPECT_FALSE(platform.FileExists(from_dir_.path().Append(kFile1)));
  EXPECT_FALSE(platform.FileExists(from_dir_.path().Append(kFile2)));
}

TEST_F(MigrationHelperTest, MigrateInProgressPartialFile) {
  // Test the case where the migration was interrupted part way through, with a
  // file having been partially copied to the destination but not fully.
  Platform platform;
  MigrationHelper helper(
      &platform, from_dir_.path(), to_dir_.path(), status_files_dir_.path(),
      kDefaultChunkSize);
  helper.set_namespaced_mtime_xattr_name_for_testing(kMtimeXattrName);
  helper.set_namespaced_atime_xattr_name_for_testing(kAtimeXattrName);

  constexpr char kFileName[] = "file";
  const FilePath kFromFilePath = from_dir_.path().Append(kFileName);
  const FilePath kToFilePath = to_dir_.path().Append(kFileName);

  const size_t kFinalFileSize = kDefaultChunkSize * 2;
  const size_t kFromFileSize = kDefaultChunkSize;
  const size_t kToFileSize = kDefaultChunkSize;
  char full_contents[kFinalFileSize];
  base::RandBytes(full_contents, kFinalFileSize);
  ASSERT_EQ(kDefaultChunkSize,
            base::WriteFile(kFromFilePath, full_contents, kFromFileSize));
  base::File kToFile =
      base::File(kToFilePath, base::File::FLAG_CREATE | base::File::FLAG_WRITE);
  kToFile.SetLength(kFinalFileSize);
  const size_t kToFileOffset = kFinalFileSize - kToFileSize;
  ASSERT_EQ(
      kToFileSize,
      kToFile.Write(kToFileOffset, full_contents + kToFileOffset, kToFileSize));
  ASSERT_EQ(kFinalFileSize, kToFile.GetLength());
  kToFile.Close();

  EXPECT_TRUE(helper.Migrate(base::Bind(&MigrationHelperTest::ProgressCaptor,
                                        base::Unretained(this))));

  // File has been moved to to_dir_
  char to_contents[kFinalFileSize];
  EXPECT_EQ(kFinalFileSize,
            base::ReadFile(kToFilePath, to_contents, kFinalFileSize));
  EXPECT_EQ(std::string(full_contents, kFinalFileSize),
            std::string(to_contents, kFinalFileSize));
  EXPECT_FALSE(platform.FileExists(kFromFilePath));
}

TEST_F(MigrationHelperTest, MigrateInProgressPartialFileDuplicateData) {
  // Test the case where the migration was interrupted part way through, with a
  // file having been partially copied to the destination but the source file
  // not yet having been truncated to reflect that.
  Platform platform;
  MigrationHelper helper(
      &platform, from_dir_.path(), to_dir_.path(), status_files_dir_.path(),
      kDefaultChunkSize);
  helper.set_namespaced_mtime_xattr_name_for_testing(kMtimeXattrName);
  helper.set_namespaced_atime_xattr_name_for_testing(kAtimeXattrName);

  constexpr char kFileName[] = "file";
  const FilePath kFromFilePath = from_dir_.path().Append(kFileName);
  const FilePath kToFilePath = to_dir_.path().Append(kFileName);

  const size_t kFinalFileSize = kDefaultChunkSize * 2;
  const size_t kFromFileSize = kFinalFileSize;
  const size_t kToFileSize = kDefaultChunkSize;
  char full_contents[kFinalFileSize];
  base::RandBytes(full_contents, kFinalFileSize);
  ASSERT_EQ(kFromFileSize,
            base::WriteFile(kFromFilePath, full_contents, kFromFileSize));
  base::File kToFile =
      base::File(kToFilePath, base::File::FLAG_CREATE | base::File::FLAG_WRITE);
  kToFile.SetLength(kFinalFileSize);
  const size_t kToFileOffset = kFinalFileSize - kToFileSize;
  ASSERT_EQ(
      kDefaultChunkSize,
      kToFile.Write(kToFileOffset, full_contents + kToFileOffset, kToFileSize));
  ASSERT_EQ(kFinalFileSize, kToFile.GetLength());
  kToFile.Close();

  EXPECT_TRUE(helper.Migrate(base::Bind(&MigrationHelperTest::ProgressCaptor,
                                        base::Unretained(this))));

  // File has been moved to to_dir_
  char to_contents[kFinalFileSize];
  EXPECT_EQ(kFinalFileSize,
            base::ReadFile(kToFilePath, to_contents, kFinalFileSize));
  EXPECT_EQ(std::string(full_contents, kFinalFileSize),
            std::string(to_contents, kFinalFileSize));
  EXPECT_FALSE(platform.FileExists(kFromFilePath));
}

TEST_F(MigrationHelperTest, ProgressCallback) {
  Platform platform;
  MigrationHelper helper(
      &platform, from_dir_.path(), to_dir_.path(), status_files_dir_.path(),
      kDefaultChunkSize);
  helper.set_namespaced_mtime_xattr_name_for_testing(kMtimeXattrName);
  helper.set_namespaced_atime_xattr_name_for_testing(kAtimeXattrName);

  constexpr char kFileName[] = "file";
  constexpr char kLinkName[] = "link";
  constexpr char kDirName[] = "dir";
  const FilePath kFromSubdir = from_dir_.path().Append(kDirName);
  const FilePath kFromFile = kFromSubdir.Append(kFileName);
  const FilePath kFromLink = kFromSubdir.Append(kLinkName);
  const FilePath kToSubdir = to_dir_.path().Append(kDirName);
  const FilePath kToFile = kToSubdir.Append(kFileName);

  const size_t kFileSize = kDefaultChunkSize;
  char from_contents[kFileSize];
  base::RandBytes(from_contents, kFileSize);
  ASSERT_TRUE(base::CreateDirectory(kFromSubdir));
  ASSERT_TRUE(base::CreateSymbolicLink(kFromFile.BaseName(), kFromLink));
  ASSERT_EQ(kFileSize, base::WriteFile(kFromFile, from_contents, kFileSize));

  int64_t expected_size = kFileSize;
  expected_size += kFromFile.BaseName().value().length();
  int64_t dir_size;
  ASSERT_TRUE(platform.GetFileSize(kFromSubdir, &dir_size));
  expected_size += dir_size;

  EXPECT_TRUE(helper.Migrate(base::Bind(&MigrationHelperTest::ProgressCaptor,
                                        base::Unretained(this))));

  ASSERT_EQ(migrated_values_.size(), total_values_.size());
  int callbacks = migrated_values_.size();
  EXPECT_GT(callbacks, 2);
  EXPECT_EQ(callbacks, total_values_.size());
  EXPECT_EQ(callbacks, status_values_.size());

  // Verify that the progress goes from initializing to in_progress.
  EXPECT_EQ(DIRCRYPTO_MIGRATION_INITIALIZING, status_values_[0]);
  for (int i = 1; i < callbacks; i++) {
    SCOPED_TRACE(i);
    EXPECT_EQ(DIRCRYPTO_MIGRATION_IN_PROGRESS, status_values_[i]);
  }

  // Verify that migrated value starts at 0 and increases to total
  EXPECT_EQ(0, migrated_values_[1]);
  for (int i = 2; i < callbacks - 1; i++) {
    SCOPED_TRACE(i);
    EXPECT_GE(migrated_values_[i], migrated_values_[i - 1]);
  }
  EXPECT_EQ(expected_size, migrated_values_[callbacks - 1]);

  // Verify that total always matches the expected size
  EXPECT_EQ(callbacks, total_values_.size());
  for (int i = 1; i < callbacks; i++) {
    SCOPED_TRACE(i);
    EXPECT_EQ(expected_size, total_values_[i]);
  }
}

TEST_F(MigrationHelperTest, NotEnoughFreeSpace) {
  testing::NiceMock<MockPlatform> mock_platform;
  Platform real_platform;
  PassThroughPlatformMethods(&mock_platform, &real_platform);
  MigrationHelper helper(
      &mock_platform, from_dir_.path(), to_dir_.path(),
      status_files_dir_.path(), kDefaultChunkSize);

  EXPECT_CALL(mock_platform, AmountOfFreeDiskSpace(testing::_))
      .WillOnce(testing::Return(0));
  EXPECT_FALSE(helper.Migrate(base::Bind(&MigrationHelperTest::ProgressCaptor,
                                         base::Unretained(this))));
}

TEST_F(MigrationHelperTest, ForceSmallerChunkSize) {
  testing::NiceMock<MockPlatform> mock_platform;
  Platform real_platform;
  PassThroughPlatformMethods(&mock_platform, &real_platform);
  constexpr int kMaxChunkSize = 128 << 20;  // 128MB
  MigrationHelper helper(
      &mock_platform, from_dir_.path(), to_dir_.path(),
      status_files_dir_.path(), kMaxChunkSize);
  helper.set_namespaced_mtime_xattr_name_for_testing(kMtimeXattrName);
  helper.set_namespaced_atime_xattr_name_for_testing(kAtimeXattrName);

  constexpr int kFreeSpace = 9 << 20;
  // Chunk size should be limited to a multiple of 4MB (kErasureBlockSize)
  // smaller than kFreeSpace - kFreeSpaceBuffer (4MB)
  constexpr int kExpectedChunkSize = 4 << 20;
  constexpr int kFileSize = 7 << 20;
  const FilePath kFromFilePath = from_dir_.path().Append("file");
  base::File from_file(kFromFilePath,
                       base::File::FLAG_CREATE | base::File::FLAG_WRITE);
  from_file.SetLength(kFileSize);
  from_file.Close();

  EXPECT_CALL(mock_platform, AmountOfFreeDiskSpace(testing::_))
      .WillOnce(testing::Return(kFreeSpace));
  EXPECT_CALL(mock_platform,
              SendFile(testing::_,
                       testing::_,
                       kExpectedChunkSize,
                       kFileSize - kExpectedChunkSize))
      .WillOnce(testing::Return(true));
  EXPECT_CALL(mock_platform,
              SendFile(testing::_, testing::_, 0, kExpectedChunkSize))
      .WillOnce(testing::Return(true));
  EXPECT_TRUE(helper.Migrate(base::Bind(&MigrationHelperTest::ProgressCaptor,
                                        base::Unretained(this))));
}

TEST_F(MigrationHelperTest, SkipInvalidSQLiteFiles) {
  testing::NiceMock<MockPlatform> mock_platform;
  Platform real_platform;
  PassThroughPlatformMethods(&mock_platform, &real_platform);
  MigrationHelper helper(
      &mock_platform, from_dir_.path(), to_dir_.path(),
      status_files_dir_.path(), kDefaultChunkSize);
  helper.set_namespaced_mtime_xattr_name_for_testing(kMtimeXattrName);
  helper.set_namespaced_atime_xattr_name_for_testing(kAtimeXattrName);

  const FilePath kFromSQLiteShm = from_dir_.path().Append(kKnownCorruptions[0]);
  const FilePath kToSQLiteShm = to_dir_.path().Append(kKnownCorruptions[0]);
  ASSERT_TRUE(base::CreateDirectory(kFromSQLiteShm.DirName()));
  ASSERT_TRUE(real_platform.TouchFileDurable(kFromSQLiteShm));
  EXPECT_CALL(mock_platform, InitializeFile(testing::_, testing::_, testing::_))
      .WillRepeatedly(testing::DoDefault());
  EXPECT_CALL(mock_platform,
              InitializeFile(testing::_, kFromSQLiteShm, testing::_))
      .WillOnce(testing::Invoke(
          [](base::File* file, const FilePath& path, uint32_t mode) {
            *file = base::File(base::File::FILE_ERROR_IO);
          }));

  EXPECT_TRUE(helper.Migrate(base::Bind(&MigrationHelperTest::ProgressCaptor,
                                        base::Unretained(this))));
  EXPECT_TRUE(real_platform.DirectoryExists(kToSQLiteShm.DirName()));
  EXPECT_FALSE(real_platform.FileExists(kToSQLiteShm));
  EXPECT_FALSE(real_platform.FileExists(kFromSQLiteShm));
}

class DataMigrationTest : public MigrationHelperTest,
                          public ::testing::WithParamInterface<size_t> {};

TEST_P(DataMigrationTest, CopyFileData) {
  Platform platform;
  MigrationHelper helper(
      &platform, from_dir_.path(), to_dir_.path(), status_files_dir_.path(),
      kDefaultChunkSize);
  helper.set_namespaced_mtime_xattr_name_for_testing(kMtimeXattrName);
  helper.set_namespaced_atime_xattr_name_for_testing(kAtimeXattrName);

  constexpr char kFileName[] = "file";
  const FilePath kFromFile = from_dir_.path().Append(kFileName);
  const FilePath kToFile = to_dir_.path().Append(kFileName);

  const size_t kFileSize = GetParam();
  char from_contents[kFileSize];
  base::RandBytes(from_contents, kFileSize);
  ASSERT_EQ(kFileSize, base::WriteFile(kFromFile, from_contents, kFileSize));

  EXPECT_TRUE(helper.Migrate(base::Bind(&MigrationHelperTest::ProgressCaptor,
                                        base::Unretained(this))));

  char to_contents[kFileSize];
  EXPECT_EQ(kFileSize, base::ReadFile(kToFile, to_contents, kFileSize));
  EXPECT_EQ(0, strncmp(from_contents, to_contents, kFileSize));
  EXPECT_FALSE(platform.FileExists(kFromFile));
}

INSTANTIATE_TEST_CASE_P(WithRandomData,
                        DataMigrationTest,
                        ::testing::Values(kDefaultChunkSize / 2,
                                          kDefaultChunkSize,
                                          kDefaultChunkSize * 2,
                                          kDefaultChunkSize * 2 +
                                              kDefaultChunkSize / 2,
                                          kDefaultChunkSize * 10,
                                          kDefaultChunkSize * 100,
                                          123456,
                                          1,
                                          2));

}  // namespace dircrypto_data_migrator
}  // namespace cryptohome
