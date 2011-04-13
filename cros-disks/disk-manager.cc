// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "disk-manager.h"
#include "disk.h"
#include "udev-device.h"

#include <base/logging.h>
#include <libudev.h>
#include <vector>


namespace cros_disks {

DiskManager::DiskManager() 
    : udev_(udev_new()), 
      udev_monitor_fd_(0) { 

  CHECK(udev_) << "Failed to initialize udev";
  udev_monitor_ = udev_monitor_new_from_netlink(udev_, "udev");
  udev_monitor_filter_add_match_subsystem_devtype(udev_monitor_, "block", NULL);
  udev_monitor_enable_receiving(udev_monitor_);
  udev_monitor_fd_ = udev_monitor_get_fd(udev_monitor_);
}

DiskManager::~DiskManager() {
  udev_monitor_unref(udev_monitor_);
  udev_unref(udev_);
}

std::vector<Disk> DiskManager::EnumerateDisks() {
  std::vector<Disk> disks;

  struct udev_enumerate *enumerate = udev_enumerate_new(udev_);
  udev_enumerate_add_match_subsystem(enumerate, "block");
  udev_enumerate_scan_devices(enumerate);

  struct udev_list_entry *device_list, *device_list_entry;
  device_list = udev_enumerate_get_list_entry(enumerate);
  udev_list_entry_foreach(device_list_entry, device_list) {
    const char *path = udev_list_entry_get_name(device_list_entry);
    udev_device *dev = udev_device_new_from_syspath(udev_, path);

    LOG(INFO) << "Device";
    LOG(INFO) << "   Node: " << udev_device_get_devnode(dev);
    LOG(INFO) << "   Subsystem: " << udev_device_get_subsystem(dev);
    LOG(INFO) << "   Devtype: " << udev_device_get_devtype(dev);
    LOG(INFO) << "   Devpath: " << udev_device_get_devpath(dev);
    LOG(INFO) << "   Sysname: " << udev_device_get_sysname(dev);
    LOG(INFO) << "   Syspath: " << udev_device_get_syspath(dev);
    LOG(INFO) << "   Properties: ";
    struct udev_list_entry *property_list, *property_list_entry;
    property_list = udev_device_get_properties_list_entry(dev);
    udev_list_entry_foreach (property_list_entry, property_list) {
      const char *key = udev_list_entry_get_name(property_list_entry);
      const char *value = udev_list_entry_get_value(property_list_entry);
      LOG(INFO) << "      " << key << " = " << value;
    }

    disks.push_back(UdevDevice(dev).ToDisk());
    udev_device_unref(dev);
  }

  udev_enumerate_unref(enumerate);

  return disks;
}

bool DiskManager::ProcessUdevChanges() {
  struct udev_device *dev = udev_monitor_receive_device(udev_monitor_);
  CHECK(dev) << "Unknown udev device";
  LOG(INFO) << "Got Device";
  LOG(INFO) << "   Node: " << udev_device_get_devnode(dev);
  LOG(INFO) << "   Subsystem: " << udev_device_get_subsystem(dev);
  LOG(INFO) << "   Devtype: " << udev_device_get_devtype(dev);
  LOG(INFO) << "   Action: " << udev_device_get_action(dev);
  udev_device_unref(dev);
  return true;
}

} // namespace cros_disks
