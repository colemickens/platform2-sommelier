// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CROS_DISKS_DISK_MANAGER_H_
#define CROS_DISKS_DISK_MANAGER_H_

#include <blkid/blkid.h>
#include <libudev.h>
#include <sys/types.h>

#include <map>
#include <set>
#include <string>
#include <vector>

#include <base/basictypes.h>
#include <gtest/gtest_prod.h>

#include "cros-disks/device-event.h"

namespace cros_disks {

class Disk;
class Filesystem;
class Mounter;
class UdevDevice;

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

  // Reads the changes from udev and converts the changes into a device event.
  // Returns true on success. Must be called to clear the fd.
  bool GetDeviceEvent(DeviceEvent* event);

  // Returns a Filesystem object if a given filesystem type is supported.
  // Otherwise, it returns NULL.
  const Filesystem* GetFilesystem(const std::string& filesystem_type) const;

  // Gets the filesystem type of a device.
  std::string GetFilesystemTypeOfDevice(const std::string& device_path);

  // Gets a Disk object that corresponds to a given device file.
  bool GetDiskByDevicePath(const std::string& device_path, Disk *disk) const;

  // Gets the user ID and group ID for a given username.
  bool GetUserAndGroupId(const std::string& username,
      uid_t *uid, gid_t *gid) const;

  // Gets the mount directory name for a disk.
  std::string GetMountDirectoryName(const Disk& disk) const;

  // Creates a specified directory if it does not exist.
  // If the specified directory already exists, this function tries to
  // reuse it if the directory is empty and not being used.
  bool CreateDirectory(const std::string& path) const;

  // Creates a mount directory for a given disk.
  std::string CreateMountDirectory(const Disk& disk,
      const std::string& target_path) const;

  // Extracts unmount flags for Unmount() from an array of options.
  bool ExtractUnmountOptions(const std::vector<std::string>& options,
      int *unmount_flags) const;

  // Mounts a given disk.
  bool Mount(const std::string& device_path,
      const std::string& filesystem_type,
      const std::vector<std::string>& options,
      std::string *mount_path);

  // Unmounts a given device path.
  bool Unmount(const std::string& device_path,
      const std::vector<std::string>& options);

  // Registers a set of default filesystems to the disk manager.
  void RegisterDefaultFilesystems();

  // Registers a filesystem to the disk manager.
  // Subsequent registrations of the same filesystem type are ignored.
  void RegisterFilesystem(const Filesystem& filesystem);

  // Removes a directory.
  bool RemoveDirectory(const std::string& path) const;

  bool experimental_features_enabled() const {
    return experimental_features_enabled_;
  }
  void set_experimental_features_enabled(bool experimental_features_enabled) {
    experimental_features_enabled_ = experimental_features_enabled;
  }

  // A file descriptor that can be select()ed or poll()ed for system changes.
  int udev_monitor_fd() const { return udev_monitor_fd_; }

 private:
  // Creates an appropriate mounter object for a given filesystem.
  // The caller is responsible for deleting the mounter object.
  Mounter* CreateMounter(const Disk& disk, const Filesystem& filesystem,
      const std::string& target_path,
      const std::vector<std::string>& options) const;

  // Gets a device file from the cache mapping from sysfs path to device file.
  std::string GetDeviceFileFromCache(const std::string& device_path) const;

  // Determines a device/disk event from a udev block device change.
  DeviceEvent::EventType ProcessBlockDeviceEvent(const UdevDevice& device,
      const char *action);

  // Determines a device/disk event from a udev SCSI device change.
  DeviceEvent::EventType ProcessScsiDeviceEvent(const UdevDevice& device,
      const char *action);

  // The root udev object.
  mutable struct udev* udev_;

  // Provides access to udev changes as they occur.
  struct udev_monitor* udev_monitor_;

  // A file descriptor that indicates changes to the system.
  int udev_monitor_fd_;

  // blkid_cache object.
  blkid_cache blkid_cache_;

  // A set of device sysfs paths detected by the udev monitor.
  std::set<std::string> devices_detected_;

  // A set of disk sysfs paths detected by the udev monitor.
  std::set<std::string> disks_detected_;

  // A cache mapping device sysfs path to device file.
  std::map<std::string, std::string> device_file_map_;

  // This variable is set to true if the disk manager enables
  // experimental features.
  bool experimental_features_enabled_;

  // A set of supported filesystems indexed by filesystem type.
  std::map<std::string, Filesystem> filesystems_;

  FRIEND_TEST(DiskManagerTest, CreateExternalMounter);
  FRIEND_TEST(DiskManagerTest, CreateSystemMounter);
  FRIEND_TEST(DiskManagerTest, GetDeviceFileFromCache);
  FRIEND_TEST(DiskManagerTest, RegisterFilesystem);

  DISALLOW_COPY_AND_ASSIGN(DiskManager);
};

}  // namespace cros_disks

#endif  // CROS_DISKS_DISK_MANAGER_H_
