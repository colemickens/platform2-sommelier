// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DISK_MANAGER_H__
#define DISK_MANAGER_H__

#include <libudev.h>
#include <vector>

namespace cros_disks {

class Disk;

// The DiskManager is responsible for reading device state from udev. 
// Said changes could be the result of a udev notification or a synchronous 
// call to enumerate the relevant storage devices attached to the system. 
//
// Sample Usage:
//
// DiskManager manager;
// manager.EnumerateDisks();
// select(manager.udev_monitor_fd())...
//
// This class is designed to run within a single-threaded GMainLoop application
// and should not be considered thread safe.
class DiskManager {
 public:
  DiskManager();
  virtual ~DiskManager();

  // Lists the current block devices attached to the system.
  virtual std::vector<Disk> EnumerateDisks();

  // Reads the changes from udev. Must be called to clear the 
  // fd.
  bool ProcessUdevChanges();

  // A file descriptor that can be select()ed or poll()ed for system changes.
  int udev_monitor_fd() const { return udev_monitor_fd_; }

 private:

  // The root udev object.
  struct udev* udev_;

  // Provides access to udev changes as they occur.
  struct udev_monitor* udev_monitor_;

  // A file descriptor that indicates changes to the system.
  int udev_monitor_fd_;
};

} // namespace cros_disks

#endif // DISK_MANAGER_H__
