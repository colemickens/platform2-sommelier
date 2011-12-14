// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cros-disks/device-event.h"

namespace cros_disks {

bool DeviceEvent::operator==(const DeviceEvent& event) const {
  return event.event_type == event_type && event.device_path == device_path;
}

bool DeviceEvent::IsDiskEvent() const {
  switch (event_type) {
    case kDiskAdded:
    case kDiskChanged:
    case kDiskRemoved:
      return true;
    default:
      return false;
  }
}

}  // namespace cros_disks
