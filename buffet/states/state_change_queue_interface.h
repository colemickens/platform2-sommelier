// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BUFFET_STATES_STATE_CHANGE_QUEUE_INTERFACE_H_
#define BUFFET_STATES_STATE_CHANGE_QUEUE_INTERFACE_H_

#include <vector>

#include <base/time/time.h>
#include <chromeos/variant_dictionary.h>

namespace buffet {

// A simple notification record event to track device state changes.
// The |timestamp| records the time of the state change.
// |property_set| contains a property set with the new property values.
// The property set contains only the properties updated at the time the event
// was recorded.
struct StateChange {
  base::Time timestamp;
  chromeos::VariantDictionary property_set;
};

// An abstract interface to StateChangeQueue to record and retrieve state
// change notification events.
class StateChangeQueueInterface {
 public:
  // Returns true if the state change notification queue is empty.
  virtual bool IsEmpty() const = 0;

  // Called by StateManager when device state properties are updated.
  virtual bool NotifyPropertiesUpdated(const StateChange& change) = 0;

  // Returns the recorded state changes since last time this method was called.
  virtual std::vector<StateChange> GetAndClearRecordedStateChanges() = 0;

 protected:
  // No one should attempt do destroy the queue through the interface.
  ~StateChangeQueueInterface() {}
};

}  // namespace buffet

#endif  // BUFFET_STATES_STATE_CHANGE_QUEUE_INTERFACE_H_
