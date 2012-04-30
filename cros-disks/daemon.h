// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CROS_DISKS_DAEMON_H_
#define CROS_DISKS_DAEMON_H_

#include <base/basictypes.h>

#include "cros-disks/archive-manager.h"
#include "cros-disks/cros-disks-server-impl.h"
#include "cros-disks/device-ejector.h"
#include "cros-disks/device-event-moderator.h"
#include "cros-disks/disk-manager.h"
#include "cros-disks/format-manager.h"
#include "cros-disks/metrics.h"
#include "cros-disks/platform.h"
#include "cros-disks/power-manager-proxy.h"
#include "cros-disks/session-manager-proxy.h"

namespace cros_disks {

class Daemon {
 public:
  explicit Daemon(DBus::Connection* dbus_connection);
  ~Daemon();

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
  PowerManagerProxy power_manager_proxy_;
  SessionManagerProxy session_manager_proxy_;

  DISALLOW_COPY_AND_ASSIGN(Daemon);
};

}  // namespace cros_disks

#endif  // CROS_DISKS_DAEMON_H_
