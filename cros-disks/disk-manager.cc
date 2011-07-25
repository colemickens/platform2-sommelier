// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cros-disks/disk-manager.h"

#include <libudev.h>
#include <string.h>
#include <sys/mount.h>

#include <base/logging.h>
#include <base/memory/scoped_ptr.h>
#include <base/string_util.h>

#include "cros-disks/disk.h"
#include "cros-disks/external-mounter.h"
#include "cros-disks/filesystem.h"
#include "cros-disks/mount-options.h"
#include "cros-disks/platform.h"
#include "cros-disks/system-mounter.h"
#include "cros-disks/udev-device.h"

using std::map;
using std::set;
using std::string;
using std::vector;

namespace cros_disks {

static const char kBlockSubsystem[] = "block";
static const char kScsiSubsystem[] = "scsi";
static const char kScsiDevice[] = "scsi_device";
// TODO(benchan): Infer the current user/group from session manager
// instead of hardcoding as chronos
static const char kMountDefaultUser[] = "chronos";
static const char kUnmountOptionForce[] = "force";
static const char kMountRootPrefix[] = "/media/";
static const char kUdevAddAction[] = "add";
static const char kUdevChangeAction[] = "change";
static const char kUdevRemoveAction[] = "remove";
static const unsigned kMaxNumMountTrials = 10000;

DiskManager::DiskManager(Platform* platform)
    : platform_(platform),
      udev_(udev_new()),
      udev_monitor_fd_(0),
      blkid_cache_(NULL),
      experimental_features_enabled_(false) {
  CHECK(platform_) << "Invalid platform object";
  CHECK(udev_) << "Failed to initialize udev";
  udev_monitor_ = udev_monitor_new_from_netlink(udev_, "udev");
  CHECK(udev_monitor_) << "Failed to create a udev monitor";
  udev_monitor_filter_add_match_subsystem_devtype(udev_monitor_,
      kBlockSubsystem, NULL);
  udev_monitor_filter_add_match_subsystem_devtype(udev_monitor_,
      kScsiSubsystem, kScsiDevice);
  udev_monitor_enable_receiving(udev_monitor_);
  udev_monitor_fd_ = udev_monitor_get_fd(udev_monitor_);
}

DiskManager::~DiskManager() {
  udev_monitor_unref(udev_monitor_);
  udev_unref(udev_);
}

vector<Disk> DiskManager::EnumerateDisks() const {
  vector<Disk> disks;

  struct udev_enumerate *enumerate = udev_enumerate_new(udev_);
  udev_enumerate_add_match_subsystem(enumerate, kBlockSubsystem);
  udev_enumerate_scan_devices(enumerate);

  struct udev_list_entry *device_list, *device_list_entry;
  device_list = udev_enumerate_get_list_entry(enumerate);
  udev_list_entry_foreach(device_list_entry, device_list) {
    const char *path = udev_list_entry_get_name(device_list_entry);
    udev_device *dev = udev_device_new_from_syspath(udev_, path);
    if (dev == NULL) continue;

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

DeviceEvent::EventType DiskManager::ProcessBlockDeviceEvent(
    const UdevDevice& device, const char *action) {
  DeviceEvent::EventType event_type = DeviceEvent::kIgnored;
  string device_path = device.NativePath();

  bool disk_added = false;
  bool disk_removed = false;
  if (strcmp(action, kUdevAddAction) == 0) {
    disk_added = true;
  } else if (strcmp(action, kUdevRemoveAction) == 0) {
    disk_removed = true;
  } else if (strcmp(action, kUdevChangeAction) == 0) {
    // For removable devices like CD-ROM, an eject request event
    // is treated as disk removal, while a media change event with
    // media available is treated as disk insertion.
    if (device.IsPropertyTrue("DISK_EJECT_REQUEST")) {
      disk_removed = true;
    } else if (device.IsPropertyTrue("DISK_MEDIA_CHANGE")) {
      if (device.IsMediaAvailable()) {
        disk_added = true;
      }
    }
  }

  if (disk_added) {
    set<string>::const_iterator disk_iter =
      disks_detected_.find(device_path);
    if (disk_iter != disks_detected_.end()) {
      // Disk already exists, so remove it and then add it again.
      event_type = DeviceEvent::kDiskAddedAfterRemoved;
    } else {
      disks_detected_.insert(device_path);
      event_type = DeviceEvent::kDiskAdded;
    }
  } else if (disk_removed) {
    disks_detected_.erase(device_path);
    event_type = DeviceEvent::kDiskRemoved;
  }
  return event_type;
}

DeviceEvent::EventType DiskManager::ProcessScsiDeviceEvent(
    const UdevDevice& device, const char *action) {
  DeviceEvent::EventType event_type = DeviceEvent::kIgnored;
  string device_path = device.NativePath();

  set<string>::const_iterator device_iter;
  if (strcmp(action, kUdevAddAction) == 0) {
    device_iter = devices_detected_.find(device_path);
    if (device_iter != devices_detected_.end()) {
      event_type = DeviceEvent::kDeviceScanned;
    } else {
      devices_detected_.insert(device_path);
      event_type = DeviceEvent::kDeviceAdded;
    }
  } else if (strcmp(action, kUdevRemoveAction) == 0) {
    device_iter = devices_detected_.find(device_path);
    if (device_iter != devices_detected_.end()) {
      devices_detected_.erase(device_iter);
      event_type = DeviceEvent::kDeviceRemoved;
    }
  }
  return event_type;
}

bool DiskManager::GetDeviceEvent(DeviceEvent* event) {
  struct udev_device *dev = udev_monitor_receive_device(udev_monitor_);
  CHECK(dev) << "Unknown udev device";
  CHECK(event) << "Invalid device event object";
  LOG(INFO) << "Got Device";
  LOG(INFO) << "   Syspath: " << udev_device_get_syspath(dev);
  LOG(INFO) << "   Node: " << udev_device_get_devnode(dev);
  LOG(INFO) << "   Subsystem: " << udev_device_get_subsystem(dev);
  LOG(INFO) << "   Devtype: " << udev_device_get_devtype(dev);
  LOG(INFO) << "   Action: " << udev_device_get_action(dev);

  const char *sys_path = udev_device_get_syspath(dev);
  const char *subsystem = udev_device_get_subsystem(dev);
  const char *action = udev_device_get_action(dev);
  if (!sys_path || !subsystem || !action) {
    udev_device_unref(dev);
    return false;
  }

  event->device_path = sys_path;
  event->event_type = DeviceEvent::kIgnored;

  // Ignore change events from boot devices, virtual devices,
  // or Chrome OS specific partitions, which should not be sent
  // to the Chrome browser.
  UdevDevice udev(dev);
  if (udev.IsAutoMountable()) {
    // udev_monitor_ only monitors block or scsi device changes, so
    // subsystem is either "block" or "scsi".
    if (strcmp(subsystem, kBlockSubsystem) == 0) {
      event->event_type = ProcessBlockDeviceEvent(udev, action);
    } else {  // strcmp(subsystem, kScsiSubsystem) == 0
      event->event_type = ProcessScsiDeviceEvent(udev, action);
    }
  }

  udev_device_unref(dev);
  return true;
}

bool DiskManager::GetDiskByDevicePath(const string& device_path,
    Disk *disk) const {
  if (device_path.empty())
    return false;

  bool is_sys_path = StartsWithASCII(device_path, "/sys/", true);
  bool is_dev_path = StartsWithASCII(device_path, "/devices/", true);
  bool is_dev_file = StartsWithASCII(device_path, "/dev/", true);
  bool disk_found = false;

  struct udev_enumerate *enumerate = udev_enumerate_new(udev_);
  udev_enumerate_add_match_subsystem(enumerate, kBlockSubsystem);
  udev_enumerate_scan_devices(enumerate);

  struct udev_list_entry *device_list, *device_list_entry;
  device_list = udev_enumerate_get_list_entry(enumerate);
  udev_list_entry_foreach(device_list_entry, device_list) {
    const char *sys_path = udev_list_entry_get_name(device_list_entry);
    udev_device *dev = udev_device_new_from_syspath(udev_, sys_path);
    if (dev == NULL) continue;

    const char *dev_path = udev_device_get_devpath(dev);
    const char *dev_file = udev_device_get_devnode(dev);
    if ((is_sys_path && device_path == sys_path) ||
        (is_dev_path && device_path == dev_path) ||
        (is_dev_file && dev_file && device_path == dev_file)) {
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

string DiskManager::GetDeviceFileFromCache(const string& device_path) const {
  map<string, string>::const_iterator map_iterator =
    device_file_map_.find(device_path);
  return (map_iterator != device_file_map_.end()) ? map_iterator->second : "";
}

const Filesystem* DiskManager::GetFilesystem(
    const std::string& filesystem_type) const {
  map<string, Filesystem>::const_iterator filesystem_iterator =
    filesystems_.find(filesystem_type);
  if (filesystem_iterator == filesystems_.end())
    return NULL;

  if (!experimental_features_enabled_ &&
      filesystem_iterator->second.is_experimental())
    return NULL;

  return &filesystem_iterator->second;
}

string DiskManager::GetFilesystemTypeOfDevice(const string& device_path) {
  string filesystem_type;
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

string DiskManager::CreateMountDirectory(const Disk& disk,
    const string& target_path) const {
  if (target_path.empty()) {
    string mount_path = kMountRootPrefix + disk.GetPresentationName();
    if (platform_->CreateOrReuseEmptyDirectoryWithFallback(&mount_path,
          kMaxNumMountTrials)) {
      return mount_path;
    }
  } else {
    if (platform_->CreateOrReuseEmptyDirectory(target_path)) {
      return target_path;
    }
  }
  return string();
}

bool DiskManager::ExtractUnmountOptions(const vector<string>& options,
    int *unmount_flags) const {
  if (unmount_flags == NULL)
    return false;

  *unmount_flags = 0;
  for (vector<string>::const_iterator
      option_iterator = options.begin(); option_iterator != options.end();
      ++option_iterator) {
    const string& option = *option_iterator;
    if (option == kUnmountOptionForce) {
      *unmount_flags |= MNT_FORCE;
    } else {
      LOG(ERROR) << "Got unsupported unmount option: " << option;
      return false;
    }
  }
  return true;
}

bool DiskManager::Mount(const string& device_path,
    const string& filesystem_type, const vector<string>& options,
    string *mount_path) {
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

  const string& device_file = disk.device_file();
  if (device_file.empty()) {
    LOG(ERROR) << "'" << device_path << "' does not have a device file.";
    return false;
  }

  // Cache the mapping from device sysfs path to device file.
  // After a device is removed, the sysfs path may no longer exist,
  // so this cache helps Unmount to search for a mounted device file
  // based on a removed sysfs path.
  device_file_map_[disk.native_path()] = device_file;

  // If no explicit filesystem type is given, try to determine it via blkid.
  string device_filesystem_type = filesystem_type.empty() ?
    GetFilesystemTypeOfDevice(device_file) : filesystem_type;
  if (device_filesystem_type.empty()) {
    LOG(ERROR) << "Failed to determine the file system type of device '"
      << device_path << "'";
    return false;
  }

  const Filesystem* filesystem = GetFilesystem(device_filesystem_type);
  if (filesystem == NULL) {
    LOG(ERROR) << "File system type '" << device_filesystem_type
      << "' on device '" << device_path << "' is not supported.";
    return false;
  }

  string mount_target = mount_path ? *mount_path : string();
  mount_target = CreateMountDirectory(disk, mount_target);
  if (mount_target.empty()) {
    LOG(ERROR) << "Failed to create mount directory for '"
      << device_path << "'";
    return false;
  }

  scoped_ptr<Mounter> mounter(CreateMounter(disk, *filesystem,
        mount_target, options));
  if (!mounter->Mount()) {
    LOG(ERROR) << "Failed to mount '" << device_path << "' to '"
      << mount_target << "'";
    platform_->RemoveEmptyDirectory(mount_target);
    return false;
  }

  if (mount_path)
    *mount_path = mount_target;
  return true;
}

bool DiskManager::Unmount(const string& device_path,
    const vector<string>& options) {

  if (device_path.empty()) {
    LOG(ERROR) << "Invalid device path.";
    return false;
  }

  string device_file = GetDeviceFileFromCache(device_path);
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
  vector<string> mount_paths =
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
  for (vector<string>::const_iterator
      path_iterator = mount_paths.begin();
      path_iterator != mount_paths.end(); ++path_iterator) {
    if (umount2(path_iterator->c_str(), unmount_flags) == 0) {
      LOG(INFO) << "Unmounted '" << device_path << "' from '"
        << *path_iterator << "'";
    } else {
      PLOG(ERROR) << "Failed to unmount '" << device_path
        << "' from '" << *path_iterator << "'";
      unmount_ok = false;
    }
    platform_->RemoveEmptyDirectory(*path_iterator);
  }

  device_file_map_.erase(device_path);

  return unmount_ok;
}

void DiskManager::RegisterDefaultFilesystems() {
  // TODO(benchan): Perhaps these settings can be read from a config file.
  Filesystem vfat_fs("vfat");
  vfat_fs.set_accepts_user_and_group_id(true);
  vfat_fs.AddExtraMountOption("shortname=mixed");
  vfat_fs.AddExtraMountOption("utf8");
  RegisterFilesystem(vfat_fs);

  Filesystem ntfs_fs("ntfs");
  ntfs_fs.set_mount_type("ntfs-3g");
  ntfs_fs.set_is_experimental(true);
  ntfs_fs.set_is_mounted_read_only(true);
  ntfs_fs.set_accepts_user_and_group_id(true);
  ntfs_fs.set_requires_external_mounter(true);
  RegisterFilesystem(ntfs_fs);

  Filesystem hfsplus_fs("hfsplus");
  hfsplus_fs.set_accepts_user_and_group_id(true);
  RegisterFilesystem(hfsplus_fs);

  Filesystem iso9660_fs("iso9660");
  iso9660_fs.set_is_mounted_read_only(true);
  iso9660_fs.set_accepts_user_and_group_id(true);
  iso9660_fs.AddExtraMountOption("utf8");
  RegisterFilesystem(iso9660_fs);

  Filesystem udf_fs("udf");
  udf_fs.set_is_mounted_read_only(true);
  udf_fs.set_accepts_user_and_group_id(true);
  udf_fs.AddExtraMountOption("utf8");
  RegisterFilesystem(udf_fs);

  Filesystem ext2_fs("ext2");
  RegisterFilesystem(ext2_fs);

  Filesystem ext3_fs("ext3");
  RegisterFilesystem(ext3_fs);

  Filesystem ext4_fs("ext4");
  RegisterFilesystem(ext4_fs);
}

void DiskManager::RegisterFilesystem(const Filesystem& filesystem) {
  filesystems_.insert(std::make_pair(filesystem.type(), filesystem));
}

Mounter* DiskManager::CreateMounter(const Disk& disk,
    const Filesystem& filesystem, const string& target_path,
    const vector<string>& options) const {
  const vector<string>& extra_options =
    filesystem.extra_mount_options();
  vector<string> extended_options;
  extended_options.reserve(options.size() + extra_options.size());
  extended_options.assign(options.begin(), options.end());
  extended_options.insert(extended_options.end(),
      extra_options.begin(), extra_options.end());

  string default_user_id, default_group_id;
  bool set_user_and_group_id = filesystem.accepts_user_and_group_id();
  if (set_user_and_group_id) {
    uid_t uid;
    gid_t gid;
    if (platform_->GetUserAndGroupId(kMountDefaultUser, &uid, &gid)) {
      default_user_id = StringPrintf("%d", uid);
      default_group_id = StringPrintf("%d", gid);
    }
  }

  MountOptions mount_options;
  mount_options.Initialize(extended_options, set_user_and_group_id,
      default_user_id, default_group_id);

  if (filesystem.is_mounted_read_only() ||
      disk.is_read_only() || disk.is_optical_disk()) {
    mount_options.SetReadOnlyOption();
  }

  Mounter* mounter;
  if (filesystem.requires_external_mounter()) {
    mounter = new ExternalMounter(disk.device_file(), target_path,
        filesystem.mount_type(), mount_options);
  } else {
    mounter = new SystemMounter(disk.device_file(), target_path,
        filesystem.mount_type(), mount_options);
  }
  return mounter;
}

}  // namespace cros_disks
