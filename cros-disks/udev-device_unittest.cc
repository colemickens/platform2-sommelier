// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cros-disks/udev-device.h"

#include <libudev.h>

#include <base/logging.h>
#include <gtest/gtest.h>
#include <rootdev/rootdev.h>

using std::string;
using std::vector;

namespace cros_disks {

static const char kLoopDeviceFile[] = "/dev/loop0";

class UdevDeviceTest : public ::testing::Test {
 protected:
  static void SetUpTestCase() {
    udev_ = udev_new();
    if (!udev_)
      return;

    string boot_device_path = GetBootDevicePath();

    struct udev_enumerate *enumerate = udev_enumerate_new(udev_);
    udev_enumerate_add_match_subsystem(enumerate, "block");
    udev_enumerate_scan_devices(enumerate);

    struct udev_list_entry *device_list, *device_list_entry;
    device_list = udev_enumerate_get_list_entry(enumerate);
    udev_list_entry_foreach(device_list_entry, device_list) {
      const char *path = udev_list_entry_get_name(device_list_entry);
      struct udev_device* device = udev_device_new_from_syspath(udev_, path);
      if (!device) continue;

      const char *device_file = udev_device_get_devnode(device);
      if (!device_file) continue;

      if (!boot_device_ && !boot_device_path.empty() &&
          boot_device_path == device_file) {
        udev_device_ref(device);
        boot_device_ = device;
      }

      vector<string> mount_paths = UdevDevice::GetMountPaths(device_file);
      if (!mounted_device_ && !mount_paths.empty()) {
        udev_device_ref(device);
        mounted_device_ = device;
      }

      if (!loop_device_ && strcmp(device_file, kLoopDeviceFile) == 0) {
        udev_device_ref(device);
        loop_device_ = device;
      }

      udev_device_unref(device);
    }
    udev_enumerate_unref(enumerate);
  }

  static void TearDownTestCase() {
    if (boot_device_) {
      udev_device_unref(boot_device_);
      boot_device_ = NULL;
    }
    if (loop_device_) {
      udev_device_unref(loop_device_);
      loop_device_ = NULL;
    }
    if (mounted_device_) {
      udev_device_unref(mounted_device_);
      mounted_device_ = NULL;
    }
    if (udev_) {
      udev_unref(udev_);
      udev_ = NULL;
    }
  }

  static string GetBootDevicePath() {
    char boot_device_path[PATH_MAX];
    if (rootdev(boot_device_path, sizeof(boot_device_path), true, true))
      return boot_device_path;
    return string();
  }

  static struct udev* udev_;
  static struct udev_device* boot_device_;
  static struct udev_device* loop_device_;
  static struct udev_device* mounted_device_;
};

struct udev* UdevDeviceTest::udev_ = NULL;
struct udev_device* UdevDeviceTest::boot_device_ = NULL;
struct udev_device* UdevDeviceTest::loop_device_ = NULL;
struct udev_device* UdevDeviceTest::mounted_device_ = NULL;

TEST_F(UdevDeviceTest, IsValueBooleanTrue) {
  if (mounted_device_) {
    UdevDevice device(mounted_device_);
    EXPECT_FALSE(device.IsValueBooleanTrue(NULL));
    EXPECT_FALSE(device.IsValueBooleanTrue(""));
    EXPECT_FALSE(device.IsValueBooleanTrue("0"));
    EXPECT_FALSE(device.IsValueBooleanTrue("test"));
    EXPECT_TRUE(device.IsValueBooleanTrue("1"));
  }
}

TEST_F(UdevDeviceTest, IsAttributeTrueForNonexistentAttribute) {
  if (mounted_device_) {
    UdevDevice device(mounted_device_);
    EXPECT_FALSE(device.IsAttributeTrue("nonexistent-attribute"));
  }
}

TEST_F(UdevDeviceTest, HasAttributeForExistentAttribute) {
  if (mounted_device_) {
    UdevDevice device(mounted_device_);
    EXPECT_TRUE(device.HasAttribute("stat"));
    EXPECT_TRUE(device.HasAttribute("size"));
  }
}

TEST_F(UdevDeviceTest, GetAttributeForExistentAttribute) {
  if (mounted_device_) {
    UdevDevice device(mounted_device_);
    EXPECT_NE("", device.GetAttribute("size"));
  }
}

TEST_F(UdevDeviceTest, GetAttributeForNonexistentAttribute) {
  if (mounted_device_) {
    UdevDevice device(mounted_device_);
    EXPECT_EQ("", device.GetAttribute("nonexistent-attribute"));
  }
}

TEST_F(UdevDeviceTest, HasAttributeForNonexistentAttribute) {
  if (mounted_device_) {
    UdevDevice device(mounted_device_);
    EXPECT_FALSE(device.HasAttribute("nonexistent-attribute"));
  }
}

TEST_F(UdevDeviceTest, IsPropertyTrueForNonexistentProperty) {
  if (mounted_device_) {
    UdevDevice device(mounted_device_);
    EXPECT_FALSE(device.IsPropertyTrue("nonexistent-property"));
  }
}

TEST_F(UdevDeviceTest, GetPropertyForExistentProperty) {
  if (mounted_device_) {
    UdevDevice device(mounted_device_);
    EXPECT_NE("", device.GetProperty("DEVTYPE"));
  }
}

TEST_F(UdevDeviceTest, GetPropertyForNonexistentProperty) {
  if (mounted_device_) {
    UdevDevice device(mounted_device_);
    EXPECT_EQ("", device.GetProperty("nonexistent-property"));
  }
}

TEST_F(UdevDeviceTest, HasPropertyForExistentProperty) {
  if (mounted_device_) {
    UdevDevice device(mounted_device_);
    EXPECT_TRUE(device.HasProperty("DEVTYPE"));
    EXPECT_TRUE(device.HasProperty("DEVNAME"));
  }
}

TEST_F(UdevDeviceTest, HasPropertyForNonexistentProperty) {
  if (mounted_device_) {
    UdevDevice device(mounted_device_);
    EXPECT_FALSE(device.HasProperty("nonexistent-property"));
  }
}

TEST_F(UdevDeviceTest, GetPropertyFromBlkIdForNonexistentProperty) {
  if (mounted_device_) {
    UdevDevice device(mounted_device_);
    EXPECT_EQ("", device.GetPropertyFromBlkId("nonexistent-property"));
  }
}

TEST_F(UdevDeviceTest, IsAutoMountable) {
  if (boot_device_) {
    UdevDevice device(boot_device_);
    EXPECT_FALSE(device.IsAutoMountable());
  }
  if (loop_device_) {
    UdevDevice device(loop_device_);
    EXPECT_FALSE(device.IsAutoMountable());
  }
}

TEST_F(UdevDeviceTest, IsOnBootDevice) {
  if (boot_device_) {
    UdevDevice device(boot_device_);
    EXPECT_TRUE(device.IsOnBootDevice());
  }
  if (loop_device_) {
    UdevDevice device(loop_device_);
    EXPECT_FALSE(device.IsOnBootDevice());
  }
}

TEST_F(UdevDeviceTest, IsOnRemovableDevice) {
  if (loop_device_) {
    UdevDevice device(loop_device_);
    EXPECT_FALSE(device.IsOnRemovableDevice());
  }
}

TEST_F(UdevDeviceTest, IsMediaAvailable) {
  if (mounted_device_) {
    UdevDevice device(mounted_device_);
    EXPECT_TRUE(device.IsMediaAvailable());
  }
}

TEST_F(UdevDeviceTest, IsVirtual) {
  if (loop_device_) {
    UdevDevice device(loop_device_);
    EXPECT_TRUE(device.IsVirtual());
  }
}

TEST_F(UdevDeviceTest, GetSizeInfo) {
  if (mounted_device_) {
    UdevDevice device(mounted_device_);
    uint64 total_size = 0, remaining_size = 0;
    device.GetSizeInfo(&total_size, &remaining_size);
    LOG(INFO) << "GetSizeInfo: total=" << total_size
              << ", remaining=" << remaining_size;
    EXPECT_GT(total_size, 0);
  }
}

TEST_F(UdevDeviceTest, GetMountPaths) {
  if (mounted_device_) {
    UdevDevice device(mounted_device_);
    vector<string> mount_paths = device.GetMountPaths();
    EXPECT_FALSE(mount_paths.empty());
  }
}

TEST_F(UdevDeviceTest, ToDisk) {
  if (boot_device_) {
    UdevDevice device(boot_device_);
    Disk disk = device.ToDisk();
    EXPECT_FALSE(disk.is_auto_mountable());
    EXPECT_TRUE(disk.is_on_boot_device());
  }
  if (loop_device_) {
    UdevDevice device(loop_device_);
    Disk disk = device.ToDisk();
    EXPECT_FALSE(disk.is_auto_mountable());
    EXPECT_TRUE(disk.is_virtual());
    EXPECT_EQ(kLoopDeviceFile, disk.device_file());
  }
  if (mounted_device_) {
    UdevDevice device(mounted_device_);
    Disk disk = device.ToDisk();
    EXPECT_TRUE(disk.is_mounted());
    EXPECT_FALSE(disk.mount_paths().empty());
  }
}

}  // namespace cros_disks
