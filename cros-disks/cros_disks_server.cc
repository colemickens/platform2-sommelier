// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cros-disks/cros_disks_server.h"

#include <base/logging.h>
#include <chromeos/dbus/service_constants.h>

#include "cros-disks/archive_manager.h"
#include "cros-disks/device_event.h"
#include "cros-disks/disk.h"
#include "cros-disks/disk_manager.h"
#include "cros-disks/format_manager.h"
#include "cros-disks/platform.h"
#include "cros-disks/rename_manager.h"

using std::string;
using std::vector;

namespace cros_disks {

CrosDisksServer::CrosDisksServer(DBus::Connection& connection,  // NOLINT
                                 Platform* platform,
                                 DiskManager* disk_manager,
                                 FormatManager* format_manager,
                                 RenameManager* rename_manager)
    : DBus::ObjectAdaptor(connection, kCrosDisksServicePath),
      platform_(platform),
      disk_manager_(disk_manager),
      format_manager_(format_manager),
      rename_manager_(rename_manager) {
  CHECK(platform_) << "Invalid platform object";
  CHECK(disk_manager_) << "Invalid disk manager object";
  CHECK(format_manager_) << "Invalid format manager object";
  CHECK(rename_manager_) << "Invalid rename manager object";

  format_manager_->set_observer(this);
  rename_manager_->set_observer(this);
}

void CrosDisksServer::RegisterMountManager(MountManager* mount_manager) {
  CHECK(mount_manager) << "Invalid mount manager object";
  mount_managers_.push_back(mount_manager);
}

bool CrosDisksServer::IsAlive(DBus::Error& error) {  // NOLINT
  return true;
}

void CrosDisksServer::Format(const string& path,
                             const string& filesystem_type,
                             const vector<string>& options,
                             DBus::Error& error) {  // NOLINT
  FormatErrorType error_type = FORMAT_ERROR_NONE;
  Disk disk;
  if (!disk_manager_->GetDiskByDevicePath(path, &disk)) {
    error_type = FORMAT_ERROR_INVALID_DEVICE_PATH;
  } else if (disk.is_on_boot_device()) {
    error_type = FORMAT_ERROR_DEVICE_NOT_ALLOWED;
  } else {
    error_type = format_manager_->StartFormatting(path, disk.device_file(),
                                                  filesystem_type);
  }

  if (error_type != FORMAT_ERROR_NONE) {
    LOG(ERROR) << "Could not format device '" << path << "' as filesystem '"
               << filesystem_type << "'";
    FormatCompleted(error_type, path);
  }
}

void CrosDisksServer::Rename(const string& path,
                             const string& volume_name,
                             DBus::Error& error) {  // NOLINT
  RenameErrorType error_type = RENAME_ERROR_NONE;
  Disk disk;
  if (!disk_manager_->GetDiskByDevicePath(path, &disk)) {
    error_type = RENAME_ERROR_INVALID_DEVICE_PATH;
  } else if (disk.is_on_boot_device() || disk.is_read_only()) {
    error_type = RENAME_ERROR_DEVICE_NOT_ALLOWED;
  } else {
    error_type = rename_manager_->StartRenaming(
        path, disk.device_file(), volume_name, disk.filesystem_type());
  }

  if (error_type != RENAME_ERROR_NONE) {
    LOG(ERROR) << "Could not rename device '" << path << "' as '" << volume_name
               << "'";
    RenameCompleted(error_type, path);
  }
}

MountManager* CrosDisksServer::FindMounter(const string& source_path) const {
  for (const auto& manager : mount_managers_) {
    if (manager->CanMount(source_path)) {
      return manager;
    }
  }
  return nullptr;
}

void CrosDisksServer::Mount(const string& path,
                            const string& filesystem_type,
                            const vector<string>& options,
                            DBus::Error& error) {  // NOLINT
  MountErrorType error_type = MOUNT_ERROR_INVALID_PATH;
  MountSourceType source_type = MOUNT_SOURCE_INVALID;
  string source_path;
  string mount_path;

  if (platform_->GetRealPath(path, &source_path)) {
    MountManager* mounter = FindMounter(source_path);
    if (mounter) {
      source_type = mounter->GetMountSourceType();
      error_type =
          mounter->Mount(source_path, filesystem_type, options, &mount_path);
    }
  }

  if (error_type != MOUNT_ERROR_NONE) {
    LOG(ERROR) << "Failed to mount '" << path << "'";
  }
  MountCompleted(error_type, path, source_type, mount_path);
}

void CrosDisksServer::Unmount(const string& path,
                              const vector<string>& options,
                              DBus::Error& error) {  // NOLINT
  MountErrorType error_type = MOUNT_ERROR_INVALID_PATH;
  for (const auto& manager : mount_managers_) {
    if (manager->CanUnmount(path)) {
      error_type = manager->Unmount(path, options);
      break;
    }
  }

  if (error_type != MOUNT_ERROR_NONE) {
    string message = "Failed to unmount '" + path + "'";
    error.set(kCrosDisksServiceError, message.c_str());
  }
}

void CrosDisksServer::UnmountAll(DBus::Error& error) {  // NOLINT
  DoUnmountAll();
}

void CrosDisksServer::DoUnmountAll() {
  for (const auto& manager : mount_managers_) {
    manager->UnmountAll();
  }
}

vector<string> CrosDisksServer::DoEnumerateDevices(
    bool auto_mountable_only) const {
  vector<Disk> disks = disk_manager_->EnumerateDisks();
  vector<string> devices;
  devices.reserve(disks.size());
  for (const auto& disk : disks) {
    if (!auto_mountable_only || disk.is_auto_mountable()) {
      devices.push_back(disk.native_path());
    }
  }
  return devices;
}

vector<string> CrosDisksServer::EnumerateDevices(
    DBus::Error& error) {  // NOLINT
  return DoEnumerateDevices(false);
}

vector<string> CrosDisksServer::EnumerateAutoMountableDevices(
    DBus::Error& error) {  // NOLINT
  return DoEnumerateDevices(true);
}

vector<CrosDisksServer::DBusMountEntry> CrosDisksServer::EnumerateMountEntries(
    DBus::Error& error) {  // NOLINT
  vector<DBusMountEntry> dbus_mount_entries;
  for (const auto& manager : mount_managers_) {
    vector<MountEntry> mount_entries;
    manager->GetMountEntries(&mount_entries);
    for (const auto& mount_entry : mount_entries) {
      dbus_mount_entries.push_back(
          {mount_entry.error_type, mount_entry.source_path,
           mount_entry.source_type, mount_entry.mount_path});
    }
  }
  return dbus_mount_entries;
}

DBusDisk CrosDisksServer::GetDeviceProperties(const string& device_path,
                                              DBus::Error& error) {  // NOLINT
  Disk disk;
  if (!disk_manager_->GetDiskByDevicePath(device_path, &disk)) {
    string message = "Could not get the properties of device " + device_path;
    LOG(ERROR) << message;
    error.set(kCrosDisksServiceError, message.c_str());
  }
  return disk.ToDBusFormat();
}

void CrosDisksServer::OnFormatCompleted(const string& device_path,
                                        FormatErrorType error_type) {
  FormatCompleted(error_type, device_path);
}

void CrosDisksServer::OnRenameCompleted(const string& device_path,
                                        RenameErrorType error_type) {
  RenameCompleted(error_type, device_path);
}

void CrosDisksServer::OnScreenIsLocked() {
  // no-op
}

void CrosDisksServer::OnScreenIsUnlocked() {
  // no-op
}

void CrosDisksServer::OnSessionStarted() {
  for (const auto& manager : mount_managers_) {
    manager->StartSession();
  }
}

void CrosDisksServer::OnSessionStopped() {
  for (const auto& manager : mount_managers_) {
    manager->StopSession();
  }
}

void CrosDisksServer::DispatchDeviceEvent(const DeviceEvent& event) {
  switch (event.event_type) {
    case DeviceEvent::kDeviceAdded:
      DeviceAdded(event.device_path);
      break;
    case DeviceEvent::kDeviceScanned:
      DeviceScanned(event.device_path);
      break;
    case DeviceEvent::kDeviceRemoved:
      DeviceRemoved(event.device_path);
      break;
    case DeviceEvent::kDiskAdded:
      DiskAdded(event.device_path);
      break;
    case DeviceEvent::kDiskChanged:
      DiskChanged(event.device_path);
      break;
    case DeviceEvent::kDiskRemoved:
      DiskRemoved(event.device_path);
      break;
    default:
      break;
  }
}

}  // namespace cros_disks
