// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/logging.h>

#include "cros-disks-server-impl.h"
#include "disk.h"
#include "disk-manager.h"

#include <sys/mount.h>

namespace cros_disks {

// TODO(rtc): this should probably be a flag.
static const char* kServicePath = "/org/chromium/CrosDisks";
static const char* kServiceErrorName = "org.chromium.CrosDisks.Error";

CrosDisksServer::CrosDisksServer(DBus::Connection& connection,
    DiskManager* disk_manager)
    : DBus::ObjectAdaptor(connection, kServicePath),
      disk_manager_(disk_manager) {
}

CrosDisksServer::~CrosDisksServer() { }

bool CrosDisksServer::IsAlive(DBus::Error& error) {  // NOLINT
  return true;
}

std::string CrosDisksServer::FilesystemMount(
    const std::string& device_path,
    const std::string& filesystem_type,
    const std::vector<std::string>& mount_options,
    DBus::Error& error) {  // NOLINT

  std::string mount_path;
  if (disk_manager_->Mount(device_path, filesystem_type, mount_options,
        &mount_path)) {
    DeviceChanged(device_path);
  } else {
    std::string message = "Could not mount device " + device_path;
    LOG(ERROR) << message;
    error.set(kServiceErrorName, message.c_str());
  }
  return mount_path;
}

void CrosDisksServer::FilesystemUnmount(
    const std::string& device_path,
    const std::vector<std::string>& mount_options,
    DBus::Error& error) {  // NOLINT

  if (!disk_manager_->Unmount(device_path, mount_options)) {
    std::string message = "Could not unmount device " + device_path;
    LOG(ERROR) << message;
    error.set(kServiceErrorName, message.c_str());
  }
}

std::vector<std::string> CrosDisksServer::EnumerateDeviceFiles(
    DBus::Error& error) {  // NOLINT
  std::vector<Disk> disks = disk_manager_->EnumerateDisks();
  std::vector<std::string> devices;
  devices.reserve(disks.size());
  for (std::vector<Disk>::const_iterator disk_iterator(disks.begin());
      disk_iterator != disks.end(); ++disk_iterator)
    devices.push_back(disk_iterator->device_file());
  return devices;
}

DBusDisk CrosDisksServer::GetDeviceProperties(const std::string& device_path,
    DBus::Error& error) {  // NOLINT
  Disk disk;
  if (!disk_manager_->GetDiskByDevicePath(device_path, &disk)) {
    std::string message = "Could not get the properties of device "
      + device_path;
    LOG(ERROR) << message;
    error.set(kServiceErrorName, message.c_str());
  }
  return disk.ToDBusFormat();
}

void CrosDisksServer::SignalDeviceChanges() {
  std::string device_path;
  std::string action;
  if (disk_manager_->ProcessUdevChanges(&device_path, &action)) {
    if (action == "add") {
      DeviceAdded(device_path);
    } else if (action == "remove") {
      DeviceRemoved(device_path);
    } else if (action == "change") {
      DeviceChanged(device_path);
    }
  }
}

} // namespace cros_disks
