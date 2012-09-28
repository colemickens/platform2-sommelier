// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cros-disks/cros-disks-server-impl.h"

#include <base/logging.h>
#include <chromeos/dbus/service_constants.h>

#include "cros-disks/archive-manager.h"
#include "cros-disks/device-event.h"
#include "cros-disks/disk.h"
#include "cros-disks/disk-manager.h"
#include "cros-disks/format-manager.h"
#include "cros-disks/platform.h"

using std::string;
using std::vector;

namespace cros_disks {

CrosDisksServer::CrosDisksServer(DBus::Connection& connection,  // NOLINT
                                 Platform* platform,
                                 ArchiveManager* archive_manager,
                                 DiskManager* disk_manager,
                                 FormatManager* format_manager)
    : DBus::ObjectAdaptor(connection, kCrosDisksServicePath),
      platform_(platform),
      archive_manager_(archive_manager),
      disk_manager_(disk_manager),
      format_manager_(format_manager) {
  CHECK(platform_) << "Invalid platform object";
  CHECK(archive_manager_) << "Invalid archive manager object";
  CHECK(disk_manager_) << "Invalid disk manager object";
  CHECK(format_manager_) << "Invalid format manager object";

  // TODO(benchan): Refactor the code so that we don't have to pass
  //                DiskManager, ArchiveManager, etc to the constructor
  //                of CrosDisksServer, but instead pass a list of mount
  //                managers.
  mount_managers_.push_back(disk_manager_);
  mount_managers_.push_back(archive_manager_);

  InitializeProperties();
  format_manager_->set_observer(this);
}

CrosDisksServer::~CrosDisksServer() {
}

bool CrosDisksServer::IsAlive(DBus::Error& error) {  // NOLINT
  return true;
}

void CrosDisksServer::Format(const string& path,
                             const string& filesystem_type,
                             const vector<string>& options,
                             DBus::Error &error) {  // NOLINT
  FormatDevice(path, filesystem_type, error);
}

// TODO(benchan): Remove this method after Chrome switches to use Format().
bool CrosDisksServer::FormatDevice(const string& path,
                                   const string& filesystem_type,
                                   DBus::Error &error) {  // NOLINT
  FormatErrorType error_type = FORMAT_ERROR_NONE;
  Disk disk;
  if (!disk_manager_->GetDiskByDevicePath(path, &disk)) {
    error_type = FORMAT_ERROR_INVALID_DEVICE_PATH;
  } else if (disk.is_on_boot_device()) {
    error_type = FORMAT_ERROR_DEVICE_NOT_ALLOWED;
  } else {
    error_type = format_manager_->StartFormatting(path, filesystem_type);
  }

  if (error_type != FORMAT_ERROR_NONE) {
    LOG(ERROR) << "Could not format device '" << path
               << "' as filesystem '" << filesystem_type << "'";
    FormatCompleted(error_type, path);
    return false;
  }
  return true;
}

void CrosDisksServer::Mount(const string& path,
                            const string& filesystem_type,
                            const vector<string>& options,
                            DBus::Error& error) {  // NOLINT
  MountErrorType error_type = MOUNT_ERROR_INVALID_PATH;
  MountSourceType source_type = MOUNT_SOURCE_INVALID;
  string mount_path;

  for (vector<MountManager*>::iterator manager_iter = mount_managers_.begin();
       manager_iter != mount_managers_.end(); ++manager_iter) {
    MountManager* manager = *manager_iter;
    if (manager->CanMount(path)) {
      source_type = manager->GetMountSourceType();
      error_type = manager->Mount(path, filesystem_type, options, &mount_path);
      break;
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
  for (vector<MountManager*>::iterator manager_iter = mount_managers_.begin();
       manager_iter != mount_managers_.end(); ++manager_iter) {
    MountManager* manager = *manager_iter;
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
  for (vector<MountManager*>::iterator manager_iter = mount_managers_.begin();
       manager_iter != mount_managers_.end(); ++manager_iter) {
    MountManager* manager = *manager_iter;
    manager->UnmountAll();
  }
}

vector<string> CrosDisksServer::DoEnumerateDevices(
    bool auto_mountable_only) const {
  vector<Disk> disks = disk_manager_->EnumerateDisks();
  vector<string> devices;
  devices.reserve(disks.size());
  for (vector<Disk>::const_iterator disk_iterator = disks.begin();
       disk_iterator != disks.end(); ++disk_iterator) {
    if (!auto_mountable_only || disk_iterator->is_auto_mountable()) {
      devices.push_back(disk_iterator->native_path());
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
  // TODO(benchan): Deprecate the FormattingFinished signal once Chrome
  // switches to observe the FormatCompleted signal instead.
  if (error_type != FORMAT_ERROR_NONE) {
    FormattingFinished("!" + device_path);
    LOG(ERROR) << "Failed to format '" << device_path << "'";
  } else {
    FormattingFinished(device_path);
  }
  FormatCompleted(error_type, device_path);
}

void CrosDisksServer::OnScreenIsLocked() {
  // no-op
}

void CrosDisksServer::OnScreenIsUnlocked() {
  // no-op
}

void CrosDisksServer::OnSessionStarted(const string& user) {
  for (vector<MountManager*>::iterator manager_iter = mount_managers_.begin();
       manager_iter != mount_managers_.end(); ++manager_iter) {
    MountManager* manager = *manager_iter;
    manager->StartSession(user);
  }
}

void CrosDisksServer::OnSessionStopped(const string& user) {
  for (vector<MountManager*>::iterator manager_iter = mount_managers_.begin();
       manager_iter != mount_managers_.end(); ++manager_iter) {
    MountManager* manager = *manager_iter;
    manager->StopSession(user);
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

void CrosDisksServer::InitializeProperties() {
  try {
    DBus::Variant value;
    value.writer().append_bool(platform_->experimental_features_enabled());
    CrosDisks_adaptor::set_property(kExperimentalFeaturesEnabled, value);
  } catch (const DBus::Error& e) {  // NOLINT
    LOG(FATAL) << "Failed to initialize properties: " << e.what();
  }
}

void CrosDisksServer::on_set_property(
    DBus::InterfaceAdaptor& interface,  // NOLINT
    const string& property, const DBus::Variant& value) {
  if (property == kExperimentalFeaturesEnabled) {
    platform_->set_experimental_features_enabled(value.reader().get_bool());
  }
}

}  // namespace cros_disks
