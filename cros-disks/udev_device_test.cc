// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cros-disks/udev_device.h"

#include <libudev.h>

#include <base/logging.h>
#include <gtest/gtest.h>
#include <linux/limits.h>
#include <rootdev/rootdev.h>

namespace {

const char kLoopDevicePrefix[] = "/dev/loop";
const char kRamDeviceFile[] = "/dev/ram0";
const char kZRamDeviceFile[] = "/dev/zram0";

}  // namespace

namespace cros_disks {

class UdevDeviceTest : public ::testing::Test {
 protected:
  static void SetUpTestCase() {
    udev_ = udev_new();
    if (!udev_)
      return;

    std::string boot_device_path = GetBootDevicePath();

    udev_enumerate* enumerate = udev_enumerate_new(udev_);
    udev_enumerate_add_match_subsystem(enumerate, "block");
    udev_enumerate_scan_devices(enumerate);

    udev_list_entry *device_list, *device_list_entry;
    device_list = udev_enumerate_get_list_entry(enumerate);
    udev_list_entry_foreach(device_list_entry, device_list) {
      const char* path = udev_list_entry_get_name(device_list_entry);
      udev_device* device = udev_device_new_from_syspath(udev_, path);
      if (!device)
        continue;

      const char* device_file = udev_device_get_devnode(device);
      if (!device_file)
        continue;

      if (!boot_device_ && !boot_device_path.empty() &&
          boot_device_path == device_file) {
        udev_device_ref(device);
        boot_device_ = device;

        // Check if the boot device is also mounted. If so, use it for tests
        // that expect a mounted device since the boot device is unlikely to
        // be unmounted during the tests.
        std::vector<std::string> mount_paths =
            UdevDevice::GetMountPaths(device_file);
        if (!mounted_device_ && !mount_paths.empty()) {
          udev_device_ref(device);
          mounted_device_ = device;
        }
      }

      if (!loop_device_ && device_file != boot_device_path &&
          strncmp(device_file, kLoopDevicePrefix, strlen(kLoopDevicePrefix)) ==
              0) {
        udev_device_ref(device);
        loop_device_ = device;
      }

      if (!ram_device_ && (strcmp(device_file, kRamDeviceFile) == 0 ||
                           strcmp(device_file, kZRamDeviceFile) == 0)) {
        udev_device_ref(device);
        ram_device_ = device;
      }

      if (!partitioned_device_) {
        const char* device_type = udev_device_get_devtype(device);
        if (device_type && strcmp(device_type, "partition") == 0) {
          partitioned_device_ = udev_device_get_parent(device);
          udev_device_ref(partitioned_device_);
        }
      }

      udev_device_unref(device);
    }
    udev_enumerate_unref(enumerate);
  }

  static void TearDownTestCase() {
    if (boot_device_) {
      udev_device_unref(boot_device_);
      boot_device_ = nullptr;
    }
    if (loop_device_) {
      udev_device_unref(loop_device_);
      loop_device_ = nullptr;
    }
    if (ram_device_) {
      udev_device_unref(ram_device_);
      ram_device_ = nullptr;
    }
    if (mounted_device_) {
      udev_device_unref(mounted_device_);
      mounted_device_ = nullptr;
    }
    if (partitioned_device_) {
      udev_device_unref(partitioned_device_);
      partitioned_device_ = nullptr;
    }
    if (udev_) {
      udev_unref(udev_);
      udev_ = nullptr;
    }
  }

  static std::string GetBootDevicePath() {
    char boot_device_path[PATH_MAX];
    if (rootdev(boot_device_path, sizeof(boot_device_path), true, true) == 0)
      return boot_device_path;
    return std::string();
  }

  static udev* udev_;
  static udev_device* boot_device_;
  static udev_device* loop_device_;
  static udev_device* ram_device_;
  static udev_device* mounted_device_;
  static udev_device* partitioned_device_;
};

udev* UdevDeviceTest::udev_ = nullptr;
udev_device* UdevDeviceTest::boot_device_ = nullptr;
udev_device* UdevDeviceTest::loop_device_ = nullptr;
udev_device* UdevDeviceTest::ram_device_ = nullptr;
udev_device* UdevDeviceTest::mounted_device_ = nullptr;
udev_device* UdevDeviceTest::partitioned_device_ = nullptr;

TEST_F(UdevDeviceTest, EnsureUTF8String) {
  // Valid UTF8
  EXPECT_EQ("ascii", UdevDevice::EnsureUTF8String("ascii"));
  EXPECT_EQ("\xc2\x81", UdevDevice::EnsureUTF8String("\xc2\x81"));
  // Invalid UTF8: overlong sequences
  EXPECT_EQ("", UdevDevice::EnsureUTF8String("\xc0\x80"));  // U+0000
}

TEST_F(UdevDeviceTest, IsValueBooleanTrue) {
  EXPECT_FALSE(UdevDevice::IsValueBooleanTrue(nullptr));
  EXPECT_FALSE(UdevDevice::IsValueBooleanTrue(""));
  EXPECT_FALSE(UdevDevice::IsValueBooleanTrue("0"));
  EXPECT_FALSE(UdevDevice::IsValueBooleanTrue("test"));
  EXPECT_TRUE(UdevDevice::IsValueBooleanTrue("1"));
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

TEST_F(UdevDeviceTest, GetPartitionCount) {
  if (partitioned_device_) {
    UdevDevice device(partitioned_device_);
    EXPECT_NE(0, device.GetPartitionCount());
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

TEST_F(UdevDeviceTest, IsIgnored) {
  if (boot_device_) {
    UdevDevice device(boot_device_);
    EXPECT_FALSE(device.IsIgnored());
  }
  if (loop_device_) {
    UdevDevice device(loop_device_);
    EXPECT_FALSE(device.IsIgnored());
  }
  if (ram_device_) {
    UdevDevice device(ram_device_);
    EXPECT_TRUE(device.IsIgnored());
  }
}

TEST_F(UdevDeviceTest, IsOnBootDevice) {
  if (boot_device_) {
    UdevDevice device(boot_device_);
    EXPECT_TRUE(device.IsOnBootDevice());
  }
#if 0
  // TODO(benchan): Re-enable this test case after figuring out why it fails on
  // some buildbot (chromium:866231).
  if (loop_device_) {
    UdevDevice device(loop_device_);
    EXPECT_FALSE(device.IsOnBootDevice());
  }
#endif
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

TEST_F(UdevDeviceTest, IsMobileBroadbandDevice) {
  if (boot_device_) {
    UdevDevice device(boot_device_);
    EXPECT_FALSE(device.IsMobileBroadbandDevice());
  }
  if (loop_device_) {
    UdevDevice device(loop_device_);
    EXPECT_FALSE(device.IsMobileBroadbandDevice());
  }
}

TEST_F(UdevDeviceTest, IsVirtual) {
  if (loop_device_) {
    UdevDevice device(loop_device_);
    EXPECT_TRUE(device.IsVirtual());
  }
  if (ram_device_) {
    UdevDevice device(ram_device_);
    EXPECT_TRUE(device.IsVirtual());
  }
}

TEST_F(UdevDeviceTest, IsLoopDevice) {
  if (loop_device_) {
    UdevDevice device(loop_device_);
    EXPECT_TRUE(device.IsLoopDevice());
  }
  if (ram_device_) {
    UdevDevice device(ram_device_);
    EXPECT_FALSE(device.IsLoopDevice());
  }
}

TEST_F(UdevDeviceTest, GetSizeInfo) {
  if (mounted_device_) {
    UdevDevice device(mounted_device_);
    uint64_t total_size = 0, remaining_size = 0;
    device.GetSizeInfo(&total_size, &remaining_size);
    LOG(INFO) << "GetSizeInfo: total=" << total_size
              << ", remaining=" << remaining_size;
    EXPECT_GT(total_size, 0);
  }
}

TEST_F(UdevDeviceTest, GetMountPaths) {
  if (mounted_device_) {
    UdevDevice device(mounted_device_);
    std::vector<std::string> mount_paths = device.GetMountPaths();
    EXPECT_FALSE(mount_paths.empty());
  }
}

TEST_F(UdevDeviceTest, ToDisk) {
  if (boot_device_) {
    UdevDevice device(boot_device_);
    Disk disk = device.ToDisk();
    EXPECT_FALSE(disk.is_auto_mountable);
    EXPECT_TRUE(disk.is_on_boot_device);
  }
  if (loop_device_) {
    UdevDevice device(loop_device_);
    Disk disk = device.ToDisk();
    EXPECT_FALSE(disk.is_auto_mountable);
    EXPECT_TRUE(disk.is_virtual);
    EXPECT_EQ(kLoopDevicePrefix,
              disk.device_file.substr(0, strlen(kLoopDevicePrefix)));
  }
  if (mounted_device_) {
    UdevDevice device(mounted_device_);
    Disk disk = device.ToDisk();
    EXPECT_TRUE(disk.IsMounted());
    EXPECT_FALSE(disk.mount_paths.empty());
  }
}

}  // namespace cros_disks
