// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cros-disks/disk_manager.h"

#include <stdlib.h>
#include <sys/mount.h>
#include <time.h>

#include <memory>

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/logging.h>
#include <base/stl_util.h>
#include <brillo/process_reaper.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "cros-disks/device_ejector.h"
#include "cros-disks/disk.h"
#include "cros-disks/disk_monitor.h"
#include "cros-disks/exfat_mounter.h"
#include "cros-disks/filesystem.h"
#include "cros-disks/metrics.h"
#include "cros-disks/mounter.h"
#include "cros-disks/ntfs_mounter.h"
#include "cros-disks/platform.h"

using testing::Return;
using testing::_;

namespace {

const char kMountRootDirectory[] = "/media/removable";

}  // namespace

namespace cros_disks {

class MockDeviceEjector : public DeviceEjector {
 public:
  MockDeviceEjector() : DeviceEjector(nullptr) {}

  MOCK_METHOD1(Eject, bool(const std::string& device_path));
};

class MockDiskMonitor : public DiskMonitor {
 public:
  MockDiskMonitor() = default;

  bool Initialize() override { return true; }

  MOCK_CONST_METHOD0(EnumerateDisks, std::vector<Disk>());
  MOCK_CONST_METHOD2(GetDiskByDevicePath,
                     bool(const base::FilePath& device_path, Disk* disk));
};

class DiskManagerTest : public ::testing::Test {
 public:
  DiskManagerTest()
      : manager_(kMountRootDirectory,
                 &platform_,
                 &metrics_,
                 &process_reaper_,
                 &monitor_,
                 &ejector_) {}

 protected:
  Metrics metrics_;
  Platform platform_;
  brillo::ProcessReaper process_reaper_;
  MockDeviceEjector ejector_;
  MockDiskMonitor monitor_;
  DiskManager manager_;
};

TEST_F(DiskManagerTest, CreateExFATMounter) {
  Disk disk;
  disk.device_file = "/dev/sda1";

  Filesystem filesystem("exfat");
  filesystem.mounter_type = ExFATMounter::kMounterType;

  std::string target_path = "/media/disk";
  std::vector<std::string> options = {"rw", "nodev", "noexec", "nosuid"};

  auto mounter = manager_.CreateMounter(disk, filesystem, target_path, options);
  EXPECT_NE(nullptr, mounter.get());
  EXPECT_EQ(filesystem.mount_type, mounter->filesystem_type());
  EXPECT_EQ(disk.device_file, mounter->source());
  EXPECT_EQ(target_path, mounter->target_path().value());
  EXPECT_EQ("rw,nodev,noexec,nosuid", mounter->mount_options().ToString());
}

TEST_F(DiskManagerTest, CreateNTFSMounter) {
  Disk disk;
  disk.device_file = "/dev/sda1";

  Filesystem filesystem("ntfs");
  filesystem.mounter_type = NTFSMounter::kMounterType;

  std::string target_path = "/media/disk";
  std::vector<std::string> options = {"rw", "nodev", "noexec", "nosuid"};

  auto mounter = manager_.CreateMounter(disk, filesystem, target_path, options);
  EXPECT_NE(nullptr, mounter.get());
  EXPECT_EQ(filesystem.mount_type, mounter->filesystem_type());
  EXPECT_EQ(disk.device_file, mounter->source());
  EXPECT_EQ(target_path, mounter->target_path().value());
  EXPECT_EQ("rw,nodev,noexec,nosuid", mounter->mount_options().ToString());
}

TEST_F(DiskManagerTest, CreateVFATSystemMounter) {
  Disk disk;
  disk.device_file = "/dev/sda1";

  Filesystem filesystem("vfat");
  filesystem.extra_mount_options = {"utf8", "shortname=mixed"};

  std::string target_path = "/media/disk";
  std::vector<std::string> options = {"rw", "nodev", "noexec", "nosuid"};

  // Override the time zone to make this test deterministic.
  // This test uses AWST (Perth, Australia), which is UTC+8, as the test time
  // zone. However, the TZ environment variable is the time to be added to local
  // time to get to UTC, hence the negative.
  setenv("TZ", "UTC-8", 1);

  auto mounter = manager_.CreateMounter(disk, filesystem, target_path, options);
  EXPECT_NE(nullptr, mounter.get());
  EXPECT_EQ(filesystem.mount_type, mounter->filesystem_type());
  EXPECT_EQ(disk.device_file, mounter->source());
  EXPECT_EQ(target_path, mounter->target_path().value());
  EXPECT_EQ("utf8,shortname=mixed,time_offset=480,rw,nodev,noexec,nosuid",
            mounter->mount_options().ToString());
}

TEST_F(DiskManagerTest, CreateExt4SystemMounter) {
  Disk disk;
  disk.device_file = "/dev/sda1";

  std::string target_path = "/media/disk";
  // "mand" is not a whitelisted option, so it should be skipped.
  std::vector<std::string> options = {"rw", "nodev", "noexec", "nosuid",
                                      "mand"};

  Filesystem filesystem("ext4");
  auto mounter = manager_.CreateMounter(disk, filesystem, target_path, options);
  EXPECT_NE(nullptr, mounter.get());
  EXPECT_EQ(filesystem.mount_type, mounter->filesystem_type());
  EXPECT_EQ(disk.device_file, mounter->source());
  EXPECT_EQ(target_path, mounter->target_path().value());
  EXPECT_EQ("rw,nodev,noexec,nosuid", mounter->mount_options().ToString());
}

TEST_F(DiskManagerTest, GetFilesystem) {
  EXPECT_EQ(nullptr, manager_.GetFilesystem("nonexistent-fs"));

  Filesystem normal_fs("normal-fs");
  EXPECT_EQ(nullptr, manager_.GetFilesystem(normal_fs.type));
  manager_.RegisterFilesystem(normal_fs);
  EXPECT_NE(nullptr, manager_.GetFilesystem(normal_fs.type));
}

TEST_F(DiskManagerTest, RegisterFilesystem) {
  const std::map<std::string, Filesystem>& filesystems = manager_.filesystems_;
  EXPECT_EQ(0, filesystems.size());
  EXPECT_TRUE(filesystems.find("nonexistent") == filesystems.end());

  Filesystem fat_fs("fat");
  fat_fs.accepts_user_and_group_id = true;
  manager_.RegisterFilesystem(fat_fs);
  EXPECT_EQ(1, filesystems.size());
  EXPECT_TRUE(filesystems.find(fat_fs.type) != filesystems.end());

  Filesystem vfat_fs("vfat");
  vfat_fs.accepts_user_and_group_id = true;
  manager_.RegisterFilesystem(vfat_fs);
  EXPECT_EQ(2, filesystems.size());
  EXPECT_TRUE(filesystems.find(vfat_fs.type) != filesystems.end());
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
  EXPECT_FALSE(manager_.CanMount("/home/chronos/user/GCache/test1.zip"));
  EXPECT_FALSE(
      manager_.CanMount("/home/chronos"
                        "/u-0123456789abcdef0123456789abcdef01234567"
                        "/Downloads/test1.zip"));
  EXPECT_FALSE(
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
  EXPECT_FALSE(manager_.CanUnmount("/home/chronos/user/GCache/test1.zip"));
  EXPECT_FALSE(
      manager_.CanUnmount("/home/chronos"
                          "/u-0123456789abcdef0123456789abcdef01234567"
                          "/Downloads/test1.zip"));
  EXPECT_FALSE(
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
}

TEST_F(DiskManagerTest, DoMountDiskWithNonexistentSourcePath) {
  std::string filesystem_type = "ext3";
  std::string source_path = "/dev/nonexistent-path";
  std::string mount_path = "/tmp/cros-disks-test";
  std::vector<std::string> options;
  MountOptions applied_options;
  EXPECT_EQ(MOUNT_ERROR_INVALID_DEVICE_PATH,
            manager_.DoMount(source_path, filesystem_type, options, mount_path,
                             &applied_options));
}

TEST_F(DiskManagerTest, DoUnmountDiskWithInvalidUnmountOptions) {
  std::string source_path = "/dev/nonexistent-path";
  std::vector<std::string> options = {"invalid-unmount-option"};
  EXPECT_EQ(MOUNT_ERROR_INVALID_UNMOUNT_OPTIONS,
            manager_.DoUnmount(source_path, options));
}

TEST_F(DiskManagerTest, ScheduleEjectOnUnmount) {
  std::string mount_path = "/media/removable/disk";
  Disk disk;
  disk.device_file = "/dev/sr0";
  EXPECT_FALSE(manager_.ScheduleEjectOnUnmount(mount_path, disk));
  EXPECT_FALSE(
      base::ContainsKey(manager_.devices_to_eject_on_unmount_, mount_path));

  disk.media_type = DEVICE_MEDIA_OPTICAL_DISC;
  EXPECT_TRUE(manager_.ScheduleEjectOnUnmount(mount_path, disk));
  EXPECT_TRUE(
      base::ContainsKey(manager_.devices_to_eject_on_unmount_, mount_path));

  disk.media_type = DEVICE_MEDIA_DVD;
  manager_.devices_to_eject_on_unmount_.clear();
  EXPECT_TRUE(manager_.ScheduleEjectOnUnmount(mount_path, disk));
  EXPECT_TRUE(
      base::ContainsKey(manager_.devices_to_eject_on_unmount_, mount_path));
}

TEST_F(DiskManagerTest, EjectDeviceOfMountPath) {
  std::string mount_path = "/media/removable/disk";
  Disk disk;
  disk.device_file = "/dev/sr0";
  manager_.devices_to_eject_on_unmount_[mount_path] = disk;
  EXPECT_CALL(ejector_, Eject("/dev/sr0")).WillOnce(Return(true));
  EXPECT_TRUE(manager_.EjectDeviceOfMountPath(mount_path));
  EXPECT_FALSE(
      base::ContainsKey(manager_.devices_to_eject_on_unmount_, mount_path));
}

TEST_F(DiskManagerTest, EjectDeviceOfMountPathWhenEjectFailed) {
  std::string mount_path = "/media/removable/disk";
  Disk disk;
  disk.device_file = "/dev/sr0";
  manager_.devices_to_eject_on_unmount_[mount_path] = disk;
  EXPECT_CALL(ejector_, Eject(_)).WillOnce(Return(false));
  EXPECT_FALSE(manager_.EjectDeviceOfMountPath(mount_path));
  EXPECT_FALSE(
      base::ContainsKey(manager_.devices_to_eject_on_unmount_, mount_path));
}

TEST_F(DiskManagerTest, EjectDeviceOfMountPathWhenExplicitlyDisabled) {
  std::string mount_path = "/media/removable/disk";
  Disk disk;
  disk.device_file = "/dev/sr0";
  manager_.devices_to_eject_on_unmount_[mount_path] = disk;
  manager_.eject_device_on_unmount_ = false;
  EXPECT_CALL(ejector_, Eject(_)).Times(0);
  EXPECT_FALSE(manager_.EjectDeviceOfMountPath(mount_path));
  EXPECT_FALSE(
      base::ContainsKey(manager_.devices_to_eject_on_unmount_, mount_path));
}

TEST_F(DiskManagerTest, EjectDeviceOfMountPathWhenMountPathExcluded) {
  EXPECT_CALL(ejector_, Eject(_)).Times(0);
  EXPECT_FALSE(manager_.EjectDeviceOfMountPath("/media/removable/disk"));
}

}  // namespace cros_disks
