// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "disk-manager.h"

#include "disk.h"

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
  //TODO(rtc): implement this...
  std::vector<Disk> disks;
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
