// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CROS_DISKS_DEVICE_EVENT_DISPATCHER_INTERFACE_H_
#define CROS_DISKS_DEVICE_EVENT_DISPATCHER_INTERFACE_H_

#include <base/basictypes.h>

namespace cros_disks {

class DeviceEvent;

// An interface class for dispatching device events.
class DeviceEventDispatcherInterface {
 public:
  virtual ~DeviceEventDispatcherInterface() {}

  // Implemented by a derived class to dispatch a device event.
  virtual void DispatchDeviceEvent(const DeviceEvent& event) = 0;
};

}  // namespace cros_disks

#endif  // CROS_DISKS_DEVICE_EVENT_DISPATCHER_INTERFACE_H_
