// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CROS_DISKS_DAEMON_H_
#define CROS_DISKS_DAEMON_H_

#include <base/macros.h>

#include "cros-disks/archive_manager.h"
#include "cros-disks/cros_disks_server.h"
#include "cros-disks/device_ejector.h"
#include "cros-disks/device_event_moderator.h"
#include "cros-disks/disk_manager.h"
#include "cros-disks/format_manager.h"
#include "cros-disks/metrics.h"
#include "cros-disks/platform.h"
#include "cros-disks/session_manager_proxy.h"

namespace cros_disks {

class Daemon {
 public:
  explicit Daemon(DBus::Connection* dbus_connection);
  ~Daemon() = default;

  // Initializes various components in the daemon.
  void Initialize();

  // Returns a file descriptor for monitoring device events.
  int GetDeviceEventDescriptor() const;

  // Processes the available device events.
  void ProcessDeviceEvents();

 private:
  Metrics metrics_;
  Platform platform_;
  ArchiveManager archive_manager_;
  DeviceEjector device_ejector_;
  DiskManager disk_manager_;
  FormatManager format_manager_;
  CrosDisksServer server_;
  DeviceEventModerator event_moderator_;
  SessionManagerProxy session_manager_proxy_;

  DISALLOW_COPY_AND_ASSIGN(Daemon);
};

}  // namespace cros_disks

#endif  // CROS_DISKS_DAEMON_H_
