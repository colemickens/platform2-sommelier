// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_INSTALL_CONFIG
#define CHROMEOS_INSTALL_CONFIG

#include "inst_util.h"

#include <string>

enum BiosType {
  kBiosTypeUnknown,
  kBiosTypeSecure,
  kBiosTypeUBoot,
  kBiosTypeLegacy,
  kBiosTypeEFI,
};

// We commonly need to have the same data about devices in multiple formats
// during the install process. This class allows us to have a partition
// device in whichever format is currently most useful.
//
// Partition device name "/dev/sda3"
// Base device and number "/dev/sda" 3
// Mount point (optional) "/tmp/root.mnt"
class Partition {
 public:
  Partition() {};
  Partition(std::string device) : device_(device) {};
  Partition(std::string device, std::string mount) :
      device_(device), mount_(mount) {};

  // Get/Set the partition device, usually of form: /dev/sda3
  std::string device() const { return device_; }
  void set_device(const std::string& device) { device_ = device; }

  // If the device is /dev/sda3 the base_device is /dev/sda
  std::string base_device() const {
    return GetBlockDevFromPartitionDev(device());
  }

  // If the device is /dev/sda3 the number is 3
  int number() const {
    return GetPartitionFromPartitionDev(device());
  }

  std::string uuid() const;

  // The mount point for this device or "" if unmounted/unknown
  std::string mount() const { return mount_; }
  void set_mount(const std::string& mount) { mount_ = mount; }

 private:
  std::string device_;
  std::string mount_;
};

// This class contains all of the information commonly passed around
// during a post install.
struct InstallConfig {
  // "A" or "B" in a standard install
  std::string slot;

  Partition root;
  Partition kernel;
  Partition boot;

  BiosType bios_type;
};

#endif  // CHROMEOS_INSTALL_CONFIG
