// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cros-disks/disk-manager.h"

#include <sys/mount.h>

#include <base/memory/scoped_temp_dir.h>
#include <gtest/gtest.h>

#include "cros-disks/disk.h"
#include "cros-disks/filesystem.h"
#include "cros-disks/mounter.h"
#include "cros-disks/platform.h"

using std::map;
using std::string;
using std::vector;

namespace cros_disks {

static const char kMountRootDirectory[] = "/media/removable";

class DiskManagerTest : public ::testing::Test {
 public:
  DiskManagerTest()
      : manager_(kMountRootDirectory, &platform_) {
  }

 protected:
  Platform platform_;
  DiskManager manager_;
};

TEST_F(DiskManagerTest, CreateExternalMounter) {
  Disk disk;
  disk.set_device_file("/dev/sda1");

  Filesystem filesystem("ntfs");
  filesystem.set_mount_type("ntfs-3g");
  filesystem.set_requires_external_mounter(true);

  string target_path = "/media/disk";

  vector<string> options;
  options.push_back("rw");
  options.push_back("nodev");
  options.push_back("noexec");
  options.push_back("nosuid");

  Mounter *mounter =
      manager_.CreateMounter(disk, filesystem, target_path, options);
  EXPECT_EQ(filesystem.mount_type(), mounter->filesystem_type());
  EXPECT_EQ(disk.device_file(), mounter->source_path());
  EXPECT_EQ(target_path, mounter->target_path());
  EXPECT_EQ("nodev,noexec,nosuid,rw", mounter->mount_options().ToString());
}

TEST_F(DiskManagerTest, CreateSystemMounter) {
  Disk disk;
  disk.set_device_file("/dev/sda1");

  Filesystem filesystem("vfat");
  filesystem.AddExtraMountOption("utf8");
  filesystem.AddExtraMountOption("shortname=mixed");

  string target_path = "/media/disk";

  vector<string> options;
  options.push_back("rw");
  options.push_back("nodev");
  options.push_back("noexec");
  options.push_back("nosuid");

  Mounter *mounter =
      manager_.CreateMounter(disk, filesystem, target_path, options);
  EXPECT_EQ(filesystem.mount_type(), mounter->filesystem_type());
  EXPECT_EQ(disk.device_file(), mounter->source_path());
  EXPECT_EQ(target_path, mounter->target_path());
  EXPECT_EQ("nodev,noexec,nosuid,utf8,shortname=mixed,rw",
            mounter->mount_options().ToString());
}

TEST_F(DiskManagerTest, EnumerateDisks) {
  vector<Disk> disks = manager_.EnumerateDisks();
}

TEST_F(DiskManagerTest, GetDiskByDevicePath) {
  Disk disk;
  string device_path = "/dev/sda";
  EXPECT_TRUE(manager_.GetDiskByDevicePath(device_path, &disk));
  EXPECT_EQ(device_path, disk.device_file());

  device_path = "/dev/sda1";
  EXPECT_TRUE(manager_.GetDiskByDevicePath(device_path, &disk));
  EXPECT_EQ(device_path, disk.device_file());
}

TEST_F(DiskManagerTest, GetDiskByNonexistentDevicePath) {
  Disk disk;
  string device_path = "/dev/nonexistent-path";
  EXPECT_FALSE(manager_.GetDiskByDevicePath(device_path, &disk));
}

TEST_F(DiskManagerTest, GetFilesystem) {
  const Filesystem* null_pointer = NULL;

  EXPECT_EQ(null_pointer, manager_.GetFilesystem("nonexistent-fs"));

  Filesystem normal_fs("normal-fs");
  EXPECT_EQ(null_pointer, manager_.GetFilesystem(normal_fs.type()));
  manager_.RegisterFilesystem(normal_fs);
  platform_.set_experimental_features_enabled(false);
  EXPECT_NE(null_pointer, manager_.GetFilesystem(normal_fs.type()));
  platform_.set_experimental_features_enabled(true);
  EXPECT_NE(null_pointer, manager_.GetFilesystem(normal_fs.type()));

  Filesystem experimental_fs("experimental-fs");
  experimental_fs.set_is_experimental(true);
  EXPECT_EQ(null_pointer, manager_.GetFilesystem(experimental_fs.type()));
  manager_.RegisterFilesystem(experimental_fs);
  platform_.set_experimental_features_enabled(false);
  EXPECT_EQ(null_pointer, manager_.GetFilesystem(experimental_fs.type()));
  platform_.set_experimental_features_enabled(true);
  EXPECT_NE(null_pointer, manager_.GetFilesystem(experimental_fs.type()));
}

TEST_F(DiskManagerTest, GetFilesystemTypeOfNonexistentDevice) {
  string device_path = "/dev/nonexistent-path";
  string filesystem_type = manager_.GetFilesystemTypeOfDevice(device_path);
  EXPECT_EQ("", filesystem_type);
}

TEST_F(DiskManagerTest, RegisterFilesystem) {
  map<string, Filesystem>& filesystems = manager_.filesystems_;
  EXPECT_EQ(0, filesystems.size());
  EXPECT_TRUE(filesystems.find("nonexistent") == filesystems.end());

  Filesystem fat_fs("fat");
  fat_fs.set_accepts_user_and_group_id(true);
  manager_.RegisterFilesystem(fat_fs);
  EXPECT_EQ(1, filesystems.size());
  EXPECT_TRUE(filesystems.find(fat_fs.type()) != filesystems.end());

  Filesystem vfat_fs("vfat");
  vfat_fs.set_accepts_user_and_group_id(true);
  manager_.RegisterFilesystem(vfat_fs);
  EXPECT_EQ(2, filesystems.size());
  EXPECT_TRUE(filesystems.find(vfat_fs.type()) != filesystems.end());
}

TEST_F(DiskManagerTest, CanMount) {
  EXPECT_TRUE(manager_.CanMount("/dev/sda1"));
  EXPECT_TRUE(manager_.CanMount("/devices/block/sda/sda1"));
  EXPECT_TRUE(manager_.CanMount("/sys/devices/block/sda/sda1"));
  EXPECT_FALSE(manager_.CanMount("/media/removable/disk1"));
  EXPECT_FALSE(manager_.CanMount("/media/removable/disk1/"));
  EXPECT_FALSE(manager_.CanMount("/media/removable/disk 1"));
  EXPECT_FALSE(manager_.CanMount("/media/archive/test.zip"));
  EXPECT_FALSE(manager_.CanMount("/media/archive/test.zip/"));
  EXPECT_FALSE(manager_.CanMount("/media/archive/test 1.zip"));
  EXPECT_FALSE(manager_.CanMount("/media/removable/disk1/test.zip"));
  EXPECT_FALSE(manager_.CanMount("/media/removable/disk1/test 1.zip"));
  EXPECT_FALSE(manager_.CanMount("/media/removable/disk1/dir1/test.zip"));
  EXPECT_FALSE(manager_.CanMount("/media/removable/test.zip/test1.zip"));
  EXPECT_FALSE(manager_.CanMount("/home/chronos/user/Downloads/test1.zip"));
  EXPECT_FALSE(manager_.CanMount(""));
  EXPECT_FALSE(manager_.CanMount("/tmp"));
  EXPECT_FALSE(manager_.CanMount("/media/removable"));
  EXPECT_FALSE(manager_.CanMount("/media/removable/"));
  EXPECT_FALSE(manager_.CanMount("/media/archive"));
  EXPECT_FALSE(manager_.CanMount("/media/archive/"));
  EXPECT_FALSE(manager_.CanMount("/home/chronos/user/Downloads"));
  EXPECT_FALSE(manager_.CanMount("/home/chronos/user/Downloads/"));
}

TEST_F(DiskManagerTest, CanUnmount) {
  EXPECT_TRUE(manager_.CanUnmount("/dev/sda1"));
  EXPECT_TRUE(manager_.CanUnmount("/devices/block/sda/sda1"));
  EXPECT_TRUE(manager_.CanUnmount("/sys/devices/block/sda/sda1"));
  EXPECT_TRUE(manager_.CanUnmount("/media/removable/disk1"));
  EXPECT_TRUE(manager_.CanUnmount("/media/removable/disk1/"));
  EXPECT_TRUE(manager_.CanUnmount("/media/removable/disk 1"));
  EXPECT_FALSE(manager_.CanUnmount("/media/archive/test.zip"));
  EXPECT_FALSE(manager_.CanUnmount("/media/archive/test.zip/"));
  EXPECT_FALSE(manager_.CanUnmount("/media/archive/test 1.zip"));
  EXPECT_FALSE(manager_.CanUnmount("/media/removable/disk1/test.zip"));
  EXPECT_FALSE(manager_.CanUnmount("/media/removable/disk1/test 1.zip"));
  EXPECT_FALSE(manager_.CanUnmount("/media/removable/disk1/dir1/test.zip"));
  EXPECT_FALSE(manager_.CanUnmount("/media/removable/test.zip/test1.zip"));
  EXPECT_FALSE(manager_.CanUnmount("/home/chronos/user/Downloads/test1.zip"));
  EXPECT_FALSE(manager_.CanUnmount(""));
  EXPECT_FALSE(manager_.CanUnmount("/tmp"));
  EXPECT_FALSE(manager_.CanUnmount("/media/removable"));
  EXPECT_FALSE(manager_.CanUnmount("/media/removable/"));
  EXPECT_FALSE(manager_.CanUnmount("/media/archive"));
  EXPECT_FALSE(manager_.CanUnmount("/media/archive/"));
  EXPECT_FALSE(manager_.CanUnmount("/home/chronos/user/Downloads"));
  EXPECT_FALSE(manager_.CanUnmount("/home/chronos/user/Downloads/"));
}

TEST_F(DiskManagerTest, DoMountDiskWithNonexistentSourcePath) {
  string filesystem_type = "ext3";
  string source_path = "/dev/nonexistent-path";
  string mount_path = "/tmp/cros-disks-test";
  vector<string> options;
  EXPECT_EQ(kMountErrorInvalidDevicePath,
            manager_.DoMount(source_path, filesystem_type, options,
                             mount_path));
}

TEST_F(DiskManagerTest, DoUnmountDiskWithInvalidUnmountOptions) {
  string source_path = "/dev/nonexistent-path";
  vector<string> options;
  options.push_back("invalid-unmount-option");
  EXPECT_EQ(kMountErrorInvalidUnmountOptions,
            manager_.DoUnmount(source_path, options));
}

}  // namespace cros_disks
