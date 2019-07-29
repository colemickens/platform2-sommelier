// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cros-disks/device_event_moderator.h"

#include <base/logging.h>

#include "cros-disks/device_event_dispatcher_interface.h"
#include "cros-disks/device_event_source_interface.h"
#include "cros-disks/quote.h"

namespace cros_disks {

DeviceEventModerator::DeviceEventModerator(
    DeviceEventDispatcherInterface* event_dispatcher,
    DeviceEventSourceInterface* event_source,
    bool dispatch_initially)
    : event_dispatcher_(event_dispatcher),
      event_source_(event_source),
      is_event_queued_(dispatch_initially) {
  CHECK(event_dispatcher_) << "Invalid event dispatcher object";
  CHECK(event_source_) << "Invalid event source object";
}

void DeviceEventModerator::DispatchQueuedDeviceEvents() {
  const DeviceEvent* event;
  while ((event = event_queue_.Head()) != nullptr) {
    LOG(INFO) << "Dispatch queued event type:" << event->event_type
              << " device:" << quote(event->device_path);
    event_dispatcher_->DispatchDeviceEvent(*event);
    event_queue_.Remove();
  }
}

void DeviceEventModerator::OnScreenIsLocked() {
  LOG(INFO) << "Screen is locked. Device events are now queued.";
  is_event_queued_ = true;
}

void DeviceEventModerator::OnScreenIsUnlocked() {
  LOG(INFO) << "Screen is locked. Queued device events are now dispatched.";
  DispatchQueuedDeviceEvents();
  is_event_queued_ = false;
}

void DeviceEventModerator::OnSessionStarted() {
  LOG(INFO) << "Session started. Queued device events are now dispatched.";
  DispatchQueuedDeviceEvents();
  is_event_queued_ = false;
}

void DeviceEventModerator::OnSessionStopped() {
  LOG(INFO) << "Session stopped. Device events are now queued.";
  is_event_queued_ = true;
}

void DeviceEventModerator::ProcessDeviceEvents() {
  DeviceEventList events;
  if (event_source_->GetDeviceEvents(&events)) {
    if (is_event_queued_) {
      for (const auto& event : events) {
        event_queue_.Add(event);
      }
    } else {
      for (const auto& event : events) {
        event_dispatcher_->DispatchDeviceEvent(event);
      }
    }
  }
}

}  // namespace cros_disks
