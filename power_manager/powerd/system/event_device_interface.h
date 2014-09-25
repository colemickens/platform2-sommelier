// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWERD_SYSTEM_EVENT_DEVICE_INTERFACE_H_
#define POWER_MANAGER_POWERD_SYSTEM_EVENT_DEVICE_INTERFACE_H_

#include <string>
#include <vector>

#include <base/callback_forward.h>
#include <base/macros.h>
#include <base/memory/linked_ptr.h>

#include "power_manager/common/power_constants.h"

struct input_event;  // from <linux/input.h>

namespace power_manager {
namespace system {

// Provides methods to access event devices, i.e. the device files exposed by
// the kernel evdev interface: /dev/input/eventN.
class EventDeviceInterface {
 public:
  EventDeviceInterface() {}
  virtual ~EventDeviceInterface() {}

  // Returns a human-readable identifier to be used for debugging.
  virtual std::string GetDebugName() = 0;

  // Returns the physical path of the device.
  // TODO(patrikf): Consider using udev and tags instead.
  virtual std::string GetPhysPath() = 0;

  // Returns true if the device can report lid events.
  virtual bool IsLidSwitch() = 0;

  // Returns true if the device can report power button events.
  virtual bool IsPowerButton() = 0;

  // Returns the current state of the lid switch or LID_NOT_PRESENT on error.
  // May not be called after ReadEvents() or WatchForEvents().
  virtual LidState GetInitialLidState() = 0;

  // Reads a number of events into |events_out|. Returns true if the operation
  // was successful and events were present.
  virtual bool ReadEvents(std::vector<input_event>* events_out) = 0;

  // Start watching this device for incoming events, and run |new_events_cb|
  // when events are ready to be read with ReadEvents(). Shall only be called
  // once.
  virtual void WatchForEvents(base::Closure new_events_cb) = 0;
};

class EventDeviceFactoryInterface {
 public:
  EventDeviceFactoryInterface() {}
  virtual ~EventDeviceFactoryInterface() {}

  // Opens an event device by path. Returns the device or NULL on error.
  virtual linked_ptr<EventDeviceInterface> Open(const std::string& path) = 0;
};

}  // namespace system
}  // namespace power_manager

#endif  // POWER_MANAGER_POWERD_SYSTEM_EVENT_DEVICE_INTERFACE_H_
