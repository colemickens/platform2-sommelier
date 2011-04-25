// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "disk-manager.h"
#include "disk.h"
#include "udev-device.h"

#include <base/logging.h>
#include <base/string_number_conversions.h>
#include <base/string_util.h>
#include <errno.h>
#include <libudev.h>
#include <sys/mount.h>
#include <unistd.h>
#include <algorithm>
#include <fstream>
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

std::vector<Disk> DiskManager::EnumerateDisks() const {
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

bool DiskManager::ProcessUdevChanges(std::string *device_path,
    std::string *action) {
  struct udev_device *dev = udev_monitor_receive_device(udev_monitor_);
  CHECK(dev) << "Unknown udev device";
  CHECK(device_path) << "Invalid device path";
  CHECK(action) << "Unknown device action";
  LOG(INFO) << "Got Device";
  LOG(INFO) << "   Node: " << udev_device_get_devnode(dev);
  LOG(INFO) << "   Subsystem: " << udev_device_get_subsystem(dev);
  LOG(INFO) << "   Devtype: " << udev_device_get_devtype(dev);
  LOG(INFO) << "   Action: " << udev_device_get_action(dev);
  *device_path = udev_device_get_devnode(dev);
  *action = udev_device_get_action(dev);
  udev_device_unref(dev);
  return true;
}

bool DiskManager::GetDiskByDevicePath(const std::string& device_path,
    Disk *disk) const {
  bool disk_found = false;

  struct udev_enumerate *enumerate = udev_enumerate_new(udev_);
  udev_enumerate_add_match_subsystem(enumerate, "block");
  udev_enumerate_scan_devices(enumerate);

  struct udev_list_entry *device_list, *device_list_entry;
  device_list = udev_enumerate_get_list_entry(enumerate);
  udev_list_entry_foreach(device_list_entry, device_list) {
    const char *path = udev_list_entry_get_name(device_list_entry);
    udev_device *dev = udev_device_new_from_syspath(udev_, path);
    const char *device_file = udev_device_get_devnode(dev);
    if (device_file && strcmp(device_file, device_path.c_str()) == 0) {
      disk_found = true;
      if (disk)
        *disk = UdevDevice(dev).ToDisk();
    }
    udev_device_unref(dev);
    if (disk_found)
      break;
  }

  udev_enumerate_unref(enumerate);

  return disk_found;
}

std::vector<std::string> DiskManager::GetFilesystems() const {
  std::ifstream stream("/proc/filesystems");
  if (stream.is_open()) {
    return GetFilesystems(stream);
  }
  LOG(ERROR) << "Unable to read /proc/filesystems";
  return std::vector<std::string>();
}

std::vector<std::string> DiskManager::GetFilesystems(
    std::istream& stream) const {
  std::vector<std::string> filesystems;
  std::string line;
  while (std::getline(stream, line)) {
    std::vector<std::string> tokens;
    SplitString(line, '\t', &tokens);
    if (tokens.size() >= 2) {
      if (tokens[0] != "nodev")
        filesystems.push_back(tokens[1]);
    }
  }
  return filesystems;
}

std::string DiskManager::CreateMountDirectory(const Disk& disk,
    const std::string& target_path) const {
  // Construct the mount path as follows:
  // (1) If a target path is specified, use it.
  // (2) Otherwise, if the disk has a non-empty label
  //     use /media/<label> as the target path.
  //     Note: any forward slash '/' in the label is replaced
  //     with a underscore '_'.
  // (3) Otherwise, use /media/disk as the target path.
  // (4) If the target path obtained in (2) or (3) cannot
  //     be created, try suffixing it with a number.

  if (!target_path.empty()) {
    if (mkdir(target_path.c_str(), S_IRWXU) != 0) {
      LOG(ERROR) << "Failed to create directory '" << target_path
        << "' (errno=" << errno << ")";
      return std::string();
    }
    return target_path;
  }

  std::string mount_path;
  if (disk.label().empty()) {
    mount_path = "/media/disk";
  } else {
    mount_path = disk.label();
    std::replace(mount_path.begin(), mount_path.end(), '/', '_');
    mount_path = "/media/" + mount_path;
  }

  unsigned suffix = 1;
  std::string mount_path_prefix = mount_path + "_";
  while (mkdir(mount_path.c_str(), S_IRWXU) != 0) {
    mount_path = mount_path_prefix + base::UintToString(suffix);
    suffix++;
    // TODO(benchan): Should we give up after a number of trials?
  }

  return mount_path;
}

bool DiskManager::ExtractMountOptions(const std::vector<std::string>& options,
    unsigned long *mount_flags, std::string *mount_data) const {
  if (mount_flags == NULL || mount_data == NULL)
    return false;

  *mount_flags = MS_RDONLY;
  mount_data->clear();
  for (std::vector<std::string>::const_iterator
      option_iterator = options.begin(); option_iterator != options.end();
      ++option_iterator) {
    const std::string& option = *option_iterator;
    if (option == "rw") {
      *mount_flags &= ~(unsigned long)MS_RDONLY;
    } else if (option == "nodev") {
      *mount_flags |= MS_NODEV;
    } else if (option == "noexec") {
      *mount_flags |= MS_NOEXEC;
    } else if (option == "nosuid") {
      *mount_flags |= MS_NOSUID;
    } else {
      LOG(ERROR) << "Got unsupported mount option: " << option;
      return false;
    }
  }
  return true;
}

bool DiskManager::ExtractUnmountOptions(const std::vector<std::string>& options,
    int *unmount_flags) const {
  if (unmount_flags == NULL)
    return false;

  *unmount_flags = 0;
  for (std::vector<std::string>::const_iterator
      option_iterator = options.begin(); option_iterator != options.end();
      ++option_iterator) {
    const std::string& option = *option_iterator;
    if (option == "force") {
      *unmount_flags |= MNT_FORCE;
    } else {
      LOG(ERROR) << "Got unsupported unmount option: " << option;
      return false;
    }
  }
  return true;
}

bool DiskManager::Mount(const std::string& device_path,
    const std::string& filesystem_type,
    const std::vector<std::string>& options,
    std::string *mount_path) const {

  if (device_path.empty()) {
    LOG(ERROR) << "Invalid device path.";
    return false;
  }

  Disk disk;
  if (!GetDiskByDevicePath(device_path, &disk)) {
    LOG(ERROR) << "Device not found: " << device_path;
    return false;
  }

  if (disk.is_mounted()) {
    LOG(ERROR) << "'" << device_path << "' is already mounted.";
    return false;
  }

  std::string mount_target = mount_path ? *mount_path : std::string();
  mount_target = CreateMountDirectory(disk, mount_target);
  if (mount_target.empty()) {
    LOG(ERROR) << "Failed to create mount directory for '"
      << device_path << "'";
    return false;
  }

  unsigned long mount_flags;
  std::string mount_data;
  if (!ExtractMountOptions(options, &mount_flags, &mount_data)) {
    LOG(ERROR) << "Invalid mount options.";
    return false;
  }

  std::vector<std::string> filesystems;
  if (filesystem_type.empty()) {
    filesystems = GetFilesystems();
  } else {
    filesystems.push_back(filesystem_type);
  }

  bool mounted = false;
  for (std::vector<std::string>::const_iterator
      filesystem_iterator(filesystems.begin());
      filesystem_iterator != filesystems.end(); ++filesystem_iterator) {
    if (mount(device_path.c_str(), mount_target.c_str(),
          filesystem_iterator->c_str(), mount_flags,
          mount_data.c_str()) == 0) {
      mounted = true;
      LOG(INFO) << "Mounted '" << device_path << "' to '"
        << mount_target << "' as " << *filesystem_iterator;
      break;
    }

    LOG(INFO) << "Failed to mount '" << device_path << "' to '"
      << mount_target << "' as " << *filesystem_iterator
      << " (errno=" << errno << ")";
  }

  if (!mounted) {
    LOG(ERROR) << "Failed to mount '" << device_path << "' to '"
      << mount_target << "'";
    if (rmdir(mount_target.c_str()) != 0)
      LOG(WARNING) << "Failed to remove directory '" << mount_target
        << "' (errno=" << errno << ")";
    return false;
  }

  if (mount_path)
    *mount_path = mount_target;
  return true;
}

bool DiskManager::Unmount(const std::string& device_path,
    const std::vector<std::string>& options) const {

  if (device_path.empty()) {
    LOG(ERROR) << "Invalid device path.";
    return false;
  }

  // Obtain the mount paths from /proc/mounts, instead of using
  // udev_enumerate_scan_devices, as the device node may no longer exist after
  // device removal.
  std::vector<std::string> mount_paths =
    UdevDevice::GetMountPaths(device_path);
  if (mount_paths.empty()) {
    LOG(ERROR) << "'" << device_path << "' is not mounted.";
    return false;
  }

  int unmount_flags;
  if (!ExtractUnmountOptions(options, &unmount_flags)) {
    LOG(ERROR) << "Invalid unmount options.";
    return false;
  }

  bool unmount_ok = true;
  for (std::vector<std::string>::const_iterator
      path_iterator = mount_paths.begin();
      path_iterator != mount_paths.end(); ++path_iterator) {
    if (umount2(path_iterator->c_str(), unmount_flags) == 0) {
      LOG(INFO) << "Unmounted '" << device_path << "' from '"
        << *path_iterator << "'";
    } else {
      LOG(ERROR) << "Failed to unmount '" << device_path
        << "' from '" << *path_iterator << "' (errno=" << errno << ")";
      unmount_ok = false;
    }

    if (rmdir(path_iterator->c_str()) != 0)
      LOG(WARNING) << "Failed to remove directory '" << *path_iterator
        << "' (errno=" << errno << ")";
  }
  return unmount_ok;
}

} // namespace cros_disks
