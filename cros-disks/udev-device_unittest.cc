// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <libudev.h>
#include <sstream>

#include <base/logging.h>
#include <gtest/gtest.h>

#include "udev-device.h"

namespace cros_disks {

class UdevDeviceTest : public ::testing::Test {
 public:
  UdevDeviceTest()
    : udev_(udev_new()),
      udev_device_(NULL)
  {
    EXPECT_TRUE(udev_ != NULL);
    SelectUdevDeviceForTest();
  }

  virtual ~UdevDeviceTest() {
    if (udev_device_)
      udev_device_unref(udev_device_);
    if (udev_)
      udev_unref(udev_);
  }

  virtual void SetUp() {}

  virtual void TearDown() {}

  struct udev* udev() {
    return udev_;
  }

  struct udev_device* udev_device() {
    return udev_device_;
  }

  static std::ostream& GenerateTestMountFileContent(std::ostream& stream) {
    stream << "rootfs / rootfs rw 0 0\n"
      << "none /sys sysfs rw,nosuid,nodev,noexec,relatime 0 0\n"
      << "none /proc proc rw,nosuid,nodev,noexec,relatime 0 0\n"
      << "/dev/sda1 /boot ext2 rw,relatime,errors=continue 0 0\n"
      << "none /dev/shm tmpfs rw,nosuid,nodev,relatime 0 0\n"
      << "/dev/sda1 / ext2 rw,relatime,errors=continue 0 0\n"
      << "/dev/sdb1 /opt ext2 rw,relatime,errors=continue 0 0\n";
    return stream;
  }

 private:

  void SelectUdevDeviceForTest() {
    // TODO(benchan): Make the logic for selecting a valid device more scalable
    static const char *kSystemPathsToTry[] = {
      "/sys/block/sda/sda1",
      "/sys/block/hda/hda1",
      NULL
    };

    for (const char **syspath = kSystemPathsToTry;
        *syspath != NULL && udev_device_ == NULL; ++syspath) {
      udev_device_ = udev_device_new_from_syspath(udev_, *syspath);
    }

    EXPECT_TRUE(udev_device_ != NULL);
  }

  struct udev* udev_;
  struct udev_device* udev_device_;

  DISALLOW_COPY_AND_ASSIGN(UdevDeviceTest);
};

TEST_F(UdevDeviceTest, IsAttributeTrueForNonexistentAttribute) {
  UdevDevice device(udev_device());
  EXPECT_FALSE(device.IsAttributeTrue("nonexistent-attribute"));
}

TEST_F(UdevDeviceTest, HasAttributeForExistentAttribute) {
  UdevDevice device(udev_device());
  EXPECT_TRUE(device.HasAttribute("stat"));
  EXPECT_TRUE(device.HasAttribute("size"));
}

TEST_F(UdevDeviceTest, HasAttributeForNonexistentAttribute) {
  UdevDevice device(udev_device());
  EXPECT_FALSE(device.HasAttribute("nonexistent-attribute"));
}

TEST_F(UdevDeviceTest, IsPropertyTrueForNonexistentProperty) {
  UdevDevice device(udev_device());
  EXPECT_FALSE(device.IsPropertyTrue("nonexistent-property"));
}

TEST_F(UdevDeviceTest, HasPropertyForExistentProperty) {
  UdevDevice device(udev_device());
  EXPECT_TRUE(device.HasProperty("DEVTYPE"));
  EXPECT_TRUE(device.HasProperty("DEVNAME"));
}

TEST_F(UdevDeviceTest, HasPropertyForNonexistentProperty) {
  UdevDevice device(udev_device());
  EXPECT_FALSE(device.HasProperty("nonexistent-property"));
}

TEST_F(UdevDeviceTest, IsMediaAvailable) {
  UdevDevice device(udev_device());
  EXPECT_TRUE(device.IsMediaAvailable());
}

TEST_F(UdevDeviceTest, GetSizeInfo) {
  UdevDevice device(udev_device());
  uint64 total_size = 0, remaining_size = 0;
  device.GetSizeInfo(&total_size, &remaining_size);
  EXPECT_TRUE(total_size > 0);
  EXPECT_TRUE(remaining_size > 0);
}

TEST_F(UdevDeviceTest, GetMountPaths) {
  UdevDevice device(udev_device());
  std::vector<std::string> mount_paths = device.GetMountPaths();
  EXPECT_FALSE(mount_paths.empty());
}

TEST_F(UdevDeviceTest, ParseMountPathsReturnsNoPaths) {
  std::stringstream stream;
  GenerateTestMountFileContent(stream);

  std::vector<std::string> mount_paths;
  mount_paths = UdevDevice::ParseMountPaths("/dev/sdc1", stream);
  EXPECT_EQ(0, mount_paths.size());
}

TEST_F(UdevDeviceTest, ParseMountPathsReturnsOnePath) {
  std::stringstream stream;
  GenerateTestMountFileContent(stream);

  std::vector<std::string> mount_paths;
  mount_paths = UdevDevice::ParseMountPaths("/dev/sdb1", stream);
  EXPECT_EQ(1, mount_paths.size());
  if (mount_paths.size() == 1) {
    EXPECT_EQ("/opt", mount_paths[0]);
  }
}

TEST_F(UdevDeviceTest, ParseMountPathsReturnsMultiplePaths) {
  std::stringstream stream;
  GenerateTestMountFileContent(stream);

  std::vector<std::string> mount_paths;
  mount_paths = UdevDevice::ParseMountPaths("/dev/sda1", stream);
  EXPECT_EQ(2, mount_paths.size());
  if (mount_paths.size() == 2) {
    EXPECT_EQ("/boot", mount_paths[0]);
    EXPECT_EQ("/", mount_paths[1]);
  }
}

}  // namespace cros_disks
