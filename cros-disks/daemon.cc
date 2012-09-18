// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cros-disks/daemon.h"

namespace {

const char kArchiveMountRootDirectory[] = "/media/archive";
const char kDiskMountRootDirectory[] = "/media/removable";
const char kNonPrivilegedMountUser[] = "chronos";

}  // namespace

namespace cros_disks {

Daemon::Daemon(DBus::Connection* dbus_connection)
    : archive_manager_(kArchiveMountRootDirectory, &platform_, &metrics_),
      disk_manager_(kDiskMountRootDirectory, &platform_, &metrics_,
                    &device_ejector_),
      server_(*dbus_connection, &platform_, &archive_manager_,
              &disk_manager_, &format_manager_),
      event_moderator_(&server_, &disk_manager_),
      session_manager_proxy_(dbus_connection) {
}

Daemon::~Daemon() {
}

void Daemon::Initialize() {
  CHECK(platform_.SetMountUser(kNonPrivilegedMountUser))
      << "'" << kNonPrivilegedMountUser
      << "' is not available for non-privileged mount operations.";
  CHECK(archive_manager_.Initialize())
      << "Failed to initialize the archive manager";
  CHECK(disk_manager_.Initialize()) << "Failed to initialize the disk manager";

  session_manager_proxy_.AddObserver(&server_);
  session_manager_proxy_.AddObserver(&event_moderator_);
}

int Daemon::GetDeviceEventDescriptor() const {
  return disk_manager_.udev_monitor_fd();
}

void Daemon::ProcessDeviceEvents() {
  event_moderator_.ProcessDeviceEvents();
}

}  // namespace cros_disks
