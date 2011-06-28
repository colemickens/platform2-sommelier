// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CROS_DISKS_DEVICE_EVENT_H_
#define CROS_DISKS_DEVICE_EVENT_H_

#include <string>

namespace cros_disks {

// A simple data structure for holding a device event.
struct DeviceEvent {
  enum EventType {
    kIgnored,
    kDeviceAdded,
    kDeviceScanned,
    kDeviceRemoved,
    kDiskAdded,
    kDiskAddedAfterRemoved,
    kDiskChanged,
    kDiskRemoved,
  };

  DeviceEvent()
    : event_type(kIgnored) {
  }

  DeviceEvent(EventType type, const std::string& path)
    : event_type(type),
      device_path(path) {
  }

  // Returns true if the event type is DiskAdded, DiskAddedAfterRemoved,
  // DiskChanged or DiskRemoved.
  bool IsDiskEvent() const;

  EventType event_type;
  std::string device_path;
};

}  // namespace cros_disks

#endif  // CROS_DISKS_DEVICE_EVENT_H_
