// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cros-disks/cros-disks-server-impl.h"

#include <sys/mount.h>

#include <base/logging.h>

#include "cros-disks/device-event.h"
#include "cros-disks/disk.h"
#include "cros-disks/disk-manager.h"
#include "cros-disks/format-manager.h"

using std::string;
using std::vector;

namespace cros_disks {

// TODO(rtc): this should probably be a flag.
static const char* kServicePath = "/org/chromium/CrosDisks";
static const char* kServiceErrorName = "org.chromium.CrosDisks.Error";

CrosDisksServer::CrosDisksServer(DBus::Connection& connection,  // NOLINT
    DiskManager* disk_manager,
    FormatManager * format_manager)
    : DBus::ObjectAdaptor(connection, kServicePath),
      disk_manager_(disk_manager),
      format_manager_(format_manager),
      is_device_event_queued_(true) {

  format_manager_->set_parent(this);
}

CrosDisksServer::~CrosDisksServer() {
}

bool CrosDisksServer::IsAlive(DBus::Error& error) {  // NOLINT
  return true;
}

string CrosDisksServer::GetDeviceFilesystem(const string& device_path,
    ::DBus::Error& error) {  // NOLINT
  return disk_manager_->GetFilesystemTypeOfDevice(device_path);
}

void CrosDisksServer::SignalFormattingFinished(const string& device_path,
    int status) {
  if (status) {
    FormattingFinished(device_path, false);
    LOG(ERROR) << "Could not format device '" << device_path
      << "'. Formatting process failed with an exit code " << status;
  } else {
    FormattingFinished(device_path, true);
  }
}

bool CrosDisksServer::FormatDevice(const string& device_path,
      const string& filesystem, ::DBus::Error &error) {  // NOLINT
  if (!format_manager_->StartFormatting(device_path, filesystem)) {
    LOG(ERROR) << "Could not format device " << device_path
      << " as file system '" << filesystem << "'";
    return false;
  }
  return true;
}

string CrosDisksServer::FilesystemMount(const string& device_path,
    const string& filesystem_type, const vector<string>& mount_options,
    DBus::Error& error) {  // NOLINT
  string mount_path;
  if (disk_manager_->Mount(device_path, filesystem_type, mount_options,
        &mount_path)) {
    DiskChanged(device_path);
  } else {
    string message = "Could not mount device " + device_path;
    LOG(ERROR) << message;
    error.set(kServiceErrorName, message.c_str());
  }
  return mount_path;
}

void CrosDisksServer::FilesystemUnmount(const string& device_path,
    const vector<string>& mount_options, DBus::Error& error) {  // NOLINT
  if (!disk_manager_->Unmount(device_path, mount_options)) {
    string message = "Could not unmount device " + device_path;
    LOG(ERROR) << message;
    error.set(kServiceErrorName, message.c_str());
  }
}

vector<string> CrosDisksServer::DoEnumerateDevices(
    bool auto_mountable_only) const {
  vector<Disk> disks = disk_manager_->EnumerateDisks();
  vector<string> devices;
  devices.reserve(disks.size());
  for (vector<Disk>::const_iterator disk_iterator(disks.begin());
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
    error.set(kServiceErrorName, message.c_str());
  }
  return disk.ToDBusFormat();
}

void CrosDisksServer::SignalDeviceChanges() {
  DeviceEvent event;
  if (disk_manager_->GetDeviceEvent(&event)) {
    if (is_device_event_queued_) {
      device_event_queue_.Add(event);
    } else {
      DispatchDeviceEvent(event);
    }
  }
}

void CrosDisksServer::OnScreenIsLocked() {
  LOG(INFO) << "Screen is locked";
  is_device_event_queued_ = true;
}

void CrosDisksServer::OnScreenIsUnlocked() {
  LOG(INFO) << "Screen is unlocked";
  DispatchQueuedDeviceEvents();
  is_device_event_queued_ = false;
}

void CrosDisksServer::OnSessionStarted(const string& user) {
  LOG(INFO) << "Session started";
  DispatchQueuedDeviceEvents();
  is_device_event_queued_ = false;
}

void CrosDisksServer::OnSessionStopped(const string& user) {
  LOG(INFO) << "Session stopped";
  is_device_event_queued_ = true;
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
    case DeviceEvent::kDiskAddedAfterRemoved:
      DiskRemoved(event.device_path);
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

void CrosDisksServer::DispatchQueuedDeviceEvents() {
  const DeviceEvent* event;
  while ((event = device_event_queue_.Head()) != NULL) {
    LOG(INFO) << "Dispatch queued event: type=" << event->event_type
      << " device='" << event->device_path << "'";
    DispatchDeviceEvent(*event);
    device_event_queue_.Remove();
  }
}

}  // namespace cros_disks
