// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cros-disks/archive-manager.h"

#include <gtest/gtest.h>

#include "cros-disks/platform.h"

using std::string;
using std::vector;

namespace {

const char kMountRootDirectory[] = "/media/archive";

}  // namespace

namespace cros_disks {

class ArchiveManagerTest : public ::testing::Test {
 public:
  ArchiveManagerTest()
      : manager_(kMountRootDirectory, &platform_) {
  }

 protected:
  Platform platform_;
  ArchiveManager manager_;
};

TEST_F(ArchiveManagerTest, CanMount) {
  EXPECT_FALSE(manager_.CanMount("/dev/sda1"));
  EXPECT_FALSE(manager_.CanMount("/devices/block/sda/sda1"));
  EXPECT_FALSE(manager_.CanMount("/sys/devices/block/sda/sda1"));
  EXPECT_FALSE(manager_.CanMount("/media/removable/disk1"));
  EXPECT_FALSE(manager_.CanMount("/media/removable/disk1/"));
  EXPECT_FALSE(manager_.CanMount("/media/removable/disk 1"));
  EXPECT_FALSE(manager_.CanMount("/media/archive/test.zip"));
  EXPECT_FALSE(manager_.CanMount("/media/archive/test.zip/"));
  EXPECT_FALSE(manager_.CanMount("/media/archive/test 1.zip"));
  EXPECT_TRUE(manager_.CanMount("/media/removable/disk1/test.zip"));
  EXPECT_TRUE(manager_.CanMount("/media/removable/disk1/test 1.zip"));
  EXPECT_TRUE(manager_.CanMount("/media/removable/disk1/dir1/test.zip"));
  EXPECT_TRUE(manager_.CanMount("/media/removable/test.zip/test1.zip"));
  EXPECT_TRUE(manager_.CanMount("/home/chronos/user/Downloads/test1.zip"));
  EXPECT_FALSE(manager_.CanMount(""));
  EXPECT_FALSE(manager_.CanMount("/tmp"));
  EXPECT_FALSE(manager_.CanMount("/media/removable"));
  EXPECT_FALSE(manager_.CanMount("/media/removable/"));
  EXPECT_FALSE(manager_.CanMount("/media/archive"));
  EXPECT_FALSE(manager_.CanMount("/media/archive/"));
  EXPECT_FALSE(manager_.CanMount("/home/chronos/user/Downloads"));
  EXPECT_FALSE(manager_.CanMount("/home/chronos/user/Downloads/"));
}

TEST_F(ArchiveManagerTest, CanUnmount) {
  EXPECT_FALSE(manager_.CanUnmount("/dev/sda1"));
  EXPECT_FALSE(manager_.CanUnmount("/devices/block/sda/sda1"));
  EXPECT_FALSE(manager_.CanUnmount("/sys/devices/block/sda/sda1"));
  EXPECT_FALSE(manager_.CanUnmount("/media/removable/disk1"));
  EXPECT_FALSE(manager_.CanUnmount("/media/removable/disk1/"));
  EXPECT_FALSE(manager_.CanUnmount("/media/removable/disk 1"));
  EXPECT_TRUE(manager_.CanUnmount("/media/archive/test.zip"));
  EXPECT_TRUE(manager_.CanUnmount("/media/archive/test.zip/"));
  EXPECT_TRUE(manager_.CanUnmount("/media/archive/test 1.zip"));
  EXPECT_TRUE(manager_.CanUnmount("/media/removable/disk1/test.zip"));
  EXPECT_TRUE(manager_.CanUnmount("/media/removable/disk1/test 1.zip"));
  EXPECT_TRUE(manager_.CanUnmount("/media/removable/disk1/dir1/test.zip"));
  EXPECT_TRUE(manager_.CanUnmount("/media/removable/test.zip/test1.zip"));
  EXPECT_TRUE(manager_.CanUnmount("/home/chronos/user/Downloads/test1.zip"));
  EXPECT_FALSE(manager_.CanUnmount(""));
  EXPECT_FALSE(manager_.CanUnmount("/tmp"));
  EXPECT_FALSE(manager_.CanUnmount("/media/removable"));
  EXPECT_FALSE(manager_.CanUnmount("/media/removable/"));
  EXPECT_FALSE(manager_.CanUnmount("/media/archive"));
  EXPECT_FALSE(manager_.CanUnmount("/media/archive/"));
  EXPECT_FALSE(manager_.CanUnmount("/home/chronos/user/Downloads"));
  EXPECT_FALSE(manager_.CanUnmount("/home/chronos/user/Downloads/"));
}

TEST_F(ArchiveManagerTest, DoMountFailedWithUnsupportedExtension) {
  string filesystem_type;
  string source_path = "/media/archive/test.zip/doc.zip";
  string mount_path = "/media/archive/doc.zip";
  vector<string> options;

  manager_.avfs_started_ = true;
  EXPECT_EQ(kMountErrorUnsupportedArchive,
            manager_.DoMount(source_path, filesystem_type, options,
                             mount_path));
}

TEST_F(ArchiveManagerTest, SuggestMountPath) {
  string expected_mount_path = string(kMountRootDirectory) + "/doc.zip";
  EXPECT_EQ(expected_mount_path,
            manager_.SuggestMountPath("/home/chronos/user/Downloads/doc.zip"));
  EXPECT_EQ(expected_mount_path,
            manager_.SuggestMountPath("/media/archive/test.zip/doc.zip"));
}

TEST_F(ArchiveManagerTest, RegisterFileExtension) {
  string extension = "zip";
  EXPECT_FALSE(manager_.IsFileExtensionSupported(extension));
  manager_.RegisterFileExtension(extension);
  EXPECT_TRUE(manager_.IsFileExtensionSupported(extension));
}

TEST_F(ArchiveManagerTest, GetAVFSPath) {
  EXPECT_EQ("",
            manager_.GetAVFSPath("/home/chronos/user/Downloads/doc.zip"));

  manager_.RegisterFileExtension("zip");
  EXPECT_EQ("", manager_.GetAVFSPath(""));
  EXPECT_EQ("", manager_.GetAVFSPath("test.zip"));
  EXPECT_EQ("", manager_.GetAVFSPath("/tmp/test.zip"));
  EXPECT_EQ("/var/run/avfsroot/user/doc.zip#",
            manager_.GetAVFSPath("/home/chronos/user/Downloads/doc.zip"));
  EXPECT_EQ("/var/run/avfsroot/media/archive/test.zip/doc.zip#",
            manager_.GetAVFSPath("/media/archive/test.zip/doc.zip"));
  EXPECT_EQ("/var/run/avfsroot/media/removable/disk1/test.zip#",
            manager_.GetAVFSPath("/media/removable/disk1/test.zip"));
  EXPECT_EQ("/var/run/avfsroot/media/removable/disk1/test.ZIP#",
            manager_.GetAVFSPath("/media/removable/disk1/test.ZIP"));
}

}  // namespace cros_disks
