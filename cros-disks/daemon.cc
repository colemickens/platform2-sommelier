// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cros-disks/daemon.h"

#include <chromeos/dbus/service_constants.h>

namespace {

const char kArchiveMountRootDirectory[] = "/media/archive";
const char kDiskMountRootDirectory[] = "/media/removable";
const char kFUSEMountRootDirectory[] = "/media/fuse";

// A temporary directory where every FUSE invocation will have some
// writable subdirectory.
const char kFUSEWritableRootDirectory[] = "/run/fuse";

const char kNonPrivilegedMountUser[] = "chronos";

}  // namespace

namespace cros_disks {

Daemon::Daemon(bool has_session_manager)
    : brillo::DBusServiceDaemon(kCrosDisksServiceName),
      has_session_manager_(has_session_manager),
      device_ejector_(&process_reaper_),
      archive_manager_(kArchiveMountRootDirectory, &platform_, &metrics_),
      disk_monitor_(),
      disk_manager_(kDiskMountRootDirectory,
                    &platform_,
                    &metrics_,
                    &disk_monitor_,
                    &device_ejector_),
      format_manager_(&process_reaper_),
      rename_manager_(&platform_, &process_reaper_),
      fuse_manager_(kFUSEMountRootDirectory,
                    kFUSEWritableRootDirectory,
                    &platform_,
                    &metrics_),
      device_event_task_id_(brillo::MessageLoop::kTaskIdNull) {
  CHECK(platform_.SetMountUser(kNonPrivilegedMountUser))
      << "'" << kNonPrivilegedMountUser
      << "' is not available for non-privileged mount operations.";
  CHECK(archive_manager_.Initialize())
      << "Failed to initialize the archive manager";
  CHECK(disk_manager_.Initialize()) << "Failed to initialize the disk manager";
  CHECK(fuse_manager_.Initialize()) << "Failed to initialize the FUSE manager";
  process_reaper_.Register(this);
}

Daemon::~Daemon() {
  brillo::MessageLoop::current()->CancelTask(device_event_task_id_);
}

void Daemon::RegisterDBusObjectsAsync(
    brillo::dbus_utils::AsyncEventSequencer* sequencer) {
  server_ = std::make_unique<CrosDisksServer>(
      bus_, &platform_, &disk_monitor_, &format_manager_, &rename_manager_);

  // Register mount managers with the commonly used ones come first.
  server_->RegisterMountManager(&disk_manager_);
  server_->RegisterMountManager(&archive_manager_);
  server_->RegisterMountManager(&fuse_manager_);

  event_moderator_ = std::make_unique<DeviceEventModerator>(
      server_.get(), &disk_monitor_, has_session_manager_);

  if (has_session_manager_) {
    session_manager_proxy_ = std::make_unique<SessionManagerProxy>(bus_);
    session_manager_proxy_->AddObserver(server_.get());
    session_manager_proxy_->AddObserver(event_moderator_.get());
  }

  device_event_task_id_ = brillo::MessageLoop::current()->WatchFileDescriptor(
      FROM_HERE, disk_monitor_.udev_monitor_fd(),
      brillo::MessageLoop::kWatchRead, true,
      base::Bind(&Daemon::OnDeviceEvents, base::Unretained(this)));

  server_->RegisterAsync(
      sequencer->GetHandler("Failed to export cros-disks service.", false));
}

void Daemon::OnDeviceEvents() {
  event_moderator_->ProcessDeviceEvents();
}

}  // namespace cros_disks
