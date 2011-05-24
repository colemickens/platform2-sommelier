// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <errno.h>
#include <libudev.h>
#include <pwd.h>
#include <sys/mount.h>
#include <unistd.h>

#include <algorithm>
#include <fstream>
#include <sstream>
#include <vector>

#include <base/logging.h>
#include <base/memory/scoped_ptr.h>
#include <base/string_number_conversions.h>
#include <base/string_split.h>
#include <base/string_util.h>

#include "cros-disks/disk.h"
#include "cros-disks/disk-manager.h"
#include "cros-disks/udev-device.h"

namespace cros_disks {

// TODO(benchan): Refactor DiskManager to move functionalities like
// processing mount/unmount options into separate classes.

// TODO(benchan): Infer the current user/group from session manager
// instead of hardcoding as chronos
static const char kMountDefaultUser[] = "chronos";
static const char kMountOptionReadWrite[] = "rw";
static const char kMountOptionNoDev[] = "nodev";
static const char kMountOptionNoExec[] = "noexec";
static const char kMountOptionNoSuid[] = "nosuid";
static const char kUnmountOptionForce[] = "force";
static const char kMountRootPrefix[] = "/media/";
static const char kFallbackMountPath[] = "/media/disk";
static const unsigned kMaxNumMountTrials = 10000;
static const unsigned kFallbackPasswordBufferSize = 16384;
static const char* const kFilesystemsWithMountUserAndGroupId[] = {
  "fat", "vfat", "msdos", "ntfs",
  "iso9660", "udf",
  "hfs", "hfsplus", "hpfs",
  NULL
};

DiskManager::DiskManager()
    : udev_(udev_new()),
      udev_monitor_fd_(0),
      blkid_cache_(NULL) {

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
    udev_list_entry_foreach(property_list_entry, property_list) {
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
  LOG(INFO) << "   Syspath: " << udev_device_get_syspath(dev);
  LOG(INFO) << "   Node: " << udev_device_get_devnode(dev);
  LOG(INFO) << "   Subsystem: " << udev_device_get_subsystem(dev);
  LOG(INFO) << "   Devtype: " << udev_device_get_devtype(dev);
  LOG(INFO) << "   Action: " << udev_device_get_action(dev);

  bool report_changes = false;
  // Ignore boot device or virtual device changes, which should not be
  // sent to the Chrome browser.
  UdevDevice udev(dev);
  if (!udev.IsOnBootDevice() && !udev.IsVirtual()) {
    *device_path = udev_device_get_syspath(dev);
    *action = udev_device_get_action(dev);
    report_changes = true;
  }

  udev_device_unref(dev);
  return report_changes;
}

bool DiskManager::GetDiskByDevicePath(const std::string& device_path,
    Disk *disk) const {
  if (device_path.empty())
    return false;

  bool is_sys_path = StartsWithASCII(device_path, "/sys/", true);
  bool is_dev_path = StartsWithASCII(device_path, "/devices/", true);
  bool is_dev_file = StartsWithASCII(device_path, "/dev/", true);
  bool disk_found = false;

  struct udev_enumerate *enumerate = udev_enumerate_new(udev_);
  udev_enumerate_add_match_subsystem(enumerate, "block");
  udev_enumerate_scan_devices(enumerate);

  struct udev_list_entry *device_list, *device_list_entry;
  device_list = udev_enumerate_get_list_entry(enumerate);
  udev_list_entry_foreach(device_list_entry, device_list) {
    const char *sys_path = udev_list_entry_get_name(device_list_entry);
    udev_device *dev = udev_device_new_from_syspath(udev_, sys_path);
    const char *dev_path = udev_device_get_devpath(dev);
    const char *dev_file = udev_device_get_devnode(dev);
    if ((is_sys_path && strcmp(sys_path, device_path.c_str()) == 0) ||
        (is_dev_path && strcmp(dev_path, device_path.c_str()) == 0) ||
        (is_dev_file && dev_file != NULL
         && strcmp(dev_file, device_path.c_str()) == 0)) {
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

std::string DiskManager::GetDeviceFileFromCache(
    const std::string& device_path) const {
  std::map<std::string, std::string>::const_iterator map_iterator =
    device_file_map_.find(device_path);
  return (map_iterator != device_file_map_.end()) ? map_iterator->second : "";
}

std::string DiskManager::GetFilesystemTypeOfDevice(
    const std::string& device_path) {
  std::string filesystem_type;
  if (blkid_cache_ != NULL || blkid_get_cache(&blkid_cache_, NULL) == 0) {
    blkid_dev dev =
      blkid_get_dev(blkid_cache_, device_path.c_str(), BLKID_DEV_NORMAL);
    if (dev) {
      const char *type = blkid_get_tag_value(blkid_cache_, "TYPE",
          device_path.c_str());
      if (type) {
        filesystem_type = type;
      }
    }
  }
  return filesystem_type;
}

std::vector<std::string> DiskManager::GetFilesystems() const {
  std::ifstream stream("/proc/filesystems");
  if (stream.is_open()) {
    return GetFilesystems(stream);
  }
  LOG(ERROR) << "Unable to read /proc/filesystems";
  return std::vector<std::string>();
}

bool DiskManager::GetUserAndGroupId(const std::string& username,
    uid_t *uid, gid_t *gid) const {
  long buffer_size = sysconf(_SC_GETPW_R_SIZE_MAX);
  if (buffer_size <= 0) {
    buffer_size = kFallbackPasswordBufferSize;
  }

  struct passwd password_buffer, *password_buffer_ptr = NULL;
  scoped_array<char> buffer(new char[buffer_size]);
  getpwnam_r(username.c_str(), &password_buffer, buffer.get(), buffer_size,
      &password_buffer_ptr);
  if (password_buffer_ptr == NULL) {
    return false;
  }

  if (uid) {
    *uid = password_buffer.pw_uid;
  }
  if (gid) {
    *gid = password_buffer.pw_gid;
  }
  return true;
}

std::vector<std::string> DiskManager::GetFilesystems(
    std::istream& stream) const {
  std::vector<std::string> filesystems;
  std::string line;
  while (std::getline(stream, line)) {
    std::vector<std::string> tokens;
    base::SplitString(line, '\t', &tokens);
    if (tokens.size() >= 2) {
      if (tokens[0] != "nodev")
        filesystems.push_back(tokens[1]);
    }
  }
  return filesystems;
}

std::string DiskManager::GetMountDirectoryName(const Disk& disk) const {
  std::string mount_path;
  if (!disk.label().empty()) {
    mount_path = disk.label();
    std::replace(mount_path.begin(), mount_path.end(), '/', '_');
    mount_path = kMountRootPrefix + mount_path;
  } else if (!disk.uuid().empty()) {
    mount_path = kMountRootPrefix + disk.uuid();
  } else {
    mount_path = kFallbackMountPath;
  }
  return mount_path;
}

std::string DiskManager::CreateMountDirectory(const Disk& disk,
    const std::string& target_path) const {
  // Construct the mount path as follows:
  // (1) If a target path is specified, use it.
  // (2) If the disk has a non-empty label,
  //     use /media/<label> as the target path.
  //     Note: any forward slash '/' in the label is replaced
  //     with a underscore '_'.
  // (3) If the disk has a non-empty uuid,
  //     use /media/<uuid> as the target path.
  // (4) Otherwise, use /media/disk as the target path.
  // (5) If the target path obtained in (2)-(4) cannot
  //     be created, try suffixing it with a number.

  if (!target_path.empty()) {
    if (mkdir(target_path.c_str(), S_IRWXU) != 0) {
      LOG(ERROR) << "Failed to create directory '" << target_path
        << "' (errno=" << errno << ")";
      return std::string();
    }
    return target_path;
  }

  unsigned suffix = 1;
  std::string mount_path = GetMountDirectoryName(disk);
  std::string mount_path_prefix = mount_path + "_";
  while (mkdir(mount_path.c_str(), S_IRWXU) != 0) {
    if (suffix == kMaxNumMountTrials) {
      mount_path.clear();
      break;
    }
    mount_path = mount_path_prefix + base::UintToString(suffix);
    suffix++;
  }

  return mount_path;
}

std::vector<std::string> DiskManager::SanitizeMountOptions(
    const std::vector<std::string>& options, const Disk& disk) const {
  std::vector<std::string> sanitized_options;
  sanitized_options.reserve(options.size());

  for (std::vector<std::string>::const_iterator
      option_iterator = options.begin(); option_iterator != options.end();
      ++option_iterator) {
    const std::string& option = *option_iterator;
    if (option == kMountOptionReadWrite) {
      if (disk.is_read_only() || disk.is_optical_disk())
        continue;
    }
    sanitized_options.push_back(option);
  }
  return sanitized_options;
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
    if (option == kMountOptionReadWrite) {
      *mount_flags &= ~(unsigned long)MS_RDONLY;
    } else if (option == kMountOptionNoDev) {
      *mount_flags |= MS_NODEV;
    } else if (option == kMountOptionNoExec) {
      *mount_flags |= MS_NOEXEC;
    } else if (option == kMountOptionNoSuid) {
      *mount_flags |= MS_NOSUID;
    } else {
      LOG(ERROR) << "Got unsupported mount option: " << option;
      return false;
    }
  }
  return true;
}

bool DiskManager::ExtractUnmountOptions(
    const std::vector<std::string>& options, int *unmount_flags) const {
  if (unmount_flags == NULL)
    return false;

  *unmount_flags = 0;
  for (std::vector<std::string>::const_iterator
      option_iterator = options.begin(); option_iterator != options.end();
      ++option_iterator) {
    const std::string& option = *option_iterator;
    if (option == kUnmountOptionForce) {
      *unmount_flags |= MNT_FORCE;
    } else {
      LOG(ERROR) << "Got unsupported unmount option: " << option;
      return false;
    }
  }
  return true;
}

bool DiskManager::IsMountUserAndGroupIdSupportedByFilesystem(
    const std::string& filesystem_type) const {
  for (const char* const* filesystem = kFilesystemsWithMountUserAndGroupId;
      *filesystem; ++filesystem) {
    if (strcmp(filesystem_type.c_str(), *filesystem) == 0) {
      return true;
    }
  }
  return false;
}

std::string DiskManager::ModifyMountOptionsForFilesystem(
    const std::string& filesystem_type, const std::string& options) const {
  std::stringstream extended_options(options);
  if (IsMountUserAndGroupIdSupportedByFilesystem(filesystem_type)) {
    uid_t uid;
    gid_t gid;
    if (GetUserAndGroupId(kMountDefaultUser, &uid, &gid)) {
      if (!options.empty()) {
        extended_options << ",";
      }
      extended_options << "uid=" << uid << ",gid=" << gid;
    }
  }
  return extended_options.str();
}

bool DiskManager::DoMount(const std::string& device_file,
    const std::string& mount_path, const std::string& filesystem_type,
    unsigned long mount_flags, const std::string& mount_data) const {
  bool mounted = false;
  if (mount(device_file.c_str(), mount_path.c_str(),
        filesystem_type.c_str(), mount_flags,
        mount_data.c_str()) == 0) {
    mounted = true;
  } else {
    // Try to mount the disk as read-only if mounting as read-write failed.
    unsigned long read_only_mount_flags = mount_flags | MS_RDONLY;
    if (read_only_mount_flags != mount_flags) {
      if (mount(device_file.c_str(), mount_path.c_str(),
            filesystem_type.c_str(), read_only_mount_flags,
            mount_data.c_str()) == 0) {
        mounted = true;
      }
    }
  }

  if (mounted) {
    LOG(INFO) << "Mounted '" << device_file << "' to '" << mount_path
      << "' as " << filesystem_type << " with options '" << mount_data
      << "'";
  } else {
    LOG(INFO) << "Failed to mount '" << device_file << "' to '" << mount_path
      << "' as " << filesystem_type << " with options '" << mount_data
      << "' (errno=" << errno << ")";
  }
  return mounted;
}

bool DiskManager::Mount(const std::string& device_path,
    const std::string& filesystem_type,
    const std::vector<std::string>& options,
    std::string *mount_path) {

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

  const std::string& device_file = disk.device_file();
  if (device_file.empty()) {
    LOG(ERROR) << "'" << device_path << "' does not have a device file.";
    return false;
  }

  // Cache the mapping from device sysfs path to device file.
  // After a device is removed, the sysfs path may no longer exist,
  // so this cache helps Unmount to search for a mounted device file
  // based on a removed sysfs path.
  device_file_map_[disk.native_path()] = device_file;

  unsigned long mount_flags = 0;
  std::string mount_data;
  std::vector<std::string> sanitized_options =
    SanitizeMountOptions(options, disk);
  if (!ExtractMountOptions(sanitized_options, &mount_flags, &mount_data)) {
    LOG(ERROR) << "Invalid mount options.";
    return false;
  }

  std::string mount_target = mount_path ? *mount_path : std::string();
  mount_target = CreateMountDirectory(disk, mount_target);
  if (mount_target.empty()) {
    LOG(ERROR) << "Failed to create mount directory for '"
      << device_path << "'";
    return false;
  }

  // If no explicit filesystem type is given, try to determine it via blkid.
  std::string device_filesystem_type = filesystem_type.empty() ?
    GetFilesystemTypeOfDevice(device_file) : filesystem_type;

  // If the filesystem type is not determined, try iterate through the types
  // listed in /proc/filesystems.
  std::vector<std::string> filesystems;
  if (device_filesystem_type.empty()) {
    filesystems = GetFilesystems();
  } else {
    filesystems.push_back(device_filesystem_type);
  }

  bool mounted = false;
  for (std::vector<std::string>::const_iterator
      filesystem_iterator(filesystems.begin());
      filesystem_iterator != filesystems.end(); ++filesystem_iterator) {
    const std::string& filesystem_type = *filesystem_iterator;
    const std::string mount_options =
      ModifyMountOptionsForFilesystem(filesystem_type, mount_data);
    mounted = DoMount(device_file, mount_target, filesystem_type, mount_flags,
        mount_options);
    if (mounted)
      break;
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
    const std::vector<std::string>& options) {

  if (device_path.empty()) {
    LOG(ERROR) << "Invalid device path.";
    return false;
  }

  std::string device_file = GetDeviceFileFromCache(device_path);
  if (device_file.empty()) {
    Disk disk;
    if (GetDiskByDevicePath(device_path, &disk)) {
      device_file = disk.device_file();
    }
  }

  if (device_file.empty()) {
    LOG(ERROR) << "Could not find the device file for '" << device_path << "'";
    return false;
  }

  // Obtain the mount paths from /proc/mounts, instead of using
  // udev_enumerate_scan_devices, as the device node may no longer exist after
  // device removal.
  std::vector<std::string> mount_paths =
    UdevDevice::GetMountPaths(device_file);
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

  device_file_map_.erase(device_path);

  return unmount_ok;
}

}  // namespace cros_disks
