// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cros-disks/disk_monitor.h"

#include <inttypes.h>
#include <libudev.h>
#include <string.h>
#include <sys/mount.h>
#include <time.h>

#include <base/bind.h>
#include <base/logging.h>
#include <base/stl_util.h>
#include <base/strings/string_piece.h>
#include <base/strings/string_util.h>
#include <base/strings/stringprintf.h>
#include <base/time/time.h>

#include "cros-disks/device_ejector.h"
#include "cros-disks/quote.h"
#include "cros-disks/udev_device.h"

namespace cros_disks {
namespace {

const char kBlockSubsystem[] = "block";
const char kMmcSubsystem[] = "mmc";
const char kScsiSubsystem[] = "scsi";
const char kScsiDevice[] = "scsi_device";
const char kUdevAddAction[] = "add";
const char kUdevChangeAction[] = "change";
const char kUdevRemoveAction[] = "remove";
const char kPropertyDiskEjectRequest[] = "DISK_EJECT_REQUEST";
const char kPropertyDiskMediaChange[] = "DISK_MEDIA_CHANGE";

// An EnumerateBlockDevices callback that appends a Disk object, created from
// |dev|, to |disks| if |dev| should not be ignored by cros-disks. Always
// returns true to continue the enumeration in EnumerateBlockDevices.
bool AppendDiskIfNotIgnored(std::vector<Disk>* disks, udev_device* dev) {
  DCHECK(disks);
  DCHECK(dev);

  UdevDevice device(dev);
  if (!device.IsIgnored())
    disks->push_back(device.ToDisk());

  return true;  // Continue the enumeration.
}

// An EnumerateBlockDevices callback that checks if |dev| matches |path|. If
// it's a match, sets |match| to true and |disk| (if not NULL) to a Disk object
// created from |dev|, and returns false to stop the enumeration in
// EnumerateBlockDevices. Otherwise, sets |match| to false, leaves |disk|
// unchanged, and returns true to continue the enumeration in
// EnumerateBlockDevices.
bool MatchDiskByPath(const std::string& path,
                     bool* match,
                     Disk* disk,
                     udev_device* dev) {
  DCHECK(match);
  DCHECK(dev);

  const char* sys_path = udev_device_get_syspath(dev);
  const char* dev_path = udev_device_get_devpath(dev);
  const char* dev_file = udev_device_get_devnode(dev);
  *match = (sys_path && path == sys_path) || (dev_path && path == dev_path) ||
           (dev_file && path == dev_file);
  if (!*match)
    return true;  // Not a match. Continue the enumeration.

  if (disk)
    *disk = UdevDevice(dev).ToDisk();

  return false;  // Match. Stop enumeration.
}

// Logs a device with its properties.
void LogUdevDevice(udev_device* const dev) {
  if (!VLOG_IS_ON(1))
    return;

  // Some device events (eg USB drive removal) result in devnode being null.
  // This is gracefully handled by quote() without crashing.
  VLOG(1) << "   node: " << quote(udev_device_get_devnode(dev));
  VLOG(1) << "   subsystem: " << quote(udev_device_get_subsystem(dev));
  VLOG(1) << "   devtype: " << quote(udev_device_get_devtype(dev));
  VLOG(1) << "   devpath: " << quote(udev_device_get_devpath(dev));
  VLOG(1) << "   sysname: " << quote(udev_device_get_sysname(dev));
  VLOG(1) << "   syspath: " << quote(udev_device_get_syspath(dev));

  if (!VLOG_IS_ON(2))
    return;

  // Log all properties.
  for (udev_list_entry* entry = udev_device_get_properties_list_entry(dev);
       entry; entry = udev_list_entry_get_next(entry)) {
    VLOG(2) << "   " << udev_list_entry_get_name(entry) << ": "
            << quote(udev_list_entry_get_value(entry));
  }
}

}  // namespace

DiskMonitor::DiskMonitor() : udev_(udev_new()), udev_monitor_fd_(0) {
  CHECK(udev_) << "Failed to initialize udev";
  udev_monitor_ = udev_monitor_new_from_netlink(udev_, "udev");
  CHECK(udev_monitor_) << "Failed to create a udev monitor";
  udev_monitor_filter_add_match_subsystem_devtype(udev_monitor_,
                                                  kBlockSubsystem, nullptr);
  udev_monitor_filter_add_match_subsystem_devtype(udev_monitor_, kMmcSubsystem,
                                                  nullptr);
  udev_monitor_filter_add_match_subsystem_devtype(udev_monitor_, kScsiSubsystem,
                                                  kScsiDevice);
  udev_monitor_enable_receiving(udev_monitor_);
  udev_monitor_fd_ = udev_monitor_get_fd(udev_monitor_);
}

DiskMonitor::~DiskMonitor() {
  udev_monitor_unref(udev_monitor_);
  udev_unref(udev_);
}

bool DiskMonitor::Initialize() {
  // Since there are no udev add events for the devices that already exist
  // when the disk manager starts, emulate udev add events for these devices
  // to correctly populate |disks_detected_|.
  EnumerateBlockDevices(base::Bind(&DiskMonitor::EmulateAddBlockDeviceEvent,
                                   base::Unretained(this)));
  return true;
}

bool DiskMonitor::EmulateAddBlockDeviceEvent(udev_device* dev) {
  DCHECK(dev);
  DeviceEventList events;
  ProcessBlockDeviceEvents(dev, kUdevAddAction, &events);
  LOG(INFO) << "Emulated action 'add' on device "
            << quote(udev_device_get_sysname(dev));
  LogUdevDevice(dev);
  return true;  // Continue the enumeration.
}

std::vector<Disk> DiskMonitor::EnumerateDisks() const {
  std::vector<Disk> disks;
  EnumerateBlockDevices(
      base::Bind(&AppendDiskIfNotIgnored, base::Unretained(&disks)));
  return disks;
}

void DiskMonitor::EnumerateBlockDevices(
    const base::Callback<bool(udev_device* dev)>& callback) const {
  udev_enumerate* enumerate = udev_enumerate_new(udev_);
  udev_enumerate_add_match_subsystem(enumerate, kBlockSubsystem);
  udev_enumerate_scan_devices(enumerate);

  udev_list_entry *device_list, *device_list_entry;
  device_list = udev_enumerate_get_list_entry(enumerate);
  udev_list_entry_foreach(device_list_entry, device_list) {
    const char* path = udev_list_entry_get_name(device_list_entry);
    udev_device* dev = udev_device_new_from_syspath(udev_, path);
    if (dev == nullptr)
      continue;

    VLOG(1) << "Found device " << quote(udev_device_get_sysname(dev));
    LogUdevDevice(dev);

    bool continue_enumeration = callback.Run(dev);
    udev_device_unref(dev);
    if (!continue_enumeration)
      break;
  }
  udev_enumerate_unref(enumerate);
}

void DiskMonitor::ProcessBlockDeviceEvents(udev_device* dev,
                                           const char* action,
                                           DeviceEventList* events) {
  UdevDevice device(dev);
  if (device.IsIgnored())
    return;

  bool disk_added = false;
  bool disk_removed = false;
  bool child_disk_removed = false;
  if (strcmp(action, kUdevAddAction) == 0) {
    disk_added = true;
  } else if (strcmp(action, kUdevRemoveAction) == 0) {
    disk_removed = true;
  } else if (strcmp(action, kUdevChangeAction) == 0) {
    // For removable devices like CD-ROM, an eject request event
    // is treated as disk removal, while a media change event with
    // media available is treated as disk insertion.
    if (device.IsPropertyTrue(kPropertyDiskEjectRequest)) {
      disk_removed = true;
    } else if (device.IsPropertyTrue(kPropertyDiskMediaChange)) {
      if (device.IsMediaAvailable()) {
        disk_added = true;
      } else {
        child_disk_removed = true;
      }
    }
  }

  std::string device_path = device.NativePath();
  if (disk_added) {
    if (device.IsAutoMountable()) {
      if (base::ContainsKey(disks_detected_, device_path)) {
        // Disk already exists, so remove it and then add it again.
        events->push_back(DeviceEvent(DeviceEvent::kDiskRemoved, device_path));
      } else {
        disks_detected_[device_path] = {};

        // Add the disk as a child of its parent if the parent is already
        // added to |disks_detected_|.
        udev_device* parent = udev_device_get_parent(dev);
        if (parent) {
          std::string parent_device_path = UdevDevice(parent).NativePath();
          if (base::ContainsKey(disks_detected_, parent_device_path)) {
            disks_detected_[parent_device_path].insert(device_path);
          }
        }
      }
      events->push_back(DeviceEvent(DeviceEvent::kDiskAdded, device_path));
    }
  } else if (disk_removed) {
    disks_detected_.erase(device_path);
    events->push_back(DeviceEvent(DeviceEvent::kDiskRemoved, device_path));
  } else if (child_disk_removed) {
    bool no_child_disks_found = true;
    if (base::ContainsKey(disks_detected_, device_path)) {
      auto& child_disks = disks_detected_[device_path];
      no_child_disks_found = child_disks.empty();
      for (const auto& child_disk : child_disks) {
        events->push_back(DeviceEvent(DeviceEvent::kDiskRemoved, child_disk));
      }
    }
    // When the device contains a full-disk partition, there are no child disks.
    // Remove the device instead.
    if (no_child_disks_found)
      events->push_back(DeviceEvent(DeviceEvent::kDiskRemoved, device_path));
  }
}

void DiskMonitor::ProcessMmcOrScsiDeviceEvents(udev_device* dev,
                                               const char* action,
                                               DeviceEventList* events) {
  UdevDevice device(dev);
  if (device.IsMobileBroadbandDevice())
    return;

  std::string device_path = device.NativePath();
  if (strcmp(action, kUdevAddAction) == 0) {
    if (base::ContainsKey(devices_detected_, device_path)) {
      events->push_back(DeviceEvent(DeviceEvent::kDeviceScanned, device_path));
    } else {
      devices_detected_.insert(device_path);
      events->push_back(DeviceEvent(DeviceEvent::kDeviceAdded, device_path));
    }
  } else if (strcmp(action, kUdevRemoveAction) == 0) {
    if (base::ContainsKey(devices_detected_, device_path)) {
      devices_detected_.erase(device_path);
      events->push_back(DeviceEvent(DeviceEvent::kDeviceRemoved, device_path));
    }
  }
}

bool DiskMonitor::GetDeviceEvents(DeviceEventList* events) {
  CHECK(events) << "Invalid device event list";

  udev_device* dev = udev_monitor_receive_device(udev_monitor_);
  if (!dev) {
    LOG(WARNING) << "Ignore device event with no associated udev device.";
    return false;
  }

  const char* sys_path = udev_device_get_syspath(dev);
  const char* subsystem = udev_device_get_subsystem(dev);
  const char* action = udev_device_get_action(dev);

  LOG(INFO) << "Got action " << quote(action) << " on device "
            << quote(udev_device_get_sysname(dev));
  LogUdevDevice(dev);

  if (!sys_path || !subsystem || !action) {
    udev_device_unref(dev);
    return false;
  }

  // |udev_monitor_| only monitors block, mmc, and scsi device changes, so
  // subsystem is either "block", "mmc", or "scsi".
  if (strcmp(subsystem, kBlockSubsystem) == 0) {
    ProcessBlockDeviceEvents(dev, action, events);
  } else {
    // strcmp(subsystem, kMmcSubsystem) == 0 ||
    // strcmp(subsystem, kScsiSubsystem) == 0
    ProcessMmcOrScsiDeviceEvents(dev, action, events);
  }

  udev_device_unref(dev);
  return true;
}

bool DiskMonitor::GetDiskByDevicePath(const base::FilePath& device_path,
                                      Disk* disk) const {
  if (device_path.empty())
    return false;

  bool disk_found = false;
  EnumerateBlockDevices(base::Bind(&MatchDiskByPath, device_path.value(),
                                   base::Unretained(&disk_found),
                                   base::Unretained(disk)));
  return disk_found;
}

bool DiskMonitor::IsPathRecognized(const base::FilePath& path) const {
  // The following paths are handled:
  //     /sys/...
  //     /devices/...
  //     /dev/...
  return base::StartsWith(path.value(), "/sys/",
                          base::CompareCase::SENSITIVE) ||
         base::StartsWith(path.value(), "/devices/",
                          base::CompareCase::SENSITIVE) ||
         base::StartsWith(path.value(), "/dev/", base::CompareCase::SENSITIVE);
}

}  // namespace cros_disks
