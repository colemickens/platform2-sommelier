// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cros-disks/udev-device.h"

#include <libudev.h>

#include <base/logging.h>
#include <gtest/gtest.h>

using std::string;
using std::vector;

namespace cros_disks {

class UdevDeviceTest : public ::testing::Test {
 public:
  UdevDeviceTest()
    : udev_(NULL),
      udev_device_(NULL) {
  }

  virtual void SetUp() {
    udev_ = udev_new();
    SelectUdevDeviceForTest();
    if (IsUdevDeviceAvailableForTesting())
      LOG(INFO) << "A udev device is available for testing.";
    else
      LOG(INFO) << "No udev device is available for testing. ";
  }

  virtual void TearDown() {
    if (udev_device_) {
      udev_device_unref(udev_device_);
      udev_device_ = NULL;
    }
    if (udev_) {
      udev_unref(udev_);
      udev_ = NULL;
    }
  }

  struct udev* udev() {
    return udev_;
  }

  struct udev_device* udev_device() {
    return udev_device_;
  }

  bool IsUdevDeviceAvailableForTesting() const {
    return (udev_ != NULL && udev_device_ != NULL);
  }

 private:
  void SelectUdevDeviceForTest() {
    if (udev_ == NULL)
      return;

    if (udev_device_) {
      udev_device_unref(udev_device_);
      udev_device_ = NULL;
    }

    struct udev_enumerate *enumerate = udev_enumerate_new(udev_);
    udev_enumerate_add_match_subsystem(enumerate, "block");
    udev_enumerate_scan_devices(enumerate);

    struct udev_list_entry *device_list, *device_list_entry;
    device_list = udev_enumerate_get_list_entry(enumerate);
    udev_list_entry_foreach(device_list_entry, device_list) {
      const char *path = udev_list_entry_get_name(device_list_entry);
      udev_device_ = udev_device_new_from_syspath(udev_, path);
      if (udev_device_) {
        const char *device_path = udev_device_get_devnode(udev_device_);
        if (device_path) {
          LOG(INFO) << "SelectUdevDeviceForTest: checking if '"
            << device_path << "' is mounted";
          vector<string> mount_paths = UdevDevice::GetMountPaths(device_path);
          if (!mount_paths.empty()) {
            LOG(INFO) << "SelectUdevDeviceForTest: use '" << device_path
              << "' for testing";
            break;
          }
        }
        udev_device_unref(udev_device_);
        udev_device_ = NULL;
      }
    }
    udev_enumerate_unref(enumerate);
  }

  struct udev* udev_;
  struct udev_device* udev_device_;

  DISALLOW_COPY_AND_ASSIGN(UdevDeviceTest);
};

TEST_F(UdevDeviceTest, IsAttributeTrueForNonexistentAttribute) {
  if (IsUdevDeviceAvailableForTesting()) {
    UdevDevice device(udev_device());
    EXPECT_FALSE(device.IsAttributeTrue("nonexistent-attribute"));
  }
}

TEST_F(UdevDeviceTest, HasAttributeForExistentAttribute) {
  if (IsUdevDeviceAvailableForTesting()) {
    UdevDevice device(udev_device());
    EXPECT_TRUE(device.HasAttribute("stat"));
    EXPECT_TRUE(device.HasAttribute("size"));
  }
}

TEST_F(UdevDeviceTest, HasAttributeForNonexistentAttribute) {
  if (IsUdevDeviceAvailableForTesting()) {
    UdevDevice device(udev_device());
    EXPECT_FALSE(device.HasAttribute("nonexistent-attribute"));
  }
}

TEST_F(UdevDeviceTest, IsPropertyTrueForNonexistentProperty) {
  if (IsUdevDeviceAvailableForTesting()) {
    UdevDevice device(udev_device());
    EXPECT_FALSE(device.IsPropertyTrue("nonexistent-property"));
  }
}

TEST_F(UdevDeviceTest, HasPropertyForExistentProperty) {
  if (IsUdevDeviceAvailableForTesting()) {
    UdevDevice device(udev_device());
    EXPECT_TRUE(device.HasProperty("DEVTYPE"));
    EXPECT_TRUE(device.HasProperty("DEVNAME"));
  }
}

TEST_F(UdevDeviceTest, HasPropertyForNonexistentProperty) {
  if (IsUdevDeviceAvailableForTesting()) {
    UdevDevice device(udev_device());
    EXPECT_FALSE(device.HasProperty("nonexistent-property"));
  }
}

TEST_F(UdevDeviceTest, IsMediaAvailable) {
  if (IsUdevDeviceAvailableForTesting()) {
    UdevDevice device(udev_device());
    EXPECT_TRUE(device.IsMediaAvailable());
  }
}

TEST_F(UdevDeviceTest, GetSizeInfo) {
  if (IsUdevDeviceAvailableForTesting()) {
    UdevDevice device(udev_device());
    uint64 total_size = 0, remaining_size = 0;
    device.GetSizeInfo(&total_size, &remaining_size);
    LOG(INFO) << "GetSizeInfo: total=" << total_size
      << ", remaining=" << remaining_size;
    EXPECT_GT(total_size, 0);
  }
}

TEST_F(UdevDeviceTest, GetMountPaths) {
  if (IsUdevDeviceAvailableForTesting()) {
    UdevDevice device(udev_device());
    vector<string> mount_paths = device.GetMountPaths();
    EXPECT_FALSE(mount_paths.empty());
  }
}

}  // namespace cros_disks
