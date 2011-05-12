// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CROS_DISKS_UDEV_DEVICE_H_
#define CROS_DISKS_UDEV_DEVICE_H_

#include <iostream>
#include <string>
#include <vector>

#include <base/basictypes.h>

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

  // Gets the total and remaining capacity of the device.
  void GetSizeInfo(uint64 *total_size, uint64 *remaining_size) const;

  // Checks if any media is available in the device.
  bool IsMediaAvailable() const;

  // Checks if a device is on the boot device.
  bool IsOnBootDevice() const;

  // Gets the mount paths for the device.
  std::vector<std::string> GetMountPaths() const;

  // Gets the mount paths for a given device path.
  static std::vector<std::string> GetMountPaths(
      const std::string& device_path);

  // Gets the mount paths for an input stream that has the
  // same format as /proc/mounts
  static std::vector<std::string> ParseMountPaths(
      const std::string& device_path, std::istream& stream);

  // Returns a Disk object based on the device information.
  Disk ToDisk() const;

 private:
  // Checks if a string contains a "1" (as Boolean true).
  bool IsValueBooleanTrue(const char *value) const;

  mutable struct udev_device *dev_;
};

}  // namespace cros_disks

#endif  // CROS_DISKS_UDEV_DEVICE_H_
