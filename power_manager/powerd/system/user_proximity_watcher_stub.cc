// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/system/user_proximity_observer.h"
#include "power_manager/powerd/system/user_proximity_watcher_stub.h"

namespace power_manager {
namespace system {

void UserProximityWatcherStub::AddObserver(UserProximityObserver* observer) {
  DCHECK(observer);
  observers_.AddObserver(observer);
}

void UserProximityWatcherStub::RemoveObserver(UserProximityObserver* observer) {
  DCHECK(observer);
  observers_.RemoveObserver(observer);
}

void UserProximityWatcherStub::AddSensor(int id, uint32_t role) {
  for (auto& observer : observers_) {
    observer.OnNewSensor(id, role);
  }
}

void UserProximityWatcherStub::SendEvent(int id, UserProximity proximity) {
  for (auto& observer : observers_) {
    observer.OnProximityEvent(id, proximity);
  }
}

}  // namespace system
}  // namespace power_manager
