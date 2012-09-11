// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CROS_DISKS_UDEV_DEVICE_H_
#define CROS_DISKS_UDEV_DEVICE_H_

#include <blkid/blkid.h>

#include <string>
#include <vector>

#include <base/basictypes.h>
#include <chromeos/dbus/service_constants.h>
#include <gtest/gtest_prod.h>

#include "cros-disks/disk.h"

struct udev_device;

namespace cros_disks {

// A utility class that helps query information about a udev device.
class UdevDevice {
 public:
  explicit UdevDevice(struct udev_device *dev);
  ~UdevDevice();

  // Gets the string value of a device attribute.
  std::string GetAttribute(const char *key) const;

  // Checks if the value of a device attribute represents a Boolean true.
  bool IsAttributeTrue(const char *key) const;

  // Checks if a device attribute exists.
  bool HasAttribute(const char *key) const;

  // Gets the string value of a device property.
  std::string GetProperty(const char *key) const;

  // Checks if the value of a device property represents a Boolean true.
  bool IsPropertyTrue(const char *key) const;

  // Checks if a device property exists.
  bool HasProperty(const char *key) const;

  // Gets the string value of a device property from blkid.
  std::string GetPropertyFromBlkId(const char *key);

  // Gets the total and remaining capacity of the device.
  void GetSizeInfo(uint64 *total_size, uint64 *remaining_size) const;

  // Gets the number of primary partitions on the device.
  size_t GetPrimaryPartitionCount() const;

  // Gets the device media type used on the device.
  DeviceMediaType GetDeviceMediaType() const;

  // Gets the USB vendor and product ID of the device. Returns true if the
  // IDs are found.
  bool GetVendorAndProductId(std::string* vendor_id,
                             std::string* product_id) const;

  // Checks if a device should be auto-mounted. Currently, all external
  // disk devices, which are neither on the boot device nor virtual,
  // are considered auto-mountable.
  bool IsAutoMountable() const;

  // Checks if a device should be hidden from the file browser.
  bool IsHidden();

  // Checks if any media is available in the device.
  bool IsMediaAvailable() const;

  // Checks if the device is on the boot device.
  bool IsOnBootDevice() const;

  // Checks if the device is on the removable device.
  bool IsOnRemovableDevice() const;

  // Checks if the device is a virtual device.
  bool IsVirtual() const;

  // Gets the native sysfs path of the device.
  std::string NativePath() const;

  // Gets the mount paths for the device.
  std::vector<std::string> GetMountPaths() const;

  // Gets the mount paths for a given device path.
  static std::vector<std::string> GetMountPaths(
      const std::string& device_path);

  // Returns a Disk object based on the device information.
  Disk ToDisk();

 private:
  // Returns |str| if |str| is a valid UTF8 string (determined by
  // base::IsStringUTF8) or an empty string otherwise.
  static std::string EnsureUTF8String(const std::string& str);

  // Checks if a string contains a "1" (as Boolean true).
  bool IsValueBooleanTrue(const char *value) const;

  mutable struct udev_device *dev_;

  blkid_cache blkid_cache_;

  FRIEND_TEST(UdevDeviceTest, EnsureUTF8String);
  FRIEND_TEST(UdevDeviceTest, IsValueBooleanTrue);

  DISALLOW_COPY_AND_ASSIGN(UdevDevice);
};

}  // namespace cros_disks

#endif  // CROS_DISKS_UDEV_DEVICE_H_
