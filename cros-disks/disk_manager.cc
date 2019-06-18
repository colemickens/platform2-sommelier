// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cros-disks/disk_manager.h"

#include <errno.h>
#include <inttypes.h>
#include <libudev.h>
#include <string.h>
#include <sys/mount.h>
#include <time.h>

#include <memory>
#include <utility>

#include <base/bind.h>
#include <base/logging.h>
#include <base/stl_util.h>
#include <base/strings/string_piece.h>
#include <base/strings/string_util.h>
#include <base/strings/stringprintf.h>
#include <base/time/time.h>

#include "cros-disks/device_ejector.h"
#include "cros-disks/disk_monitor.h"
#include "cros-disks/exfat_mounter.h"
#include "cros-disks/filesystem.h"
#include "cros-disks/metrics.h"
#include "cros-disks/mount_options.h"
#include "cros-disks/ntfs_mounter.h"
#include "cros-disks/platform.h"
#include "cros-disks/system_mounter.h"

namespace cros_disks {

DiskManager::DiskManager(const std::string& mount_root,
                         Platform* platform,
                         Metrics* metrics,
                         brillo::ProcessReaper* process_reaper,
                         DiskMonitor* disk_monitor,
                         DeviceEjector* device_ejector)
    : MountManager(mount_root, platform, metrics, process_reaper),
      disk_monitor_(disk_monitor),
      device_ejector_(device_ejector),
      eject_device_on_unmount_(true) {}

DiskManager::~DiskManager() {
  UnmountAll();
}

bool DiskManager::Initialize() {
  RegisterDefaultFilesystems();
  return MountManager::Initialize();
}

const Filesystem* DiskManager::GetFilesystem(
    const std::string& filesystem_type) const {
  std::map<std::string, Filesystem>::const_iterator filesystem_iterator =
      filesystems_.find(filesystem_type);
  if (filesystem_iterator == filesystems_.end())
    return nullptr;

  return &filesystem_iterator->second;
}

void DiskManager::RegisterDefaultFilesystems() {
  // TODO(benchan): Perhaps these settings can be read from a config file.
  Filesystem vfat_fs("vfat");
  vfat_fs.accepts_user_and_group_id = true;
  vfat_fs.extra_mount_options = {MountOptions::kOptionDirSync,
                                 MountOptions::kOptionFlush, "shortname=mixed",
                                 MountOptions::kOptionUtf8};
  RegisterFilesystem(vfat_fs);

  Filesystem exfat_fs("exfat");
  exfat_fs.mounter_type = ExFATMounter::kMounterType;
  exfat_fs.accepts_user_and_group_id = true;
  exfat_fs.extra_mount_options = {MountOptions::kOptionDirSync};
  RegisterFilesystem(exfat_fs);

  Filesystem ntfs_fs("ntfs");
  ntfs_fs.mounter_type = NTFSMounter::kMounterType;
  ntfs_fs.accepts_user_and_group_id = true;
  ntfs_fs.extra_mount_options = {MountOptions::kOptionDirSync};
  RegisterFilesystem(ntfs_fs);

  Filesystem hfsplus_fs("hfsplus");
  hfsplus_fs.accepts_user_and_group_id = true;
  hfsplus_fs.extra_mount_options = {MountOptions::kOptionDirSync};
  RegisterFilesystem(hfsplus_fs);

  Filesystem iso9660_fs("iso9660");
  iso9660_fs.is_mounted_read_only = true;
  iso9660_fs.accepts_user_and_group_id = true;
  iso9660_fs.extra_mount_options = {MountOptions::kOptionUtf8};
  RegisterFilesystem(iso9660_fs);

  Filesystem udf_fs("udf");
  udf_fs.is_mounted_read_only = true;
  udf_fs.accepts_user_and_group_id = true;
  udf_fs.extra_mount_options = {MountOptions::kOptionUtf8};
  RegisterFilesystem(udf_fs);

  Filesystem ext2_fs("ext2");
  ext2_fs.extra_mount_options = {MountOptions::kOptionDirSync};
  RegisterFilesystem(ext2_fs);

  Filesystem ext3_fs("ext3");
  ext3_fs.extra_mount_options = {MountOptions::kOptionDirSync};
  RegisterFilesystem(ext3_fs);

  Filesystem ext4_fs("ext4");
  ext4_fs.extra_mount_options = {MountOptions::kOptionDirSync};
  RegisterFilesystem(ext4_fs);
}

void DiskManager::RegisterFilesystem(const Filesystem& filesystem) {
  filesystems_.emplace(filesystem.type, filesystem);
}

std::unique_ptr<MounterCompat> DiskManager::CreateMounter(
    const Disk& disk,
    const Filesystem& filesystem,
    const std::string& target_path,
    const std::vector<std::string>& options) const {
  const std::vector<std::string>& extra_options =
      filesystem.extra_mount_options;
  std::vector<std::string> extended_options;
  extended_options.reserve(options.size() + extra_options.size());
  extended_options.assign(options.begin(), options.end());
  extended_options.insert(extended_options.end(), extra_options.begin(),
                          extra_options.end());

  if (filesystem.type == "vfat") {
    // FAT32 stores times as local time instead of UTC. By default, the vfat
    // kernel module will use the kernel's time zone, which is set using
    // settimeofday(), to interpret time stamps as local time. However, time
    // zones are complicated and generally a user-space concern in modern Linux.
    // The man page for {get,set}timeofday comments that the |timezone| fields
    // of these functions is obsolete. Chrome OS doesn't appear to set these
    // either. Instead, we pass the time offset explicitly as a mount option so
    // that the user can see file time stamps as local time. This mirrors what
    // the user will see in other operating systems.
    // TODO(amistry): Consider moving this code into a FATMounter.
    time_t now = base::Time::Now().ToTimeT();
    struct tm timestruct;
    // The time zone might have changed since cros-disks was started. Force a
    // re-read of the time zone to ensure the local time is what the user
    // expects.
    tzset();
    localtime_r(&now, &timestruct);
    // tm_gmtoff is a glibc extension.
    int64_t offset_minutes = static_cast<int64_t>(timestruct.tm_gmtoff) / 60;
    extended_options.push_back(
        base::StringPrintf("time_offset=%" PRId64, offset_minutes));
  }

  std::string default_user_id, default_group_id;
  if (filesystem.accepts_user_and_group_id) {
    default_user_id = base::StringPrintf("%d", platform()->mount_user_id());
    default_group_id = base::StringPrintf("%d", platform()->mount_group_id());
  }

  MountOptions mount_options;
  mount_options.WhitelistOption(MountOptions::kOptionNoSymFollow);
  mount_options.Initialize(extended_options,
                           filesystem.accepts_user_and_group_id,
                           default_user_id, default_group_id);

  if (filesystem.is_mounted_read_only || disk.is_read_only ||
      disk.IsOpticalDisk()) {
    mount_options.SetReadOnlyOption();
  }

  if (filesystem.mounter_type.empty())
    return std::make_unique<MounterCompat>(
        std::make_unique<SystemMounter>(filesystem.mount_type, platform()),
        disk.device_file, base::FilePath(target_path), mount_options);

  if (filesystem.mounter_type == ExFATMounter::kMounterType)
    return std::make_unique<ExFATMounter>(disk.device_file, target_path,
                                          filesystem.mount_type, mount_options,
                                          platform(), process_reaper());

  if (filesystem.mounter_type == NTFSMounter::kMounterType)
    return std::make_unique<NTFSMounter>(disk.device_file, target_path,
                                         filesystem.mount_type, mount_options,
                                         platform(), process_reaper());

  LOG(FATAL) << "Invalid mounter type '" << filesystem.mounter_type << "'";
  return nullptr;
}

bool DiskManager::CanMount(const std::string& source_path) const {
  // The following paths can be mounted:
  //     /sys/...
  //     /devices/...
  //     /dev/...
  return base::StartsWith(source_path, "/sys/", base::CompareCase::SENSITIVE) ||
         base::StartsWith(source_path, "/devices/",
                          base::CompareCase::SENSITIVE) ||
         base::StartsWith(source_path, "/dev/", base::CompareCase::SENSITIVE);
}

MountErrorType DiskManager::DoMount(const std::string& source_path,
                                    const std::string& filesystem_type,
                                    const std::vector<std::string>& options,
                                    const std::string& mount_path,
                                    MountOptions* applied_options) {
  CHECK(!source_path.empty()) << "Invalid source path argument";
  CHECK(!mount_path.empty()) << "Invalid mount path argument";

  Disk disk;
  if (!disk_monitor_->GetDiskByDevicePath(base::FilePath(source_path), &disk)) {
    LOG(ERROR) << "'" << source_path << "' is not a valid device.";
    return MOUNT_ERROR_INVALID_DEVICE_PATH;
  }

  if (disk.is_on_boot_device) {
    LOG(ERROR) << "'" << source_path
               << "' is on boot device and not allowed to mount.";
    return MOUNT_ERROR_INVALID_DEVICE_PATH;
  }

  if (disk.device_file.empty()) {
    LOG(ERROR) << "'" << source_path << "' does not have a device file";
    return MOUNT_ERROR_INVALID_DEVICE_PATH;
  }

  std::string device_filesystem_type =
      filesystem_type.empty() ? disk.filesystem_type : filesystem_type;
  metrics()->RecordDeviceMediaType(disk.media_type);
  metrics()->RecordFilesystemType(device_filesystem_type);
  if (device_filesystem_type.empty()) {
    LOG(ERROR) << "Failed to determine the file system type of device '"
               << source_path << "'";
    return MOUNT_ERROR_UNKNOWN_FILESYSTEM;
  }

  const Filesystem* filesystem = GetFilesystem(device_filesystem_type);
  if (filesystem == nullptr) {
    LOG(ERROR) << "File system type '" << device_filesystem_type
               << "' on device '" << source_path << "' is not supported";
    return MOUNT_ERROR_UNSUPPORTED_FILESYSTEM;
  }

  std::unique_ptr<MounterCompat> mounter(
      CreateMounter(disk, *filesystem, mount_path, options));
  CHECK(mounter) << "Failed to create a mounter";

  MountErrorType error_type = mounter->Mount();
  if (error_type != MOUNT_ERROR_NONE) {
    // Try to mount the filesystem read-only if mounting it read-write failed.
    if (!mounter->mount_options().IsReadOnlyOptionSet()) {
      LOG(INFO) << "Trying to mount '" << source_path << "' read-only";
      std::vector<std::string> ro_options = options;
      ro_options.push_back("ro");
      mounter = CreateMounter(disk, *filesystem, mount_path, ro_options);
      CHECK(mounter) << "Failed to create a 'ro' mounter";
      error_type = mounter->Mount();
    }
  }

  if (error_type == MOUNT_ERROR_NONE) {
    ScheduleEjectOnUnmount(mount_path, disk);
  }
  *applied_options = mounter->mount_options();

  return error_type;
}

MountErrorType DiskManager::DoUnmount(const std::string& path,
                                      const std::vector<std::string>& options) {
  CHECK(!path.empty()) << "Invalid path argument";

  int unmount_flags;
  if (!ExtractUnmountOptions(options, &unmount_flags)) {
    LOG(ERROR) << "Invalid unmount options";
    return MOUNT_ERROR_INVALID_UNMOUNT_OPTIONS;
  }

  // The USB or SD drive on some system may be powered off after the system
  // goes into the S3 suspend state. To avoid leaving a mount point in a stale
  // state while its associated physical drive is gone, the cros-disks client
  // on the Chrome side unmounts all the mount points it manages before the
  // system goes into suspend. However, an ongoing filesystem access may keep a
  // mount point busy, which is beyond the control of Chrome or cros-disks. We
  // used to force umount the mount points but that has become undesirable. For
  // instance, when force unmounting a mount point backed by a FUSE process,
  // umount2() reports an error and the mount point is left in a half-broken
  // state. Lazy unmount is preferred over force unmount under such condition
  // (although it still doesn't necessarily guarantee a clean unmount if the
  // filesystem access doesn't finish before the USB or SD drive is powered
  // off). To better handle this kind of situation, we first try performing a
  // normal unmount. If that fails with errno == EBUSY, we retry with a lazy
  // unmount before giving up and reporting an error.
  bool unmount_failed = (umount2(path.c_str(), unmount_flags) != 0);
  if (unmount_failed && errno == EBUSY) {
    LOG(ERROR) << "Failed to unmount '" << path
               << "' as it is busy; retry with lazy unmount";
    unmount_flags |= MNT_DETACH;
    unmount_failed = (umount2(path.c_str(), unmount_flags) != 0);
  }
  if (unmount_failed) {
    PLOG(ERROR) << "Failed to unmount '" << path << "'";
    // TODO(benchan): Extract error from low-level unmount operation.
    return MOUNT_ERROR_UNKNOWN;
  }

  EjectDeviceOfMountPath(path);

  return MOUNT_ERROR_NONE;
}

std::string DiskManager::SuggestMountPath(
    const std::string& source_path) const {
  Disk disk;
  disk_monitor_->GetDiskByDevicePath(base::FilePath(source_path), &disk);
  // If GetDiskByDevicePath fails, disk.GetPresentationName() returns
  // the fallback presentation name.
  return std::string(mount_root()) + "/" + disk.GetPresentationName();
}

bool DiskManager::ShouldReserveMountPathOnError(
    MountErrorType error_type) const {
  return error_type == MOUNT_ERROR_UNKNOWN_FILESYSTEM ||
         error_type == MOUNT_ERROR_UNSUPPORTED_FILESYSTEM;
}

bool DiskManager::ScheduleEjectOnUnmount(const std::string& mount_path,
                                         const Disk& disk) {
  if (!disk.IsOpticalDisk())
    return false;

  devices_to_eject_on_unmount_[mount_path] = disk;
  return true;
}

bool DiskManager::EjectDeviceOfMountPath(const std::string& mount_path) {
  auto it = devices_to_eject_on_unmount_.find(mount_path);
  if (it == devices_to_eject_on_unmount_.end())
    return false;

  auto disk = it->second;
  devices_to_eject_on_unmount_.erase(it);

  if (eject_device_on_unmount_) {
    return device_ejector_->Eject(disk.device_file);
  }
  return false;
}

bool DiskManager::UnmountAll() {
  // UnmountAll() is called when a user session ends. We do not want to eject
  // devices in that situation and thus set |eject_device_on_unmount_| to
  // false temporarily to prevent devices from being ejected upon unmount.
  eject_device_on_unmount_ = false;
  bool all_unmounted = MountManager::UnmountAll();
  eject_device_on_unmount_ = true;
  return all_unmounted;
}

}  // namespace cros_disks
