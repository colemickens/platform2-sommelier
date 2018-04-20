// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cros-disks/fuse_helper.h"

#include <string>
#include <vector>

#include <base/strings/string_split.h>
#include <base/strings/string_util.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "cros-disks/fuse_mounter.h"
#include "cros-disks/mount_options.h"
#include "cros-disks/platform.h"
#include "cros-disks/uri.h"

using base::FilePath;
using std::string;
using std::vector;
using testing::DoAll;
using testing::Eq;
using testing::Return;
using testing::SetArgPointee;
using testing::StrEq;
using testing::_;

namespace cros_disks {

namespace {

const char kFUSEType[] = "fuse";
const char kMountProgram[] = "dummy";
const char kMountUser[] = "nobody";
const uid_t kMountUID = 200;
const gid_t kMountGID = 201;
const uid_t kFilesUID = 700;
const uid_t kFilesGID = 701;
const uid_t kFilesAccessGID = 1501;
const Uri kSomeUri("fuse", "some/src/path");
const FilePath kWorkingDir("/wkdir");
const FilePath kMountDir("/mnt");
const char kSomeDirPath[] = "/foo/bar";

// Mock Platform implementation for testing.
class MockPlatform : public Platform {
 public:
  MockPlatform() = default;

  bool GetUserAndGroupId(const string& user,
                         uid_t* user_id,
                         gid_t* group_id) const override {
    if (user == kMountUser) {
      if (user_id)
        *user_id = kMountUID;
      if (group_id)
        *group_id = kMountGID;
      return true;
    }
    if (user == FUSEHelper::kFilesUser) {
      if (user_id)
        *user_id = kFilesUID;
      if (group_id)
        *group_id = kFilesGID;
      return true;
    }
    return false;
  }

  bool GetGroupId(const string& group, gid_t* group_id) const override {
    if (group == FUSEHelper::kFilesGroup) {
      if (group_id)
        *group_id = kFilesAccessGID;
      return true;
    }
    return false;
  }

  MOCK_CONST_METHOD1(DirectoryExists, bool(const string& path));
  MOCK_CONST_METHOD1(IsDirectoryEmpty, bool(const string& path));
  MOCK_CONST_METHOD1(CreateDirectory, bool(const string& path));
  MOCK_CONST_METHOD3(SetOwnership,
                     bool(const string& path, uid_t user_id, gid_t group_id));
  MOCK_CONST_METHOD3(GetOwnership,
                     bool(const string& path, uid_t* user_id, gid_t* group_id));
  MOCK_CONST_METHOD2(SetPermissions, bool(const string& path, mode_t mode));
};

}  // namespace

class FUSEHelperTest : public ::testing::Test {
 public:
  FUSEHelperTest()
      : helper_(kFUSEType, &platform_, FilePath(kMountProgram), kMountUser) {
    ON_CALL(platform_, IsDirectoryEmpty(_)).WillByDefault(Return(true));
  }

 protected:
  bool SetupDirectoryForFUSEAccess(const string& dir) {
    return helper_.SetupDirectoryForFUSEAccess(FilePath(dir));
  }

  MockPlatform platform_;
  FUSEHelper helper_;
};

// Verifies that CanMount correctly identifies handleable URIs.
TEST_F(FUSEHelperTest, CanMount) {
  EXPECT_TRUE(helper_.CanMount(Uri::Parse("fuse://foo")));
  EXPECT_FALSE(helper_.CanMount(Uri::Parse("boose://foo")));
  EXPECT_FALSE(helper_.CanMount(Uri::Parse("http://foo")));
  EXPECT_FALSE(helper_.CanMount(Uri::Parse("fuse://")));
}

// Verifies that GetTargetSuffix escapes unwanted chars in URI.
TEST_F(FUSEHelperTest, GetTargetSuffix) {
  EXPECT_EQ("foo", helper_.GetTargetSuffix(Uri::Parse("fuse://foo")));
  EXPECT_EQ("", helper_.GetTargetSuffix(Uri::Parse("fuse://")));
  EXPECT_EQ("a:b@c:d$__$etc$",
            helper_.GetTargetSuffix(Uri::Parse("fuse://a:b@c:d/../etc/")));
}

// Verifies that generic implementation applies default rules to MountOptions.
TEST_F(FUSEHelperTest, PrepareMountOptions) {
  vector<string> options = {"bind", "foo=bar", "baz", "dirsync"};
  auto mounter =
      helper_.CreateMounter(kWorkingDir, kSomeUri, kMountDir, options);
  EXPECT_EQ(kFUSEType, mounter->filesystem_type());
  EXPECT_EQ(kSomeUri.path(), mounter->source_path());
  EXPECT_EQ(kMountDir.value(), mounter->target_path());
  string opts = mounter->mount_options().ToString();
  EXPECT_THAT(opts, testing::StartsWith("bind,dirsync,"));
  EXPECT_THAT(opts, testing::Not(testing::HasSubstr("uid=")));
}

// Verifies that SetupDirectoryForFUSEAccess crashes if path is unsafe.
TEST_F(FUSEHelperTest, SetupDirectoryForFUSEAccess_UnsafePath) {
  EXPECT_DEATH(SetupDirectoryForFUSEAccess("foo"), ".*");
  EXPECT_DEATH(SetupDirectoryForFUSEAccess("../foo"), ".*");
  EXPECT_DEATH(SetupDirectoryForFUSEAccess("/bar/../foo"), ".*");
  EXPECT_DEATH(SetupDirectoryForFUSEAccess("/../foo"), ".*");
}

// Verifies that SetupDirectoryForFUSEAccess creates directory with
// correct access if there was no directory initially.
TEST_F(FUSEHelperTest, SetupDirectoryForFUSEAccess_NoDir) {
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
TEST_F(FUSEHelperTest, SetupDirectoryForFUSEAccess_NoDir_CantCreate) {
  EXPECT_CALL(platform_, DirectoryExists(kSomeDirPath)).WillOnce(Return(false));
  EXPECT_CALL(platform_, CreateDirectory(kSomeDirPath)).WillOnce(Return(false));
  EXPECT_CALL(platform_, SetPermissions(_, _)).Times(0);
  EXPECT_CALL(platform_, SetOwnership(_, _, _)).Times(0);
  EXPECT_FALSE(SetupDirectoryForFUSEAccess(kSomeDirPath));
}

// Verifies that SetupDirectoryForFUSEAccess sucseeds to set correct
// access if there is no directory initially even if chmod fails.
TEST_F(FUSEHelperTest, SetupDirectoryForFUSEAccess_NoDir_CantChmod) {
  EXPECT_CALL(platform_, DirectoryExists(kSomeDirPath)).WillOnce(Return(false));
  EXPECT_CALL(platform_, CreateDirectory(kSomeDirPath)).WillOnce(Return(true));
  EXPECT_CALL(platform_, SetPermissions(kSomeDirPath, 0770));
  EXPECT_CALL(platform_, SetOwnership(kSomeDirPath, kMountUID, kFilesAccessGID))
      .WillOnce(Return(true));
  EXPECT_TRUE(SetupDirectoryForFUSEAccess(kSomeDirPath));
}

// Verifies that SetupDirectoryForFUSEAccess fails if can't get attributes
// of an existing directory.
TEST_F(FUSEHelperTest, SetupDirectoryForFUSEAccess_CantStat) {
  EXPECT_CALL(platform_, DirectoryExists(kSomeDirPath)).WillOnce(Return(true));
  EXPECT_CALL(platform_, GetOwnership(kSomeDirPath, _, _));
  EXPECT_CALL(platform_, SetOwnership(_, _, _)).Times(0);
  EXPECT_FALSE(SetupDirectoryForFUSEAccess(kSomeDirPath));
}

// Verifies that SetupDirectoryForFUSEAccess succeeds with shortcut
// if directory already has correct owner.
TEST_F(FUSEHelperTest, SetupDirectoryForFUSEAccess_Owned) {
  EXPECT_CALL(platform_, DirectoryExists(kSomeDirPath)).WillOnce(Return(true));
  EXPECT_CALL(platform_, GetOwnership(kSomeDirPath, _, _))
      .WillOnce(DoAll(SetArgPointee<1>(kMountUID),
                      SetArgPointee<2>(kFilesAccessGID), Return(true)));
  EXPECT_CALL(platform_, SetOwnership(_, _, _)).Times(0);
  EXPECT_TRUE(SetupDirectoryForFUSEAccess(kSomeDirPath));
}

// Verifies that SetupDirectoryForFUSEAccess refuses to chown a
// non-chronos-owned dir.
TEST_F(FUSEHelperTest, SetupDirectoryForFUSEAccess_NotChronos) {
  EXPECT_CALL(platform_, DirectoryExists(kSomeDirPath)).WillOnce(Return(true));
  EXPECT_CALL(platform_, GetOwnership(kSomeDirPath, _, _))
      .WillOnce(DoAll(SetArgPointee<1>(kFilesUID + 42),
                      SetArgPointee<2>(kFilesAccessGID), Return(true)));
  EXPECT_CALL(platform_, SetOwnership(_, _, _)).Times(0);
  EXPECT_FALSE(SetupDirectoryForFUSEAccess(kSomeDirPath));
}

// Verifies that SetupDirectoryForFUSEAccess successfully chowns a
// chronos-owned dir.
TEST_F(FUSEHelperTest, SetupDirectoryForFUSEAccess_Chronos) {
  EXPECT_CALL(platform_, DirectoryExists(kSomeDirPath)).WillOnce(Return(true));
  EXPECT_CALL(platform_, GetOwnership(kSomeDirPath, _, _))
      .WillOnce(DoAll(SetArgPointee<1>(kFilesUID),
                      SetArgPointee<2>(kFilesAccessGID), Return(true)));
  EXPECT_CALL(platform_, SetOwnership(kSomeDirPath, kMountUID, kFilesAccessGID))
      .WillOnce(Return(true));
  EXPECT_TRUE(SetupDirectoryForFUSEAccess(kSomeDirPath));
}

// Verifies that SetupDirectoryForFUSEAccess refuses to chown a
// chronos-owned dir when it has files inside.
TEST_F(FUSEHelperTest, SetupDirectoryForFUSEAccess_Chronos_NotEmpty) {
  EXPECT_CALL(platform_, DirectoryExists(kSomeDirPath)).WillOnce(Return(true));
  EXPECT_CALL(platform_, IsDirectoryEmpty(kSomeDirPath))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(platform_, GetOwnership(kSomeDirPath, _, _))
      .WillOnce(DoAll(SetArgPointee<1>(kFilesUID),
                      SetArgPointee<2>(kFilesAccessGID), Return(true)));
  EXPECT_CALL(platform_, SetOwnership(kSomeDirPath, kMountUID, kFilesAccessGID))
      .Times(0);
  EXPECT_FALSE(SetupDirectoryForFUSEAccess(kSomeDirPath));
}

// Verifies that SetupDirectoryForFUSEAccess fails if unable to chown dir.
TEST_F(FUSEHelperTest, SetupDirectoryForFUSEAccess_CantChown) {
  EXPECT_CALL(platform_, DirectoryExists(kSomeDirPath)).WillOnce(Return(true));
  EXPECT_CALL(platform_, GetOwnership(kSomeDirPath, _, _))
      .WillOnce(DoAll(SetArgPointee<1>(kFilesUID),
                      SetArgPointee<2>(kFilesAccessGID), Return(true)));
  EXPECT_CALL(platform_, SetOwnership(kSomeDirPath, kMountUID, kFilesAccessGID))
      .WillOnce(Return(false));
  EXPECT_FALSE(SetupDirectoryForFUSEAccess(kSomeDirPath));
}

}  // namespace cros_disks
