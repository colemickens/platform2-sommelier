// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cros-disks/drivefs_helper.h"

#include <sys/mount.h>

#include <base/strings/string_util.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "cros-disks/fuse_mounter.h"
#include "cros-disks/mount_options.h"
#include "cros-disks/platform.h"
#include "cros-disks/uri.h"

namespace cros_disks {
namespace {

using testing::_;
using testing::DoAll;
using testing::EndsWith;
using testing::Eq;
using testing::HasSubstr;
using testing::Invoke;
using testing::Return;
using testing::SetArgPointee;
using testing::StrEq;

const uid_t kMountUID = 200;
const gid_t kMountGID = 201;
const uid_t kFilesUID = 700;
const uid_t kFilesGID = 701;
const uid_t kFilesAccessGID = 1501;
const char kSomeDirPath[] = "/foo/bar";

// Mock Platform implementation for testing.
class MockPlatform : public Platform {
 public:
  MockPlatform() {
    ON_CALL(*this, GetUserAndGroupId(_, _, _))
        .WillByDefault(Invoke(this, &MockPlatform::GetUserAndGroupIdImpl));
    ON_CALL(*this, GetGroupId(_, _))
        .WillByDefault(Invoke(this, &MockPlatform::GetGroupIdImpl));
    ON_CALL(*this, GetRealPath(_, _))
        .WillByDefault(Invoke(this, &MockPlatform::GetRealPathImpl));
    ON_CALL(*this, IsDirectoryEmpty(_)).WillByDefault(Return(true));
    ON_CALL(*this, DirectoryExists(_)).WillByDefault(Return(true));
    ON_CALL(*this, PathExists(EndsWith("-seccomp.policy")))
        .WillByDefault(Return(false));
  }

  MOCK_CONST_METHOD2(GetRealPath, bool(const std::string&, std::string*));
  MOCK_CONST_METHOD3(GetUserAndGroupId,
                     bool(const std::string&, uid_t* user_id, gid_t* group_id));
  MOCK_CONST_METHOD2(GetGroupId, bool(const std::string&, gid_t* group_id));
  MOCK_CONST_METHOD5(Mount,
                     bool(const std::string&,
                          const std::string&,
                          const std::string&,
                          MountOptions::Flags,
                          const std::string&));
  MOCK_CONST_METHOD1(PathExists, bool(const std::string& path));
  MOCK_CONST_METHOD1(DirectoryExists, bool(const std::string& path));
  MOCK_CONST_METHOD1(IsDirectoryEmpty, bool(const std::string& path));
  MOCK_CONST_METHOD1(CreateDirectory, bool(const std::string& path));
  MOCK_CONST_METHOD1(RemoveEmptyDirectory, bool(const std::string& path));
  MOCK_CONST_METHOD3(SetOwnership,
                     bool(const std::string& path,
                          uid_t user_id,
                          gid_t group_id));
  MOCK_CONST_METHOD3(GetOwnership,
                     bool(const std::string& path,
                          uid_t* user_id,
                          gid_t* group_id));
  MOCK_CONST_METHOD2(SetPermissions,
                     bool(const std::string& path, mode_t mode));

 private:
  bool GetRealPathImpl(const std::string& path, std::string* real_path) const {
    *real_path = "/foo/bar";
    return true;
  }

  bool GetUserAndGroupIdImpl(const std::string& user,
                             uid_t* user_id,
                             gid_t* group_id) const {
    if (user == FUSEHelper::kFilesUser) {
      if (user_id)
        *user_id = kFilesUID;
      if (group_id)
        *group_id = kFilesGID;
      return true;
    }
    if (user == "fuse-drivefs") {
      if (user_id)
        *user_id = kMountUID;
      if (group_id)
        *group_id = kMountGID;
      return true;
    }
    return false;
  }

  bool GetGroupIdImpl(const std::string& group, gid_t* group_id) const {
    if (group == FUSEHelper::kFilesGroup) {
      if (group_id)
        *group_id = kFilesAccessGID;
      return true;
    }
    return false;
  }
};

class TestDrivefsHelper : public DrivefsHelper {
 public:
  explicit TestDrivefsHelper(const Platform* platform)
      : DrivefsHelper(platform) {
    ON_CALL(*this, SetupDirectoryForFUSEAccess(_))
        .WillByDefault(Invoke(
            this,
            &TestDrivefsHelper::ForwardSetupDirectoryForFUSEAccessToImpl));
  }

  MOCK_CONST_METHOD1(SetupDirectoryForFUSEAccess,
                     bool(const base::FilePath& dirpath));

 private:
  bool ForwardSetupDirectoryForFUSEAccessToImpl(const base::FilePath& path) {
    return DrivefsHelper::SetupDirectoryForFUSEAccess(path);
  }
};

class DrivefsHelperTest : public ::testing::Test {
 public:
  DrivefsHelperTest() : helper_(&platform_) {}

 protected:
  bool SetupDirectoryForFUSEAccess(const std::string& dir) {
    return helper_.SetupDirectoryForFUSEAccess(base::FilePath(dir));
  }

  MockPlatform platform_;
  TestDrivefsHelper helper_;
};

TEST_F(DrivefsHelperTest, CreateMounter) {
  EXPECT_CALL(helper_, SetupDirectoryForFUSEAccess(base::FilePath("/foo/bar")))
      .WillOnce(Return(true));

  auto mounter = helper_.CreateMounter(
      base::FilePath("/tmp/working_dir"), Uri::Parse("drivefs://id"),
      base::FilePath("/media/fuse/drivefs/id"),
      {"rw", "datadir=/foo//bar/./", "datadir=/ignored/second/datadir/value"});
  ASSERT_TRUE(mounter);

  EXPECT_EQ("drivefs", mounter->filesystem_type());
  EXPECT_TRUE(mounter->source().empty());
  EXPECT_EQ("/media/fuse/drivefs/id", mounter->target_path().value());
  auto options_string = mounter->mount_options().ToString();
  EXPECT_THAT(options_string, HasSubstr("datadir=/foo/bar"));
  EXPECT_THAT(options_string, HasSubstr("identity=id"));
  EXPECT_THAT(options_string, HasSubstr("rw"));
  EXPECT_THAT(options_string, HasSubstr("uid=700"));
  EXPECT_THAT(options_string, HasSubstr("gid=1501"));
}

TEST_F(DrivefsHelperTest, CreateMounter_CreateDataDir) {
  EXPECT_CALL(platform_, DirectoryExists("/foo//bar/")).WillOnce(Return(false));
  EXPECT_CALL(platform_, GetRealPath("/foo", _))
      .WillOnce(DoAll(SetArgPointee<1>("/foo"), Return(true)));
  EXPECT_CALL(helper_, SetupDirectoryForFUSEAccess(base::FilePath("/foo/bar")))
      .WillOnce(Return(true));

  auto mounter = helper_.CreateMounter(
      base::FilePath("/tmp/working_dir"), Uri::Parse("drivefs://id"),
      base::FilePath("/media/fuse/drivefs/id"),
      {"rw", "datadir=/foo//bar/", "datadir=/ignored/second/datadir/value"});
  ASSERT_TRUE(mounter);

  EXPECT_EQ("drivefs", mounter->filesystem_type());
  EXPECT_TRUE(mounter->source().empty());
  EXPECT_EQ("/media/fuse/drivefs/id", mounter->target_path().value());
  auto options_string = mounter->mount_options().ToString();
  EXPECT_THAT(options_string, HasSubstr("datadir=/foo/bar"));
  EXPECT_THAT(options_string, HasSubstr("identity=id"));
  EXPECT_THAT(options_string, HasSubstr("rw"));
  EXPECT_THAT(options_string, HasSubstr("uid=700"));
  EXPECT_THAT(options_string, HasSubstr("gid=1501"));
}

TEST_F(DrivefsHelperTest, CreateMounter_GetUserAndGroupIdFails) {
  EXPECT_CALL(platform_, GetUserAndGroupId(_, _, _)).WillOnce(Return(false));
  EXPECT_CALL(helper_, SetupDirectoryForFUSEAccess(base::FilePath("/foo/bar")))
      .Times(0);

  EXPECT_FALSE(helper_.CreateMounter(
      base::FilePath("/tmp/working_dir"), Uri::Parse("drivefs://id"),
      base::FilePath("/media/fuse/drivefs/id"), {"rw", "datadir=/foo/bar"}));
}

TEST_F(DrivefsHelperTest, CreateMounter_GetAndGroupIdFails) {
  EXPECT_CALL(platform_, GetGroupId(_, _)).WillOnce(Return(false));
  EXPECT_CALL(helper_, SetupDirectoryForFUSEAccess(base::FilePath("/foo/bar")))
      .Times(0);

  EXPECT_FALSE(helper_.CreateMounter(
      base::FilePath("/tmp/working_dir"), Uri::Parse("drivefs://id"),
      base::FilePath("/media/fuse/drivefs/id"), {"rw", "datadir=/foo/bar"}));
}

TEST_F(DrivefsHelperTest, CreateMounter_GetRealPathFails_DirectoryExists) {
  EXPECT_CALL(platform_, GetRealPath("/foo/bar", _)).WillOnce(Return(false));
  EXPECT_CALL(helper_, SetupDirectoryForFUSEAccess(_)).Times(0);

  EXPECT_FALSE(helper_.CreateMounter(
      base::FilePath("/tmp/working_dir"), Uri::Parse("drivefs://id"),
      base::FilePath("/media/fuse/drivefs/id"), {"rw", "datadir=/foo/bar"}));
}

TEST_F(DrivefsHelperTest, CreateMounter_GetRealPathFails_DirectoryDoesntExist) {
  EXPECT_CALL(platform_, DirectoryExists("/foo/bar")).WillOnce(Return(false));
  EXPECT_CALL(platform_, GetRealPath("/foo", _)).WillOnce(Return(false));
  EXPECT_CALL(platform_, GetGroupId(_, _)).Times(0);
  EXPECT_CALL(helper_, SetupDirectoryForFUSEAccess(_)).Times(0);

  EXPECT_FALSE(helper_.CreateMounter(
      base::FilePath("/tmp/working_dir"), Uri::Parse("drivefs://id"),
      base::FilePath("/media/fuse/drivefs/id"), {"rw", "datadir=/foo/bar"}));
}

TEST_F(DrivefsHelperTest, CreateMounter_InvalidPath) {
  EXPECT_CALL(helper_, SetupDirectoryForFUSEAccess(_)).Times(0);

  for (const auto* path : {"relative/path", "/foo/../bar", ".", ".."}) {
    EXPECT_FALSE(helper_.CreateMounter(base::FilePath("/tmp/working_dir"),
                                       Uri::Parse("drivefs://id"),
                                       base::FilePath("/media/fuse/drivefs/id"),
                                       {"rw", "datadir=" + std::string(path)}));
  }
}

TEST_F(DrivefsHelperTest, CreateMounter_NoDatadir) {
  EXPECT_CALL(helper_, SetupDirectoryForFUSEAccess(_)).Times(0);

  EXPECT_FALSE(helper_.CreateMounter(
      base::FilePath("/tmp/working_dir"), Uri::Parse("drivefs://id"),
      base::FilePath("/media/fuse/drivefs/id"), {"rw"}));
}

TEST_F(DrivefsHelperTest, CreateMounter_SetupDirectoryFails) {
  EXPECT_CALL(helper_, SetupDirectoryForFUSEAccess(base::FilePath("/foo/bar")))
      .WillOnce(Return(false));

  EXPECT_FALSE(helper_.CreateMounter(
      base::FilePath("/tmp/working_dir"), Uri::Parse("drivefs://id"),
      base::FilePath("/media/fuse/drivefs/id"), {"rw", "datadir=/foo/bar"}));
}

// Verifies that SetupDirectoryForFUSEAccess crashes if path is unsafe.
TEST_F(DrivefsHelperTest, SetupDirectoryForFUSEAccess_UnsafePath) {
  EXPECT_DEATH(SetupDirectoryForFUSEAccess("foo"), ".*");
  EXPECT_DEATH(SetupDirectoryForFUSEAccess("../foo"), ".*");
  EXPECT_DEATH(SetupDirectoryForFUSEAccess("/bar/../foo"), ".*");
  EXPECT_DEATH(SetupDirectoryForFUSEAccess("/../foo"), ".*");
}

// Verifies that SetupDirectoryForFUSEAccess creates directory with
// correct access if there was no directory initially.
TEST_F(DrivefsHelperTest, SetupDirectoryForFUSEAccess_NoDir) {
  EXPECT_CALL(platform_, DirectoryExists(kSomeDirPath)).WillOnce(Return(false));
  EXPECT_CALL(platform_, CreateDirectory(kSomeDirPath)).WillOnce(Return(true));
  EXPECT_CALL(platform_, SetPermissions(kSomeDirPath, 0770))
      .WillOnce(Return(true));
  EXPECT_CALL(platform_, SetOwnership(kSomeDirPath, kMountUID, kFilesAccessGID))
      .WillOnce(Return(true));
  EXPECT_TRUE(SetupDirectoryForFUSEAccess(kSomeDirPath));
}

// Verifies that SetupDirectoryForFUSEAccess fails if there was no
// directory initially and can't create one.
TEST_F(DrivefsHelperTest, SetupDirectoryForFUSEAccess_NoDir_CantCreate) {
  EXPECT_CALL(platform_, DirectoryExists(kSomeDirPath)).WillOnce(Return(false));
  EXPECT_CALL(platform_, CreateDirectory(kSomeDirPath)).WillOnce(Return(false));
  EXPECT_CALL(platform_, SetPermissions(_, _)).Times(0);
  EXPECT_CALL(platform_, SetOwnership(_, _, _)).Times(0);
  EXPECT_FALSE(SetupDirectoryForFUSEAccess(kSomeDirPath));
}

// Verifies that SetupDirectoryForFUSEAccess fails if chmod fails.
TEST_F(DrivefsHelperTest, SetupDirectoryForFUSEAccess_NoDir_CantChmod) {
  EXPECT_CALL(platform_, DirectoryExists(kSomeDirPath)).WillOnce(Return(false));
  EXPECT_CALL(platform_, CreateDirectory(kSomeDirPath)).WillOnce(Return(true));
  EXPECT_CALL(platform_, SetPermissions(kSomeDirPath, 0770));
  EXPECT_FALSE(SetupDirectoryForFUSEAccess(kSomeDirPath));
}

// Verifies that SetupDirectoryForFUSEAccess fails if can't get attributes
// of an existing directory.
TEST_F(DrivefsHelperTest, SetupDirectoryForFUSEAccess_CantStat) {
  EXPECT_CALL(platform_, DirectoryExists(kSomeDirPath)).WillOnce(Return(true));
  EXPECT_CALL(platform_, GetOwnership(kSomeDirPath, _, _));
  EXPECT_CALL(platform_, SetOwnership(_, _, _)).Times(0);
  EXPECT_FALSE(SetupDirectoryForFUSEAccess(kSomeDirPath));
}

// Verifies that SetupDirectoryForFUSEAccess succeeds with shortcut
// if directory already has correct owner.
TEST_F(DrivefsHelperTest, SetupDirectoryForFUSEAccess_Owned) {
  EXPECT_CALL(platform_, DirectoryExists(kSomeDirPath)).WillOnce(Return(true));
  EXPECT_CALL(platform_, GetOwnership(kSomeDirPath, _, _))
      .WillOnce(DoAll(SetArgPointee<1>(kMountUID),
                      SetArgPointee<2>(kFilesAccessGID), Return(true)));
  EXPECT_CALL(platform_, SetOwnership(_, _, _)).Times(0);
  EXPECT_TRUE(SetupDirectoryForFUSEAccess(kSomeDirPath));
}

// Verifies that SetupDirectoryForFUSEAccess refuses to chown an existing dir if
// it can't delete it.
TEST_F(DrivefsHelperTest,
       SetupDirectoryForFUSEAccess_AlreadyExistsWithWrongOwnerAndCantDelete) {
  EXPECT_CALL(platform_, DirectoryExists(kSomeDirPath)).WillOnce(Return(true));
  EXPECT_CALL(platform_, GetOwnership(kSomeDirPath, _, _))
      .WillOnce(DoAll(SetArgPointee<1>(kFilesUID),
                      SetArgPointee<2>(kFilesAccessGID), Return(true)));
  EXPECT_CALL(platform_, RemoveEmptyDirectory(kSomeDirPath))
      .WillOnce(Return(false));
  EXPECT_CALL(platform_, SetOwnership(_, _, _)).Times(0);

  EXPECT_FALSE(SetupDirectoryForFUSEAccess(kSomeDirPath));
}

// Verifies that SetupDirectoryForFUSEAccess deletes an empty existing dir with
// incorrent owners and then performs the usual no directory exists actions.
TEST_F(DrivefsHelperTest,
       SetupDirectoryForFUSEAccess_AlreadyExistsWithWrongOwner) {
  EXPECT_CALL(platform_, DirectoryExists(kSomeDirPath)).WillOnce(Return(true));
  EXPECT_CALL(platform_, GetOwnership(kSomeDirPath, _, _))
      .WillOnce(DoAll(SetArgPointee<1>(kFilesUID),
                      SetArgPointee<2>(kFilesAccessGID), Return(true)));
  EXPECT_CALL(platform_, RemoveEmptyDirectory(kSomeDirPath))
      .WillOnce(Return(true));
  EXPECT_CALL(platform_, CreateDirectory(kSomeDirPath)).WillOnce(Return(true));
  EXPECT_CALL(platform_, SetPermissions(kSomeDirPath, 0770))
      .WillOnce(Return(true));
  EXPECT_CALL(platform_, SetOwnership(kSomeDirPath, kMountUID, kFilesAccessGID))
      .WillOnce(Return(true));
  EXPECT_TRUE(SetupDirectoryForFUSEAccess(kSomeDirPath));
}

}  // namespace
}  // namespace cros_disks
