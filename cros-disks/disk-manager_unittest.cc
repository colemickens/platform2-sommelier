// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/logging.h>
#include <gtest/gtest.h>

#include <sys/mount.h>
#include <unistd.h>
#include <sstream>

#include "disk.h"
#include "disk-manager.h"

namespace cros_disks {

class DiskManagerTest : public ::testing::Test {
 public:
  DiskManagerTest() {}

  virtual ~DiskManagerTest() {}

  virtual void SetUp() {}

  virtual void TearDown() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(DiskManagerTest);
};

TEST_F(DiskManagerTest, EnumerateDisks) {
  DiskManager manager;
  std::vector<Disk> disks = manager.EnumerateDisks();
}

TEST_F(DiskManagerTest, GetDiskByDevicePath) {
  DiskManager manager;
  Disk disk;
  std::string device_path = "/dev/sda";
  EXPECT_TRUE(manager.GetDiskByDevicePath(device_path, &disk));
  EXPECT_EQ(device_path, disk.device_file());

  device_path = "/dev/sda1";
  EXPECT_TRUE(manager.GetDiskByDevicePath(device_path, &disk));
  EXPECT_EQ(device_path, disk.device_file());
}

TEST_F(DiskManagerTest, GetDiskByNonexistentDevicePath) {
  DiskManager manager;
  Disk disk;
  std::string device_path = "/dev/nonexistent-path";
  EXPECT_FALSE(manager.GetDiskByDevicePath(device_path, &disk));
}

TEST_F(DiskManagerTest, GetFilesystems) {
  DiskManager manager;
  std::vector<std::string> filesystems = manager.GetFilesystems();
}

TEST_F(DiskManagerTest, GetFilesystemsFromStream) {
  DiskManager manager;
  std::stringstream stream;
  stream << "nodev\tsysfs\n"
    << "nodev\trootfs\n"
    << "nodev\tproc\n"
    << "nodev\ttmpfs\n"
    << "nodev\tdevpts\n"
    << "\text3\n"
    << "\text2\n"
    << "\text4\n"
    << "nodev\tramfs\n"
    << "nodev\tecryptfs\n"
    << "\tfuseblk\n"
    << "nodev\tnfs\n"
    << "nodev\tnfs4\n"
    << "\tvfat\n";

  std::vector<std::string> filesystems = manager.GetFilesystems(stream);
  EXPECT_EQ(5, filesystems.size());
  if (filesystems.size() == 5) {
    EXPECT_EQ("ext3", filesystems[0]);
    EXPECT_EQ("ext2", filesystems[1]);
    EXPECT_EQ("ext4", filesystems[2]);
    EXPECT_EQ("fuseblk", filesystems[3]);
    EXPECT_EQ("vfat", filesystems[4]);
  }
}

TEST_F(DiskManagerTest, SanitizeMountOptionsWithReadOnlyDisk) {
  DiskManager manager;
  std::vector<std::string> options;
  options.push_back("nodev");
  options.push_back("rw");
  options.push_back("noexec");
  options.push_back("nosuid");
  Disk disk;
  disk.set_is_read_only(true);

  std::vector<std::string> santized_options =
    manager.SanitizeMountOptions(options, disk);
  EXPECT_EQ(3, santized_options.size());
  EXPECT_EQ("nodev", santized_options[0]);
  EXPECT_EQ("noexec", santized_options[1]);
  EXPECT_EQ("nosuid", santized_options[2]);
}

TEST_F(DiskManagerTest, SanitizeMountOptionsWithOpticalDisk) {
  DiskManager manager;
  std::vector<std::string> options;
  options.push_back("nodev");
  options.push_back("rw");
  options.push_back("noexec");
  options.push_back("nosuid");
  Disk disk;
  disk.set_is_optical_disk(true);

  std::vector<std::string> santized_options =
    manager.SanitizeMountOptions(options, disk);
  EXPECT_EQ(3, santized_options.size());
  EXPECT_EQ("nodev", santized_options[0]);
  EXPECT_EQ("noexec", santized_options[1]);
  EXPECT_EQ("nosuid", santized_options[2]);
}

TEST_F(DiskManagerTest, ExtractSupportedMountOptions) {
  DiskManager manager;
  std::vector<std::string> options;
  unsigned long mount_flags, expected_mount_flags;
  std::string mount_data;

  // options: ro (default)
  mount_flags = 0;
  mount_data = "";
  expected_mount_flags = MS_RDONLY;
  EXPECT_TRUE(manager.ExtractMountOptions(options, &mount_flags, &mount_data));
  EXPECT_EQ(expected_mount_flags, mount_flags);
  EXPECT_EQ("", mount_data);

  // options: ro, nodev
  options.push_back("nodev");
  mount_flags = 0;
  mount_data = "";
  expected_mount_flags = MS_RDONLY | MS_NODEV;
  EXPECT_TRUE(manager.ExtractMountOptions(options, &mount_flags, &mount_data));
  EXPECT_EQ(expected_mount_flags, mount_flags);
  EXPECT_EQ("", mount_data);

  // options: nodev, rw
  options.push_back("rw");
  mount_flags = 0;
  mount_data = "";
  expected_mount_flags = MS_NODEV;
  EXPECT_TRUE(manager.ExtractMountOptions(options, &mount_flags, &mount_data));
  EXPECT_EQ(expected_mount_flags, mount_flags);
  EXPECT_EQ("", mount_data);

  // options: nodev, rw, nosuid
  options.push_back("nosuid");
  mount_flags = 0;
  mount_data = "";
  expected_mount_flags = MS_NODEV | MS_NOSUID;
  EXPECT_TRUE(manager.ExtractMountOptions(options, &mount_flags, &mount_data));
  EXPECT_EQ(expected_mount_flags, mount_flags);
  EXPECT_EQ("", mount_data);

  // options: nodev, rw, nosuid, noexec
  options.push_back("noexec");
  mount_flags = 0;
  mount_data = "";
  expected_mount_flags = MS_NODEV | MS_NOSUID | MS_NOEXEC;
  EXPECT_TRUE(manager.ExtractMountOptions(options, &mount_flags, &mount_data));
  EXPECT_EQ(expected_mount_flags, mount_flags);
  EXPECT_EQ("", mount_data);
}

TEST_F(DiskManagerTest, ExtractUnsupportedMountOptions) {
  DiskManager manager;
  unsigned long mount_flags = 0;
  std::string mount_data;
  std::vector<std::string> options;
  options.push_back("foo");
  EXPECT_FALSE(manager.ExtractMountOptions(options, &mount_flags, &mount_data));
}

TEST_F(DiskManagerTest, ExtractSupportedUnmountOptions) {
  DiskManager manager;
  int unmount_flags = 0;
  int expected_unmount_flags = MNT_FORCE;
  std::vector<std::string> options;
  options.push_back("force");
  EXPECT_TRUE(manager.ExtractUnmountOptions(options, &unmount_flags));
  EXPECT_EQ(expected_unmount_flags, unmount_flags);
}

TEST_F(DiskManagerTest, ExtractUnsupportedUnmountOptions) {
  DiskManager manager;
  int unmount_flags = 0;
  std::vector<std::string> options;
  options.push_back("foo");
  EXPECT_FALSE(manager.ExtractUnmountOptions(options, &unmount_flags));
}

TEST_F(DiskManagerTest, CreateMountDirectoryAlreadyExists) {
  DiskManager manager;
  Disk disk;
  std::string mount_path = manager.CreateMountDirectory(disk, "/proc");
  EXPECT_EQ("", mount_path);
}

TEST_F(DiskManagerTest, CreateMountDirectoryWithGivenName) {
  DiskManager manager;
  Disk disk;
  std::string target_path = "/tmp/cros-disks-test";
  std::string mount_path = manager.CreateMountDirectory(disk, target_path);
  EXPECT_EQ(target_path, mount_path);
  EXPECT_EQ(0, rmdir(target_path.c_str()));
}

TEST_F(DiskManagerTest, MountDiskWithInvalidDevicePath) {
  DiskManager manager;
  std::string device_path = "";
  std::string mount_path = "/tmp/cros-disks-test";
  std::string filesystem_type = "ext3";
  std::vector<std::string> options;
  EXPECT_FALSE(manager.Mount(device_path, filesystem_type, options,
        &mount_path));
}

TEST_F(DiskManagerTest, MountDiskAlreadyMounted) {
  DiskManager manager;
  std::string device_path = "/proc";
  std::string mount_path = "/tmp/cros-disks-test";
  std::string filesystem_type = "ext3";
  std::vector<std::string> options;
  EXPECT_FALSE(manager.Mount(device_path, filesystem_type, options,
        &mount_path));
}

TEST_F(DiskManagerTest, UnmountDiskWithInvalidDevicePath) {
  DiskManager manager;
  std::string device_path = "";
  std::vector<std::string> options;
  EXPECT_FALSE(manager.Unmount(device_path, options));
}

TEST_F(DiskManagerTest, UnmountDiskWithNonexistentDevicePath) {
  DiskManager manager;
  std::string device_path = "/dev/nonexistent-path";
  std::vector<std::string> options;
  EXPECT_FALSE(manager.Unmount(device_path, options));
}

}  // namespace cros_disks
