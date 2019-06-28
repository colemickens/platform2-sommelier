// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "diagnostics/common/disk_utils.h"

#include <fcntl.h>
#include <libudev.h>
#include <memory>
#include <string>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <unordered_map>
#include <utility>

#include <base/files/file_enumerator.h>
#include <base/files/file_util.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/string_split.h>
#include <base/strings/string_util.h>
#include <base/values.h>

namespace diagnostics {

namespace disk_utils {

using NonRemovableBlockDeviceInfo =
    ::chromeos::cros_healthd::mojom::NonRemovableBlockDeviceInfo;
using NonRemovableBlockDeviceInfoPtr =
    ::chromeos::cros_healthd::mojom::NonRemovableBlockDeviceInfoPtr;

namespace {

// Reads the contents of |filename| within |directory| into |out|, trimming
// trailing whitespace.  Returns true on success.
bool ReadAndTrimString(const base::FilePath& directory,
                       const std::string& filename,
                       std::string* out) {
  if (!base::ReadFileToString(directory.Append(filename), out))
    return false;

  base::TrimWhitespaceASCII(*out, base::TRIM_TRAILING, out);
  return true;
}

// Reads a 64-bit decimal-encoded integer value from a text file and returns
// true on success.
bool ReadInt64(const base::FilePath& directory,
               const std::string& filename,
               int64_t* out) {
  std::string buffer;
  if (!ReadAndTrimString(directory, filename, &buffer))
    return false;
  return base::StringToInt64(buffer, out);
}

// Reads a 64-bit hex-encoded integer value from a text file and returns true on
// success.
bool ReadHexInt64(const base::FilePath& directory,
                  const std::string& filename,
                  int64_t* out) {
  std::string buffer;
  if (!ReadAndTrimString(directory, filename, &buffer))
    return false;
  return base::HexStringToInt64(buffer, out);
}

// Reads a 32-bit hex-encoded unsigned integer value from a file and returns
// true on success.
bool ReadHexUint32(const base::FilePath& directory,
                   const std::string& filename,
                   uint32_t* out) {
  std::string buffer;
  if (!ReadAndTrimString(directory, filename, &buffer))
    return false;
  return base::HexStringToUInt(buffer, out);
}

// Look through all the block devices and find the ones that are explicitly
// non-removable.
std::vector<base::FilePath> GetNonRemovableBlockDevices(
    const base::FilePath& root) {
  std::vector<base::FilePath> res;
  const base::FilePath storage_dir_path(root.Append("sys/class/block/"));
  base::FileEnumerator storage_dir_it(storage_dir_path, true,
                                      base::FileEnumerator::SHOW_SYM_LINKS |
                                          base::FileEnumerator::FILES |
                                          base::FileEnumerator::DIRECTORIES);

  while (true) {
    const auto storage_path = storage_dir_it.Next();
    if (storage_path.empty())
      break;

    // Skip Loobpack or dm-verity device
    if (base::StartsWith(storage_path.BaseName().value(), "loop",
                         base::CompareCase::SENSITIVE) ||
        base::StartsWith(storage_path.BaseName().value(), "dm-",
                         base::CompareCase::SENSITIVE)) {
      continue;
    }

    // Only return non-removable devices
    int64_t removable = 0;
    if (!ReadInt64(storage_path, "removable", &removable) || removable) {
      VLOG(1) << "Storage device " << storage_path.value()
              << " does not specify the removable property or is removable.";
      continue;
    }

    res.push_back(storage_path);
  }

  return res;
}

// Gets the size of the drive in bytes, given the /dev node.
bool GetDriveDeviceSizeInBytes(const base::FilePath& dev_path,
                               int64_t* size_in_bytes) {
  int fd = open(dev_path.value().c_str(), O_RDONLY, 0);

  if (fd < 0) {
    LOG(ERROR) << "Could not open " << dev_path.value() << " for ioctl access";
    return false;
  }

  base::ScopedFD scoped_fd(fd);
  int64_t size = 0;
  int res = ioctl(fd, BLKGETSIZE64, &size);
  if (res != 0) {
    LOG(ERROR) << "Unable to run ioctl(" << fd << ", BLKGETSIZE64, &size) => "
               << res;
    return false;
  }

  DCHECK_GE(size, 0);
  VLOG(1) << "Found size of " << dev_path.value() << " is "
          << std::to_string(size);

  if (size_in_bytes)
    *size_in_bytes = size;

  return true;
}

// Fill the output with a colon-separated list of subsystems. For example,
// "block:mmc:mmc_host:pci". Similar output is returned by `lsblk -o SUBSYSTEMS`
bool GetUdevDeviceSubsystems(udev_device* input_device,
                             std::string* subsystem_output) {
  // |subsystems| will track the stack of subsystems that this device uses.
  std::vector<std::string> subsystems;

  for (udev_device* device = input_device; device != nullptr;
       device = udev_device_get_parent(device)) {
    const char* subsystem = udev_device_get_subsystem(device);
    if (subsystem != nullptr) {
      subsystems.push_back(subsystem);
    }
  }

  if (subsystems.empty()) {
    const char* devnode = udev_device_get_devnode(input_device);
    VLOG(1) << "Unable to collect any subsystems for device "
            << (devnode ? devnode : "<unknown>");
    return false;
  }

  if (subsystem_output) {
    *subsystem_output = base::JoinString(subsystems, ":");
  }
  return true;
}

// Return the /dev/... name for |sys_path|, which should be a
// /sys/class/block/... name. This utilizes libudev. Also returns the driver
// |subsystems| for use in determining the "type" of the block device.
bool GatherSysPathRelatedInfo(const base::FilePath& sys_path,
                              base::FilePath* devnode_path,
                              std::string* subsystems) {
  udev* udev = udev_new();
  if (udev == nullptr) {
    LOG(ERROR) << "Unable to get udev.";
    return false;
  }

  // Keep track of whether we have succeeded at getting a dev node.
  bool found_related_info = false;

  udev_device* device =
      udev_device_new_from_syspath(udev, sys_path.value().c_str());

  if (device != nullptr) {
    if (!GetUdevDeviceSubsystems(device, subsystems)) {
      VLOG(1) << "Unable to get a disk type from the subsystem chain.";
    } else {
      if (devnode_path) {
        *devnode_path = base::FilePath{udev_device_get_devnode(device)};
      }
      found_related_info = true;
    }
    udev_device_unref(device);
  } else {
    LOG(ERROR) << "Unable to get udev_device for " << sys_path.value();
  }

  udev_unref(udev);
  return found_related_info;
}

bool FetchNonRemovableBlockDeviceInfo(
    const base::FilePath& sys_path,
    NonRemovableBlockDeviceInfoPtr* output_info) {
  NonRemovableBlockDeviceInfo info;

  base::FilePath devnode_path;
  if (!GatherSysPathRelatedInfo(sys_path, &devnode_path,
                                /*subsystems=*/&info.type)) {
    VLOG(1) << "Unable to get the dev node path for " << sys_path.value();
    return false;
  }

  info.path = devnode_path.value();

  if (!GetDriveDeviceSizeInBytes(devnode_path, &info.size)) {
    VLOG(1) << "Could not find the device size. (" << devnode_path.value()
            << ")";
    return false;
  }

  const auto device_path = sys_path.Append("device");

  // Not all devices in sysfs have a model/name, so ignore failure here.
  if (!ReadAndTrimString(device_path, "model", &info.name)) {
    ReadAndTrimString(device_path, "name", &info.name);
  }

  // Not all devices in sysfs have a serial, so ignore the return code.
  ReadHexUint32(device_path, "serial", &info.serial);

  int64_t manfid = 0;
  if (ReadHexInt64(device_path, "manfid", &manfid)) {
    DCHECK_EQ(manfid & 0xFF, manfid);
    info.manfid = manfid;
  }

  if (output_info) {
    *output_info = info.Clone();
  }

  return true;
}

}  // namespace

std::vector<NonRemovableBlockDeviceInfoPtr> FetchNonRemovableBlockDevicesInfo(
    const base::FilePath& root) {
  // We'll fill out this |devices| vector with the return value.
  std::vector<NonRemovableBlockDeviceInfoPtr> devices{};

  for (const base::FilePath& sys_path : GetNonRemovableBlockDevices(root)) {
    VLOG(1) << "Processing the node " << sys_path.value();
    NonRemovableBlockDeviceInfoPtr info;
    if (FetchNonRemovableBlockDeviceInfo(sys_path, &info)) {
      DCHECK_NE(info->path, "");
      DCHECK_NE(info->size, 0);
      DCHECK_NE(info->type, "");
      devices.push_back(std::move(info));
    }
  }

  return devices;
}

}  // namespace disk_utils

}  // namespace diagnostics
