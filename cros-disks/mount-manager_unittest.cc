// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cros-disks/mount-manager.h"

#include <sys/mount.h>
#include <sys/unistd.h>

#include <string>
#include <vector>

#include <base/memory/scoped_temp_dir.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "cros-disks/platform.h"

using std::string;
using std::vector;
using testing::Return;
using testing::_;

namespace cros_disks {

static const char kMountRootDirectory[] = "/test";

// A mock platform class for testing the mount manager base class.
class MockPlatform : public Platform {
 public:
  MockPlatform() {
  }

  MOCK_CONST_METHOD1(CreateDirectory, bool(const string& path));
  MOCK_CONST_METHOD1(CreateOrReuseEmptyDirectory, bool(const string& path));
  MOCK_CONST_METHOD2(CreateOrReuseEmptyDirectoryWithFallback,
                     bool(string* path, unsigned max_suffix_to_retry));
  MOCK_CONST_METHOD2(GetGroupId,
                     bool(const string& group_name, gid_t* group_id));
  MOCK_CONST_METHOD1(RemoveEmptyDirectory, bool(const string& path));
  MOCK_CONST_METHOD3(SetOwnership, bool(const string& path,
                                        uid_t user_id, gid_t group_id));
  MOCK_CONST_METHOD2(SetPermissions, bool(const string& path, mode_t mode));
};

// A mock mount manager class for testing the mount manager base class.
class MountManagerUnderTest : public MountManager {
 public:
  explicit MountManagerUnderTest(Platform* platform)
      : MountManager(kMountRootDirectory, platform) {
  }

  MOCK_CONST_METHOD1(CanMount, bool(const string& source_path));
  MOCK_CONST_METHOD0(GetMountSourceType, MountSourceType());
  MOCK_METHOD4(DoMount, MountErrorType(const string& source_path,
                                       const string& filesystem_type,
                                       const vector<string>& options,
                                       const string& mount_path));
  MOCK_METHOD2(DoUnmount, MountErrorType(const string& path,
                                         const vector<string>& options));
  MOCK_CONST_METHOD1(SuggestMountPath, string(const string& source_path));
};

class MountManagerTest : public ::testing::Test {
 public:
  MountManagerTest()
      : manager_(&platform_) {
  }

 protected:
  MockPlatform platform_;
  MountManagerUnderTest manager_;
  string filesystem_type_;
  string mount_path_;
  string source_path_;
  vector<string> options_;
};

TEST_F(MountManagerTest, InitializeFailedInCreateDirectory) {
  EXPECT_CALL(platform_, CreateDirectory(kMountRootDirectory))
      .WillOnce(Return(false));
  EXPECT_CALL(platform_,
              SetOwnership(kMountRootDirectory, getuid(), getgid()))
      .Times(0);
  EXPECT_CALL(platform_, SetPermissions(kMountRootDirectory, _))
      .Times(0);

  EXPECT_FALSE(manager_.Initialize());
}

TEST_F(MountManagerTest, InitializeFailedInSetOwnership) {
  EXPECT_CALL(platform_, CreateDirectory(kMountRootDirectory))
      .WillOnce(Return(true));
  EXPECT_CALL(platform_,
              SetOwnership(kMountRootDirectory, getuid(), getgid()))
      .WillOnce(Return(false));
  EXPECT_CALL(platform_, SetPermissions(kMountRootDirectory, _))
      .Times(0);

  EXPECT_FALSE(manager_.Initialize());
}

TEST_F(MountManagerTest, InitializeFailedInSetPermissions) {
  EXPECT_CALL(platform_, CreateDirectory(kMountRootDirectory))
      .WillOnce(Return(true));
  EXPECT_CALL(platform_,
              SetOwnership(kMountRootDirectory, getuid(), getgid()))
      .WillOnce(Return(true));
  EXPECT_CALL(platform_, SetPermissions(kMountRootDirectory, _))
      .WillOnce(Return(false));

  EXPECT_FALSE(manager_.Initialize());
}

TEST_F(MountManagerTest, InitializeSucceeded) {
  EXPECT_CALL(platform_, CreateDirectory(kMountRootDirectory))
      .WillOnce(Return(true));
  EXPECT_CALL(platform_,
              SetOwnership(kMountRootDirectory, getuid(), getgid()))
      .WillOnce(Return(true));
  EXPECT_CALL(platform_, SetPermissions(kMountRootDirectory, _))
      .WillOnce(Return(true));

  EXPECT_TRUE(manager_.Initialize());
}

TEST_F(MountManagerTest, MountFailedWithEmptySourcePath) {
  EXPECT_CALL(platform_, CreateOrReuseEmptyDirectory(_)).Times(0);
  EXPECT_CALL(platform_, CreateOrReuseEmptyDirectoryWithFallback(_, _))
      .Times(0);
  EXPECT_CALL(platform_, RemoveEmptyDirectory(_)).Times(0);
  EXPECT_CALL(manager_, DoMount(_, _, _, _)).Times(0);
  EXPECT_CALL(manager_, DoUnmount(_, _)).Times(0);
  EXPECT_CALL(manager_, SuggestMountPath(_)).Times(0);

  EXPECT_EQ(kMountErrorInvalidArgument,
            manager_.Mount(source_path_, filesystem_type_, options_,
                           &mount_path_));
}

TEST_F(MountManagerTest, MountFailedWithNullMountPath) {
  source_path_ = "source";

  EXPECT_CALL(platform_, CreateOrReuseEmptyDirectory(_)).Times(0);
  EXPECT_CALL(platform_, CreateOrReuseEmptyDirectoryWithFallback(_, _))
      .Times(0);
  EXPECT_CALL(platform_, RemoveEmptyDirectory(_)).Times(0);
  EXPECT_CALL(manager_, DoMount(_, _, _, _)).Times(0);
  EXPECT_CALL(manager_, DoUnmount(_, _)).Times(0);
  EXPECT_CALL(manager_, SuggestMountPath(_)).Times(0);

  EXPECT_EQ(kMountErrorInvalidArgument,
            manager_.Mount(source_path_, filesystem_type_, options_, NULL));
}

TEST_F(MountManagerTest, MountFailedInCreateOrReuseEmptyDirectory) {
  source_path_ = "source";
  mount_path_ = "target";

  EXPECT_CALL(platform_, CreateOrReuseEmptyDirectory(mount_path_))
      .WillOnce(Return(false));
  EXPECT_CALL(platform_, CreateOrReuseEmptyDirectoryWithFallback(_, _))
      .Times(0);
  EXPECT_CALL(platform_, RemoveEmptyDirectory(_)).Times(0);
  EXPECT_CALL(manager_, DoMount(_, _, _, _)).Times(0);
  EXPECT_CALL(manager_, DoUnmount(_, _)).Times(0);
  EXPECT_CALL(manager_, SuggestMountPath(_)).Times(0);

  EXPECT_EQ(kMountErrorDirectoryCreationFailed,
            manager_.Mount(source_path_, filesystem_type_, options_,
                           &mount_path_));
  EXPECT_EQ("target", mount_path_);
  EXPECT_FALSE(manager_.IsMountPathInCache(mount_path_));
}

TEST_F(MountManagerTest, MountFailedInCreateOrReuseEmptyDirectoryWithFallback) {
  source_path_ = "source";
  string suggested_mount_path = "target";

  EXPECT_CALL(platform_, CreateOrReuseEmptyDirectory(_)).Times(0);
  EXPECT_CALL(platform_, CreateOrReuseEmptyDirectoryWithFallback(_, _))
      .WillOnce(Return(false));
  EXPECT_CALL(platform_, RemoveEmptyDirectory(_)).Times(0);
  EXPECT_CALL(manager_, DoMount(_, _, _, _)).Times(0);
  EXPECT_CALL(manager_, DoUnmount(_, _)).Times(0);
  EXPECT_CALL(manager_, SuggestMountPath(source_path_))
      .WillOnce(Return(suggested_mount_path));

  EXPECT_EQ(kMountErrorDirectoryCreationFailed,
            manager_.Mount(source_path_, filesystem_type_, options_,
                           &mount_path_));
  EXPECT_EQ("", mount_path_);
  EXPECT_FALSE(manager_.IsMountPathInCache(suggested_mount_path));
}

TEST_F(MountManagerTest, MountFailedInSetOwnership) {
  source_path_ = "source";
  mount_path_ = "target";

  EXPECT_CALL(platform_, CreateOrReuseEmptyDirectory(mount_path_))
      .WillOnce(Return(true));
  EXPECT_CALL(platform_, CreateOrReuseEmptyDirectoryWithFallback(_, _))
      .Times(0);
  EXPECT_CALL(platform_, GetGroupId(_, _)).WillOnce(Return(true));
  EXPECT_CALL(platform_, SetOwnership(mount_path_, _, _))
      .WillOnce(Return(false));
  EXPECT_CALL(platform_, SetPermissions(_, _)).Times(0);
  EXPECT_CALL(platform_, RemoveEmptyDirectory(mount_path_))
      .WillOnce(Return(true));
  EXPECT_CALL(manager_, DoMount(_, _, _, _)).Times(0);
  EXPECT_CALL(manager_, DoUnmount(_, _)).Times(0);
  EXPECT_CALL(manager_, SuggestMountPath(_)).Times(0);

  EXPECT_EQ(kMountErrorDirectoryCreationFailed,
            manager_.Mount(source_path_, filesystem_type_, options_,
                           &mount_path_));
  EXPECT_EQ("target", mount_path_);
  EXPECT_FALSE(manager_.IsMountPathInCache(mount_path_));
}

TEST_F(MountManagerTest, MountFailedInSetPermissions) {
  source_path_ = "source";
  mount_path_ = "target";

  EXPECT_CALL(platform_, CreateOrReuseEmptyDirectory(mount_path_))
      .WillOnce(Return(true));
  EXPECT_CALL(platform_, CreateOrReuseEmptyDirectoryWithFallback(_, _))
      .Times(0);
  EXPECT_CALL(platform_, GetGroupId(_, _)).WillOnce(Return(true));
  EXPECT_CALL(platform_, SetOwnership(mount_path_, _, _))
      .WillOnce(Return(true));
  EXPECT_CALL(platform_, SetPermissions(mount_path_, _))
      .WillOnce(Return(false));
  EXPECT_CALL(platform_, RemoveEmptyDirectory(mount_path_))
      .WillOnce(Return(true));
  EXPECT_CALL(manager_, DoMount(_, _, _, _)).Times(0);
  EXPECT_CALL(manager_, DoUnmount(_, _)).Times(0);
  EXPECT_CALL(manager_, SuggestMountPath(_)).Times(0);

  EXPECT_EQ(kMountErrorDirectoryCreationFailed,
            manager_.Mount(source_path_, filesystem_type_, options_,
                           &mount_path_));
  EXPECT_EQ("target", mount_path_);
  EXPECT_FALSE(manager_.IsMountPathInCache(mount_path_));
}

TEST_F(MountManagerTest, MountSucceededWithGivenMountPath) {
  source_path_ = "source";
  mount_path_ = "target";

  EXPECT_CALL(platform_, CreateOrReuseEmptyDirectory(mount_path_))
      .WillOnce(Return(true));
  EXPECT_CALL(platform_, CreateOrReuseEmptyDirectoryWithFallback(_, _))
      .Times(0);
  EXPECT_CALL(platform_, GetGroupId(_, _)).WillOnce(Return(true));
  EXPECT_CALL(platform_, SetOwnership(mount_path_, _, _))
      .WillOnce(Return(true));
  EXPECT_CALL(platform_, SetPermissions(mount_path_, _))
      .WillOnce(Return(true));
  EXPECT_CALL(platform_, RemoveEmptyDirectory(mount_path_))
      .WillOnce(Return(true));
  EXPECT_CALL(manager_, DoMount(source_path_, filesystem_type_, options_,
                                mount_path_))
      .WillOnce(Return(kMountErrorNone));
  EXPECT_CALL(manager_, DoUnmount(mount_path_, _))
      .WillOnce(Return(kMountErrorNone));
  EXPECT_CALL(manager_, SuggestMountPath(_)).Times(0);

  EXPECT_EQ(kMountErrorNone,
            manager_.Mount(source_path_, filesystem_type_, options_,
                           &mount_path_));
  EXPECT_EQ("target", mount_path_);
  EXPECT_TRUE(manager_.IsMountPathInCache(mount_path_));
  EXPECT_TRUE(manager_.UnmountAll());
}

TEST_F(MountManagerTest, MountSucceededWithEmptyMountPath) {
  source_path_ = "source";
  string suggested_mount_path = "target";

  EXPECT_CALL(platform_, CreateOrReuseEmptyDirectory(_)).Times(0);
  EXPECT_CALL(platform_, CreateOrReuseEmptyDirectoryWithFallback(_, _))
      .WillOnce(Return(true));
  EXPECT_CALL(platform_, GetGroupId(_, _)).WillOnce(Return(true));
  EXPECT_CALL(platform_, SetOwnership(suggested_mount_path, _, _))
      .WillOnce(Return(true));
  EXPECT_CALL(platform_, SetPermissions(suggested_mount_path, _))
      .WillOnce(Return(true));
  EXPECT_CALL(platform_, RemoveEmptyDirectory(suggested_mount_path))
      .WillOnce(Return(true));
  EXPECT_CALL(manager_, DoMount(source_path_, filesystem_type_, options_,
                                suggested_mount_path))
      .WillOnce(Return(kMountErrorNone));
  EXPECT_CALL(manager_, DoUnmount(suggested_mount_path, _))
      .WillOnce(Return(kMountErrorNone));
  EXPECT_CALL(manager_, SuggestMountPath(source_path_))
      .WillOnce(Return(suggested_mount_path));

  EXPECT_EQ(kMountErrorNone,
            manager_.Mount(source_path_, filesystem_type_, options_,
                           &mount_path_));
  EXPECT_EQ(suggested_mount_path, mount_path_);
  EXPECT_TRUE(manager_.IsMountPathInCache(mount_path_));
  EXPECT_TRUE(manager_.UnmountAll());
}

TEST_F(MountManagerTest, MountWithAlreadyMountedSourcePath) {
  source_path_ = "source";
  string suggested_mount_path = "target";

  EXPECT_CALL(platform_, CreateOrReuseEmptyDirectory(_)).Times(0);
  EXPECT_CALL(platform_, CreateOrReuseEmptyDirectoryWithFallback(_, _))
      .WillOnce(Return(true));
  EXPECT_CALL(platform_, GetGroupId(_, _)).WillOnce(Return(true));
  EXPECT_CALL(platform_, SetOwnership(suggested_mount_path, _, _))
      .WillOnce(Return(true));
  EXPECT_CALL(platform_, SetPermissions(suggested_mount_path, _))
      .WillOnce(Return(true));
  EXPECT_CALL(platform_, RemoveEmptyDirectory(suggested_mount_path))
      .WillOnce(Return(true));
  EXPECT_CALL(manager_, DoMount(source_path_, filesystem_type_, options_,
                                suggested_mount_path))
      .WillOnce(Return(kMountErrorNone));
  EXPECT_CALL(manager_, DoUnmount(suggested_mount_path, _))
      .WillOnce(Return(kMountErrorNone));
  EXPECT_CALL(manager_, SuggestMountPath(source_path_))
      .WillOnce(Return(suggested_mount_path));

  EXPECT_EQ(kMountErrorNone,
            manager_.Mount(source_path_, filesystem_type_, options_,
                           &mount_path_));
  EXPECT_EQ(suggested_mount_path, mount_path_);
  EXPECT_TRUE(manager_.IsMountPathInCache(mount_path_));

  // Mount an already-mounted source path without specifying a mount path
  mount_path_.clear();
  EXPECT_EQ(kMountErrorNone,
            manager_.Mount(source_path_, filesystem_type_, options_,
                           &mount_path_));
  EXPECT_EQ(suggested_mount_path, mount_path_);
  EXPECT_TRUE(manager_.IsMountPathInCache(mount_path_));

  // Mount an already-mounted source path to the same mount path
  mount_path_ = suggested_mount_path;
  EXPECT_EQ(kMountErrorNone,
            manager_.Mount(source_path_, filesystem_type_, options_,
                           &mount_path_));
  EXPECT_EQ(suggested_mount_path, mount_path_);
  EXPECT_TRUE(manager_.IsMountPathInCache(mount_path_));

  // Mount an already-mounted source path to a different mount path
  mount_path_ = "another-path";
  EXPECT_EQ(kMountErrorPathAlreadyMounted,
            manager_.Mount(source_path_, filesystem_type_, options_,
                           &mount_path_));
  EXPECT_FALSE(manager_.IsMountPathInCache(mount_path_));
  EXPECT_TRUE(manager_.IsMountPathInCache(suggested_mount_path));

  EXPECT_TRUE(manager_.UnmountAll());
}

TEST_F(MountManagerTest, UnmountFailedWithEmptyPath) {
  EXPECT_CALL(platform_, CreateOrReuseEmptyDirectory(_)).Times(0);
  EXPECT_CALL(platform_, CreateOrReuseEmptyDirectoryWithFallback(_, _))
      .Times(0);
  EXPECT_CALL(platform_, RemoveEmptyDirectory(_)).Times(0);
  EXPECT_CALL(manager_, DoMount(_, _, _, _)).Times(0);
  EXPECT_CALL(manager_, DoUnmount(_, _)).Times(0);
  EXPECT_CALL(manager_, SuggestMountPath(_)).Times(0);

  EXPECT_EQ(kMountErrorInvalidArgument,
            manager_.Unmount(mount_path_, options_));
}

TEST_F(MountManagerTest, UnmountFailedWithPathNotMounted) {
  mount_path_ = "nonexistent-path";

  EXPECT_CALL(platform_, CreateOrReuseEmptyDirectory(_)).Times(0);
  EXPECT_CALL(platform_, CreateOrReuseEmptyDirectoryWithFallback(_, _))
      .Times(0);
  EXPECT_CALL(platform_, RemoveEmptyDirectory(_)).Times(0);
  EXPECT_CALL(manager_, DoMount(_, _, _, _)).Times(0);
  EXPECT_CALL(manager_, DoUnmount(_, _)).Times(0);
  EXPECT_CALL(manager_, SuggestMountPath(_)).Times(0);

  EXPECT_EQ(kMountErrorPathNotMounted,
            manager_.Unmount(mount_path_, options_));
}

TEST_F(MountManagerTest, UnmountSucceededWithGivenSourcePath) {
  source_path_ = "source";
  mount_path_ = "target";

  EXPECT_CALL(platform_, CreateOrReuseEmptyDirectory(mount_path_))
      .WillOnce(Return(true));
  EXPECT_CALL(platform_, CreateOrReuseEmptyDirectoryWithFallback(_, _))
      .Times(0);
  EXPECT_CALL(platform_, GetGroupId(_, _)).WillOnce(Return(true));
  EXPECT_CALL(platform_, SetOwnership(mount_path_, _, _))
      .WillOnce(Return(true));
  EXPECT_CALL(platform_, SetPermissions(mount_path_, _))
      .WillOnce(Return(true));
  EXPECT_CALL(platform_, RemoveEmptyDirectory(_))
      .WillOnce(Return(true));
  EXPECT_CALL(manager_, DoMount(source_path_, filesystem_type_, options_,
                                mount_path_))
      .WillOnce(Return(kMountErrorNone));
  EXPECT_CALL(manager_, DoUnmount(mount_path_, _))
      .WillOnce(Return(kMountErrorNone));
  EXPECT_CALL(manager_, SuggestMountPath(_)).Times(0);

  EXPECT_EQ(kMountErrorNone,
            manager_.Mount(source_path_, filesystem_type_, options_,
                           &mount_path_));
  EXPECT_EQ("target", mount_path_);
  EXPECT_TRUE(manager_.IsMountPathInCache(mount_path_));

  EXPECT_EQ(kMountErrorNone, manager_.Unmount(source_path_, options_));
  EXPECT_FALSE(manager_.IsMountPathInCache(mount_path_));
}

TEST_F(MountManagerTest, UnmountSucceededWithGivenMountPath) {
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  string actual_mount_path = temp_dir.path().value();

  source_path_ = "source";
  mount_path_ = actual_mount_path;

  EXPECT_CALL(platform_, CreateOrReuseEmptyDirectory(mount_path_))
      .WillOnce(Return(true));
  EXPECT_CALL(platform_, CreateOrReuseEmptyDirectoryWithFallback(_, _))
      .Times(0);
  EXPECT_CALL(platform_, GetGroupId(_, _)).WillOnce(Return(true));
  EXPECT_CALL(platform_, SetOwnership(mount_path_, _, _))
      .WillOnce(Return(true));
  EXPECT_CALL(platform_, SetPermissions(mount_path_, _))
      .WillOnce(Return(true));
  EXPECT_CALL(platform_, RemoveEmptyDirectory(_))
      .WillOnce(Return(true));
  EXPECT_CALL(manager_, DoMount(source_path_, filesystem_type_, options_,
                                mount_path_))
      .WillOnce(Return(kMountErrorNone));
  EXPECT_CALL(manager_, DoUnmount(mount_path_, _))
      .WillOnce(Return(kMountErrorNone));
  EXPECT_CALL(manager_, SuggestMountPath(_)).Times(0);

  EXPECT_EQ(kMountErrorNone,
            manager_.Mount(source_path_, filesystem_type_, options_,
                           &mount_path_));
  EXPECT_EQ(actual_mount_path, mount_path_);
  EXPECT_TRUE(manager_.IsMountPathInCache(mount_path_));

  EXPECT_EQ(kMountErrorNone, manager_.Unmount(mount_path_, options_));
  EXPECT_FALSE(manager_.IsMountPathInCache(mount_path_));
}

TEST_F(MountManagerTest, AddMountPathFromCache) {
  string result;
  source_path_ = "source";
  mount_path_ = "target";

  EXPECT_TRUE(manager_.AddMountPathToCache(source_path_, mount_path_));
  EXPECT_TRUE(manager_.GetMountPathFromCache(source_path_, &result));
  EXPECT_EQ(mount_path_, result);

  EXPECT_FALSE(manager_.AddMountPathToCache(source_path_, "target1"));
  EXPECT_TRUE(manager_.GetMountPathFromCache(source_path_, &result));
  EXPECT_EQ(mount_path_, result);

  EXPECT_TRUE(manager_.RemoveMountPathFromCache(mount_path_));
}

TEST_F(MountManagerTest, GetMountPathFromCache) {
  string result;
  source_path_ = "source";
  mount_path_ = "target";

  EXPECT_FALSE(manager_.GetMountPathFromCache(source_path_, &result));
  EXPECT_TRUE(manager_.AddMountPathToCache(source_path_, mount_path_));
  EXPECT_TRUE(manager_.GetMountPathFromCache(source_path_, &result));
  EXPECT_EQ(mount_path_, result);
  EXPECT_TRUE(manager_.RemoveMountPathFromCache(mount_path_));
  EXPECT_FALSE(manager_.GetMountPathFromCache(source_path_, &result));
}

TEST_F(MountManagerTest, IsMountPathInCache) {
  source_path_ = "source";
  mount_path_ = "target";

  EXPECT_FALSE(manager_.IsMountPathInCache(mount_path_));
  EXPECT_TRUE(manager_.AddMountPathToCache(source_path_, mount_path_));
  EXPECT_TRUE(manager_.IsMountPathInCache(mount_path_));
  EXPECT_TRUE(manager_.RemoveMountPathFromCache(mount_path_));
  EXPECT_FALSE(manager_.IsMountPathInCache(mount_path_));
}

TEST_F(MountManagerTest, RemoveMountPathFromCache) {
  source_path_ = "source";
  mount_path_ = "target";

  EXPECT_FALSE(manager_.RemoveMountPathFromCache(mount_path_));
  EXPECT_TRUE(manager_.AddMountPathToCache(source_path_, mount_path_));
  EXPECT_TRUE(manager_.RemoveMountPathFromCache(mount_path_));
  EXPECT_FALSE(manager_.RemoveMountPathFromCache(mount_path_));
}

TEST_F(MountManagerTest, ExtractSupportedUnmountOptions) {
  int unmount_flags = 0;
  int expected_unmount_flags = MNT_FORCE;
  options_.push_back("force");
  EXPECT_TRUE(manager_.ExtractUnmountOptions(options_, &unmount_flags));
  EXPECT_EQ(expected_unmount_flags, unmount_flags);
}

TEST_F(MountManagerTest, ExtractUnsupportedUnmountOptions) {
  int unmount_flags = 0;
  options_.push_back("foo");
  EXPECT_FALSE(manager_.ExtractUnmountOptions(options_, &unmount_flags));
  EXPECT_EQ(0, unmount_flags);
}

TEST_F(MountManagerTest, IsPathImmediateChildOfParent) {
  EXPECT_TRUE(manager_.IsPathImmediateChildOfParent(
      "/media/archive/test.zip", "/media/archive"));
  EXPECT_TRUE(manager_.IsPathImmediateChildOfParent(
      "/media/archive/test.zip/", "/media/archive"));
  EXPECT_TRUE(manager_.IsPathImmediateChildOfParent(
      "/media/archive/test.zip", "/media/archive/"));
  EXPECT_TRUE(manager_.IsPathImmediateChildOfParent(
      "/media/archive/test.zip/", "/media/archive/"));
  EXPECT_FALSE(manager_.IsPathImmediateChildOfParent(
      "/media/archive/test.zip/doc.zip", "/media/archive/"));
  EXPECT_FALSE(manager_.IsPathImmediateChildOfParent(
      "/media/archive/test.zip", "/media/removable"));
  EXPECT_FALSE(manager_.IsPathImmediateChildOfParent(
      "/tmp/archive/test.zip", "/media/removable"));
  EXPECT_FALSE(manager_.IsPathImmediateChildOfParent(
      "/media", "/media/removable"));
}

}  // namespace cros_disks
