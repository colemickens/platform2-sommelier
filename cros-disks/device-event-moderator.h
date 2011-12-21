// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CROS_DISKS_DEVICE_EVENT_MODERATOR_H_
#define CROS_DISKS_DEVICE_EVENT_MODERATOR_H_

#include <string>

#include <base/basictypes.h>

#include "cros-disks/device-event-queue.h"
#include "cros-disks/power-manager-observer-interface.h"
#include "cros-disks/session-manager-observer-interface.h"

namespace cros_disks {

class DeviceEventDispatcherInterface;
class DeviceEventSourceInterface;

// A class for moderating device events by retrieving events from an event
// source and dispatching them through a dispatcher at appropriate moments.
//
// Device events are dispatched immediately only during an active user session.
// After a user session ends or the screen is locked, any received device
// event is temporarily queued and only dispatched after a new user session
// starts or the screen is unlocked. This is to minimize the chance of device
// insertion attacks when the system is not actively used.
class DeviceEventModerator : public PowerManagerObserverInterface,
                             public SessionManagerObserverInterface {
 public:
  DeviceEventModerator(DeviceEventDispatcherInterface* event_dispatcher,
                       DeviceEventSourceInterface* event_source);

  virtual ~DeviceEventModerator();

  // Dispatches all queued device events through the event dispatcher.
  void DispatchQueuedDeviceEvents();

  // Implements the PowerManagerObserverInterface interface to handle
  // the event when the screen is locked.
  virtual void OnScreenIsLocked();

  // Implements the PowerManagerObserverInterface interface to handle
  // the event when the screen is unlocked.
  virtual void OnScreenIsUnlocked();

  // Implements the SessionManagerObserverInterface interface to handle
  // the event when the session has been started.
  virtual void OnSessionStarted(const std::string& user);

  // Implements the SessionManagerObserverInterface interface to handle
  // the event when the session has been stopped.
  virtual void OnSessionStopped(const std::string& user);

  // Process the available device events from the event source.
  void ProcessDeviceEvents();

  bool is_event_queued() const { return is_event_queued_; }

 private:
  // An object that dispatches device events.
  DeviceEventDispatcherInterface* event_dispatcher_;

  // An object that queues up device events when the system is not active.
  DeviceEventQueue event_queue_;

  // An object from which device events are retrieved.
  DeviceEventSourceInterface* event_source_;

  // This variable is set to true if any new device event should be queued
  // instead of being dispatched immediately.
  bool is_event_queued_;

  DISALLOW_COPY_AND_ASSIGN(DeviceEventModerator);
};

}  // namespace cros_disks

#endif  // CROS_DISKS_DEVICE_EVENT_MODERATOR_H_
