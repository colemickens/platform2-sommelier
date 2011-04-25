// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DISK_MANAGER_H__
#define DISK_MANAGER_H__

#include <libudev.h>
#include <iostream>
#include <string>
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
  virtual std::vector<Disk> EnumerateDisks() const;

  // Reads the changes from udev. Must be called to clear the 
  // fd.
  bool ProcessUdevChanges(std::string *device_path, std::string *action);

  // Gets a Disk object that corresponds to a given device file.
  bool GetDiskByDevicePath(const std::string& device_path, Disk *disk) const;

  // Gets the list of filesystems available on the system, except for those
  // marked as nodev, by reading /proc/filesystems.
  std::vector<std::string> GetFilesystems() const;

  // Gets the list of filesystems available on the system, except for those
  // marked as nodev, by reading a stream, which follows the same format as
  // /proc/filesystems.
  std::vector<std::string> GetFilesystems(std::istream& stream) const;

  // Create a mount directory for a given disk.
  std::string CreateMountDirectory(const Disk& disk,
      const std::string& target_path) const;

  // Extracts mount flags and data for Mount() from an array of options.
  bool ExtractMountOptions(const std::vector<std::string>& options,
      unsigned long *mount_flags, std::string *mount_data) const;

  // Extracts unmount flags for Unmount() from an array of options.
  bool ExtractUnmountOptions(const std::vector<std::string>& options,
      int *unmount_flags) const;

  // Mounts a given disk.
  bool Mount(const std::string& device_path,
      const std::string& filesystem_type,
      const std::vector<std::string>& options,
      std::string *mount_path) const;

  // Unmounts a given device path.
  bool Unmount(const std::string& device_path,
      const std::vector<std::string>& options) const;

  // A file descriptor that can be select()ed or poll()ed for system changes.
  int udev_monitor_fd() const { return udev_monitor_fd_; }

 private:

  // The root udev object.
  mutable struct udev* udev_;

  // Provides access to udev changes as they occur.
  struct udev_monitor* udev_monitor_;

  // A file descriptor that indicates changes to the system.
  int udev_monitor_fd_;
};

} // namespace cros_disks

#endif // DISK_MANAGER_H__
