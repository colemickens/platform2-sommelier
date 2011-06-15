// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sys/mount.h>
#include <sys/types.h>

#include <base/memory/scoped_temp_dir.h>
#include <gtest/gtest.h>

#include "cros-disks/disk.h"
#include "cros-disks/disk-manager.h"
#include "cros-disks/filesystem.h"
#include "cros-disks/mounter.h"

namespace cros_disks {

class DiskManagerTest : public ::testing::Test {
 protected:
  DiskManager manager_;
};

TEST_F(DiskManagerTest, CreateDirectory) {
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  std::string target_path = temp_dir.path().value() + "/disk";
  EXPECT_TRUE(manager_.CreateDirectory(target_path));
}

TEST_F(DiskManagerTest, CreateExistingEmptyDirectory) {
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  std::string target_path = temp_dir.path().value() + "/disk";
  EXPECT_TRUE(manager_.CreateDirectory(target_path));
  // Create the same directory again should succeed
  // since it is empty and not in use.
  EXPECT_TRUE(manager_.CreateDirectory(target_path));
}

TEST_F(DiskManagerTest, CreateDirectoryWithoutPermission) {
  EXPECT_FALSE(manager_.CreateDirectory("/proc"));
}

TEST_F(DiskManagerTest, CreateExternalMounter) {
  Disk disk;
  disk.set_device_file("/dev/sda1");

  Filesystem filesystem("ntfs");
  filesystem.set_mount_type("ntfs-3g");
  filesystem.set_requires_external_mounter(true);

  std::string target_path = "/media/disk";

  std::vector<std::string> options;
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

  std::string target_path = "/media/disk";

  std::vector<std::string> options;
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

TEST_F(DiskManagerTest, EnumerateDisks) {
  std::vector<Disk> disks = manager_.EnumerateDisks();
}

TEST_F(DiskManagerTest, GetDeviceFileFromCache) {
  std::string device_path = "/sys/block/sda";
  std::string device_file = "/dev/sda";

  EXPECT_EQ("", manager_.GetDeviceFileFromCache(device_path));
  // Mimic the device file being added to the cache.
  manager_.device_file_map_[device_path] = device_file;
  EXPECT_EQ(device_file, manager_.GetDeviceFileFromCache(device_path));
}

TEST_F(DiskManagerTest, GetDiskByDevicePath) {
  Disk disk;
  std::string device_path = "/dev/sda";
  EXPECT_TRUE(manager_.GetDiskByDevicePath(device_path, &disk));
  EXPECT_EQ(device_path, disk.device_file());

  device_path = "/dev/sda1";
  EXPECT_TRUE(manager_.GetDiskByDevicePath(device_path, &disk));
  EXPECT_EQ(device_path, disk.device_file());
}

TEST_F(DiskManagerTest, GetDiskByNonexistentDevicePath) {
  Disk disk;
  std::string device_path = "/dev/nonexistent-path";
  EXPECT_FALSE(manager_.GetDiskByDevicePath(device_path, &disk));
}

TEST_F(DiskManagerTest, GetFilesystemTypeOfNonexistentDevice) {
  std::string device_path = "/dev/nonexistent-path";
  std::string filesystem_type = manager_.GetFilesystemTypeOfDevice(device_path);
  EXPECT_EQ("", filesystem_type);
}

TEST_F(DiskManagerTest, GetUserAndGroupIdForRoot) {
  uid_t uid;
  gid_t gid;
  EXPECT_TRUE(manager_.GetUserAndGroupId("root", &uid, &gid));
  EXPECT_EQ(0, uid);
  EXPECT_EQ(0, gid);
}

TEST_F(DiskManagerTest, GetUserAndGroupIdForNonExistentUser) {
  uid_t uid;
  gid_t gid;
  EXPECT_FALSE(manager_.GetUserAndGroupId("non-existent-user", &uid, &gid));
}

TEST_F(DiskManagerTest, ExtractSupportedUnmountOptions) {
  int unmount_flags = 0;
  int expected_unmount_flags = MNT_FORCE;
  std::vector<std::string> options;
  options.push_back("force");
  EXPECT_TRUE(manager_.ExtractUnmountOptions(options, &unmount_flags));
  EXPECT_EQ(expected_unmount_flags, unmount_flags);
}

TEST_F(DiskManagerTest, ExtractUnsupportedUnmountOptions) {
  int unmount_flags = 0;
  std::vector<std::string> options;
  options.push_back("foo");
  EXPECT_FALSE(manager_.ExtractUnmountOptions(options, &unmount_flags));
}

TEST_F(DiskManagerTest, GetMountDirectoryNameForDiskWithoutLabelOrUuid) {
  Disk disk;
  std::string mount_path = manager_.GetMountDirectoryName(disk);
  EXPECT_EQ("/media/disk", mount_path);
}

TEST_F(DiskManagerTest, GetMountDirectoryNameForDiskWithLabel) {
  Disk disk;
  disk.set_label("My Disk");
  disk.set_uuid("1A2B-3C4D");
  std::string mount_path = manager_.GetMountDirectoryName(disk);
  EXPECT_EQ("/media/My Disk", mount_path);
}

TEST_F(DiskManagerTest, GetMountDirectoryNameForDiskWithLabelWithSlashes) {
  Disk disk;
  disk.set_label("This/Is/My/Disk");
  disk.set_uuid("1A2B-3C4D");
  std::string mount_path = manager_.GetMountDirectoryName(disk);
  EXPECT_EQ("/media/This_Is_My_Disk", mount_path);
}

TEST_F(DiskManagerTest, GetMountDirectoryNameForDiskWithUuid) {
  Disk disk;
  disk.set_uuid("1A2B-3C4D");
  std::string mount_path = manager_.GetMountDirectoryName(disk);
  EXPECT_EQ("/media/1A2B-3C4D", mount_path);
}

TEST_F(DiskManagerTest, CreateMountDirectoryAlreadyExists) {
  Disk disk;
  std::string mount_path = manager_.CreateMountDirectory(disk, "/proc");
  EXPECT_EQ("", mount_path);
}

TEST_F(DiskManagerTest, CreateMountDirectoryWithGivenName) {
  Disk disk;
  std::string target_path = "/tmp/cros-disks-test";
  std::string mount_path = manager_.CreateMountDirectory(disk, target_path);
  EXPECT_EQ(target_path, mount_path);
  EXPECT_EQ(0, rmdir(target_path.c_str()));
}

TEST_F(DiskManagerTest, MountDiskWithInvalidDevicePath) {
  std::string device_path = "";
  std::string mount_path = "/tmp/cros-disks-test";
  std::string filesystem_type = "ext3";
  std::vector<std::string> options;
  EXPECT_FALSE(manager_.Mount(device_path, filesystem_type, options,
        &mount_path));
}

TEST_F(DiskManagerTest, MountDiskAlreadyMounted) {
  std::string device_path = "/proc";
  std::string mount_path = "/tmp/cros-disks-test";
  std::string filesystem_type = "ext3";
  std::vector<std::string> options;
  EXPECT_FALSE(manager_.Mount(device_path, filesystem_type, options,
        &mount_path));
}

TEST_F(DiskManagerTest, RegisterFilesystem) {
  std::map<std::string, Filesystem>& filesystems = manager_.filesystems_;
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

TEST_F(DiskManagerTest, RemoveDirectory) {
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  EXPECT_TRUE(manager_.RemoveDirectory(temp_dir.path().value()));
}

TEST_F(DiskManagerTest, RemoveNonexistentDirectory) {
  EXPECT_FALSE(manager_.RemoveDirectory("/nonexistent-path"));
}

TEST_F(DiskManagerTest, RemoveDirectoryWithoutPermission) {
  EXPECT_FALSE(manager_.RemoveDirectory("/proc"));
}

TEST_F(DiskManagerTest, UnmountDiskWithInvalidDevicePath) {
  std::string device_path = "";
  std::vector<std::string> options;
  EXPECT_FALSE(manager_.Unmount(device_path, options));
}

TEST_F(DiskManagerTest, UnmountDiskWithNonexistentDevicePath) {
  std::string device_path = "/dev/nonexistent-path";
  std::vector<std::string> options;
  EXPECT_FALSE(manager_.Unmount(device_path, options));
}

}  // namespace cros_disks
