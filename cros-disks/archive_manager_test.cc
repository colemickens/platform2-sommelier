// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cros-disks/archive_manager.h"

#include <gtest/gtest.h>

#include "cros-disks/metrics.h"
#include "cros-disks/platform.h"

using std::string;
using std::vector;

namespace {

const char kMountRootDirectory[] = "/media/archive";

}  // namespace

namespace cros_disks {

class ArchiveManagerTest : public ::testing::Test {
 public:
  ArchiveManagerTest() : manager_(kMountRootDirectory, &platform_, &metrics_) {}

 protected:
  Metrics metrics_;
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
  EXPECT_FALSE(manager_.CanMount("/home/chronos/user/Downloads/test1.zip"));
  EXPECT_FALSE(manager_.CanMount("/home/chronos/user/GCache/test1.zip"));
  EXPECT_TRUE(
      manager_.CanMount("/home/chronos"
                        "/u-0123456789abcdef0123456789abcdef01234567"
                        "/Downloads/test1.zip"));
  EXPECT_TRUE(
      manager_.CanMount("/home/chronos"
                        "/u-0123456789abcdef0123456789abcdef01234567"
                        "/MyFiles/test1.zip"));
  EXPECT_TRUE(
      manager_.CanMount("/home/chronos"
                        "/u-0123456789abcdef0123456789abcdef01234567"
                        "/GCache/test1.zip"));
  EXPECT_FALSE(manager_.CanMount(""));
  EXPECT_FALSE(manager_.CanMount("/tmp"));
  EXPECT_FALSE(manager_.CanMount("/media/removable"));
  EXPECT_FALSE(manager_.CanMount("/media/removable/"));
  EXPECT_FALSE(manager_.CanMount("/media/archive"));
  EXPECT_FALSE(manager_.CanMount("/media/archive/"));
  EXPECT_FALSE(manager_.CanMount("/home/chronos/user/Downloads"));
  EXPECT_FALSE(manager_.CanMount("/home/chronos/user/Downloads/"));
  EXPECT_FALSE(manager_.CanMount("/home/chronos/user/GCache"));
  EXPECT_FALSE(manager_.CanMount("/home/chronos/user/GCache/"));
  EXPECT_FALSE(manager_.CanMount(
      "/home/chronos/u-0123456789abcdef0123456789abcdef01234567/Downloads"));
  EXPECT_FALSE(manager_.CanMount(
      "/home/chronos/u-0123456789abcdef0123456789abcdef01234567/Downloads/"));
  EXPECT_FALSE(manager_.CanMount(
      "/home/chronos/u-0123456789abcdef0123456789abcdef01234567/GCache"));
  EXPECT_FALSE(manager_.CanMount(
      "/home/chronos/u-0123456789abcdef0123456789abcdef01234567/GCache/"));
  EXPECT_FALSE(manager_.CanMount("/home/chronos/test1.zip"));
  EXPECT_FALSE(manager_.CanMount("/home/chronos/user/test1.zip"));
  EXPECT_FALSE(manager_.CanMount(
      "/home/chronos/u-0123456789abcdef0123456789abcdef01234567/test1.zip"));
  EXPECT_FALSE(manager_.CanMount("/home/chronos/Downloads/test1.zip"));
  EXPECT_FALSE(manager_.CanMount("/home/chronos/GCache/test1.zip"));
  EXPECT_FALSE(manager_.CanMount("/home/chronos/foo/Downloads/test1.zip"));
  EXPECT_FALSE(manager_.CanMount("/home/chronos/foo/GCache/test1.zip"));
  EXPECT_FALSE(manager_.CanMount("/home/chronos/u-/Downloads/test1.zip"));
  EXPECT_FALSE(
      manager_.CanMount("/home/chronos"
                        "/u-0123456789abcdef0123456789abcdef0123456"
                        "/Downloads/test1.zip"));
  EXPECT_FALSE(
      manager_.CanMount("/home/chronos"
                        "/u-xyz3456789abcdef0123456789abcdef01234567"
                        "/Downloads/test1.zip"));
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
  EXPECT_FALSE(manager_.CanUnmount("/home/chronos/user/Downloads/test1.zip"));
  EXPECT_FALSE(manager_.CanUnmount("/home/chronos/user/GCache/test1.zip"));
  EXPECT_TRUE(
      manager_.CanUnmount("/home/chronos"
                          "/u-0123456789abcdef0123456789abcdef01234567"
                          "/Downloads/test1.zip"));
  EXPECT_TRUE(
      manager_.CanUnmount("/home/chronos"
                          "/u-0123456789abcdef0123456789abcdef01234567"
                          "/GCache/test1.zip"));
  EXPECT_FALSE(manager_.CanUnmount(""));
  EXPECT_FALSE(manager_.CanUnmount("/tmp"));
  EXPECT_FALSE(manager_.CanUnmount("/media/removable"));
  EXPECT_FALSE(manager_.CanUnmount("/media/removable/"));
  EXPECT_FALSE(manager_.CanUnmount("/media/archive"));
  EXPECT_FALSE(manager_.CanUnmount("/media/archive/"));
  EXPECT_FALSE(manager_.CanUnmount("/home/chronos/user/Downloads"));
  EXPECT_FALSE(manager_.CanUnmount("/home/chronos/user/Downloads/"));
  EXPECT_FALSE(manager_.CanUnmount("/home/chronos/user/GCache"));
  EXPECT_FALSE(manager_.CanUnmount("/home/chronos/user/GCache/"));
  EXPECT_FALSE(manager_.CanUnmount(
      "/home/chronos/u-0123456789abcdef0123456789abcdef01234567/Downloads"));
  EXPECT_FALSE(manager_.CanUnmount(
      "/home/chronos/u-0123456789abcdef0123456789abcdef01234567/Downloads/"));
  EXPECT_FALSE(manager_.CanUnmount(
      "/home/chronos/u-0123456789abcdef0123456789abcdef01234567/GCache"));
  EXPECT_FALSE(manager_.CanUnmount(
      "/home/chronos/u-0123456789abcdef0123456789abcdef01234567/GCache/"));
  EXPECT_FALSE(manager_.CanUnmount("/home/chronos/test1.zip"));
  EXPECT_FALSE(manager_.CanUnmount("/home/chronos/user/test1.zip"));
  EXPECT_FALSE(manager_.CanUnmount(
      "/home/chronos/u-0123456789abcdef0123456789abcdef01234567/test1.zip"));
  EXPECT_FALSE(manager_.CanUnmount("/home/chronos/Downloads/test1.zip"));
  EXPECT_FALSE(manager_.CanUnmount("/home/chronos/GCache/test1.zip"));
  EXPECT_FALSE(manager_.CanUnmount("/home/chronos/foo/Downloads/test1.zip"));
  EXPECT_FALSE(manager_.CanUnmount("/home/chronos/foo/GCache/test1.zip"));
  EXPECT_FALSE(manager_.CanUnmount("/home/chronos/u-/Downloads/test1.zip"));
  EXPECT_FALSE(
      manager_.CanUnmount("/home/chronos"
                          "/u-0123456789abcdef0123456789abcdef0123456"
                          "/Downloads/test1.zip"));
  EXPECT_FALSE(
      manager_.CanUnmount("/home/chronos"
                          "/u-xyz3456789abcdef0123456789abcdef01234567"
                          "/Downloads/test1.zip"));
}

TEST_F(ArchiveManagerTest, DoMountFailedWithUnsupportedExtension) {
  string filesystem_type;
  string source_path = "/media/archive/test.zip/doc.zip";
  string mount_path = "/media/archive/doc.zip";
  vector<string> options;
  MountOptions applied_options;

  manager_.avfs_started_ = true;
  EXPECT_EQ(MOUNT_ERROR_UNSUPPORTED_ARCHIVE,
            manager_.DoMount(source_path, filesystem_type, options, mount_path,
                             &applied_options));
}

TEST_F(ArchiveManagerTest, SuggestMountPath) {
  string expected_mount_path = string(kMountRootDirectory) + "/doc.zip";
  EXPECT_EQ(expected_mount_path,
            manager_.SuggestMountPath("/home/chronos/user/Downloads/doc.zip"));
  EXPECT_EQ(expected_mount_path,
            manager_.SuggestMountPath("/media/archive/test.zip/doc.zip"));
}

TEST_F(ArchiveManagerTest, GetFileExtension) {
  EXPECT_EQ("", manager_.GetFileExtension(""));
  EXPECT_EQ("", manager_.GetFileExtension("test"));
  EXPECT_EQ("", manager_.GetFileExtension("/tmp/test"));
  EXPECT_EQ("zip", manager_.GetFileExtension(".zip"));
  EXPECT_EQ("zip", manager_.GetFileExtension(".Zip"));
  EXPECT_EQ("zip", manager_.GetFileExtension("test.zip"));
  EXPECT_EQ("zip", manager_.GetFileExtension("test.ZIP"));
  EXPECT_EQ("zip", manager_.GetFileExtension("/tmp/test.zip"));
  EXPECT_EQ("rar", manager_.GetFileExtension("/tmp/test.rar"));
  EXPECT_EQ("tar", manager_.GetFileExtension("/tmp/test.tar"));
  EXPECT_EQ("tar.gz", manager_.GetFileExtension("/tmp/test.tar.gz"));
  EXPECT_EQ("tgz", manager_.GetFileExtension("/tmp/test.tgz"));
  EXPECT_EQ("tar.bz2", manager_.GetFileExtension("/tmp/test.tar.bz2"));
  EXPECT_EQ("tbz", manager_.GetFileExtension("/tmp/test.tbz"));
  EXPECT_EQ("tbz2", manager_.GetFileExtension("/tmp/test.tbz2"));
}

TEST_F(ArchiveManagerTest, GetAVFSPath) {
  manager_.RegisterFileExtension("zip", "#uzip");

  EXPECT_EQ("/run/avfsroot/users/user/Downloads/a.zip#uzip",
            manager_.GetAVFSPath("/home/chronos/user/Downloads/a.zip", "zip"));
  EXPECT_EQ(
      "/run/avfsroot/users/u-0123456789abcdef0123456789abcdef01234567"
      "/Downloads/a.zip#uzip",
      manager_.GetAVFSPath(
          "/home/chronos/u-0123456789abcdef0123456789abcdef01234567"
          "/Downloads/a.zip",
          "zip"));
  EXPECT_EQ("/run/avfsroot/media/archive/test.zip/doc.zip#uzip",
            manager_.GetAVFSPath("/media/archive/test.zip/doc.zip", "zip"));
  EXPECT_EQ("/run/avfsroot/media/removable/disk1/test.zip#uzip",
            manager_.GetAVFSPath("/media/removable/disk1/test.zip", "zip"));
  EXPECT_EQ("/run/avfsroot/media/removable/disk1/test.ZIP#uzip",
            manager_.GetAVFSPath("/media/removable/disk1/test.ZIP", "zip"));
}

TEST_F(ArchiveManagerTest, GetAVFSPathWithInvalidPaths) {
  manager_.RegisterFileExtension("zip", "#uzip");

  EXPECT_EQ("", manager_.GetAVFSPath("", "zip"));
  EXPECT_EQ("", manager_.GetAVFSPath("test.zip", "zip"));
  EXPECT_EQ("", manager_.GetAVFSPath("/tmp/test.zip", "zip"));
}

TEST_F(ArchiveManagerTest, GetAVFSPathWithUnsupportedExtensions) {
  EXPECT_EQ("", manager_.GetAVFSPath("/home/chronos/user/Downloads/a.zip", ""));
  EXPECT_EQ("",
            manager_.GetAVFSPath("/home/chronos/user/Downloads/a.zip", "zip"));
}

TEST_F(ArchiveManagerTest, GetAVFSPathWithNestedArchives) {
  manager_.RegisterFileExtension("zip", "#uzip");

  EXPECT_EQ("/run/avfsroot/media/archive/l2.zip/l1.zip#uzip",
            manager_.GetAVFSPath("/media/archive/l2.zip/l1.zip", "zip"));

  // archive within an archive
  manager_.AddMountVirtualPath("/media/archive/l2.zip",
                               "/run/avfsroot/media/l2.zip#uzip");
  EXPECT_EQ("/run/avfsroot/media/l2.zip#uzip/l1.zip#uzip",
            manager_.GetAVFSPath("/media/archive/l2.zip/l1.zip", "zip"));
  EXPECT_EQ("/run/avfsroot/media/l2.zip#uzip/t/l1.zip#uzip",
            manager_.GetAVFSPath("/media/archive/l2.zip/t/l1.zip", "zip"));
  EXPECT_EQ("/run/avfsroot/media/l2.zip#uzip/t/doc/l1.zip#uzip",
            manager_.GetAVFSPath("/media/archive/l2.zip/t/doc/l1.zip", "zip"));

  // archive within an archive within an archive
  manager_.AddMountVirtualPath("/media/archive/l1.zip",
                               "/run/avfsroot/media/l2.zip#uzip/l1.zip#uzip");
  EXPECT_EQ("/run/avfsroot/media/l2.zip#uzip/l1.zip#uzip/l0.zip#uzip",
            manager_.GetAVFSPath("/media/archive/l1.zip/l0.zip", "zip"));
  EXPECT_EQ("/run/avfsroot/media/l2.zip#uzip/l1.zip#uzip/test/l0.zip#uzip",
            manager_.GetAVFSPath("/media/archive/l1.zip/test/l0.zip", "zip"));
  manager_.RemoveMountVirtualPath("/media/archive/l1.zip");

  manager_.AddMountVirtualPath(
      "/media/archive/l1.zip",
      "/run/avfsroot/media/l2.zip#uzip/test/l1.zip#uzip");
  EXPECT_EQ("/run/avfsroot/media/l2.zip#uzip/test/l1.zip#uzip/l0.zip#uzip",
            manager_.GetAVFSPath("/media/archive/l1.zip/l0.zip", "zip"));
  EXPECT_EQ("/run/avfsroot/media/l2.zip#uzip/test/l1.zip#uzip/test/l0.zip#uzip",
            manager_.GetAVFSPath("/media/archive/l1.zip/test/l0.zip", "zip"));
  manager_.RemoveMountVirtualPath("/media/archive/l1.zip");

  manager_.RemoveMountVirtualPath("/media/archive/l2.zip");
  EXPECT_EQ("/run/avfsroot/media/archive/l2.zip/l1.zip#uzip",
            manager_.GetAVFSPath("/media/archive/l2.zip/l1.zip", "zip"));
}

}  // namespace cros_disks
