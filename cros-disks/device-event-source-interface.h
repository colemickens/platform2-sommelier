// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CROS_DISKS_DEVICE_EVENT_SOURCE_INTERFACE_H_
#define CROS_DISKS_DEVICE_EVENT_SOURCE_INTERFACE_H_

namespace cros_disks {

class DeviceEvent;

// An interface class for producing device events.
class DeviceEventSourceInterface {
 public:
  virtual ~DeviceEventSourceInterface() {}

  // Implemented by a derived class to return the next available device event
  // in |event|. Returns false on error or if not device event is available.
  virtual bool GetDeviceEvent(DeviceEvent* event) = 0;
};

}  // namespace cros_disks

#endif  // CROS_DISKS_DEVICE_EVENT_SOURCE_INTERFACE_H_
