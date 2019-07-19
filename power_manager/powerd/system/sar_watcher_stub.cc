// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/system/sar_watcher_stub.h"
#include "power_manager/powerd/system/user_proximity_observer.h"

namespace power_manager {
namespace system {

void SarWatcherStub::AddObserver(UserProximityObserver* observer) {
  DCHECK(observer);
  observers_.AddObserver(observer);
}

void SarWatcherStub::RemoveObserver(UserProximityObserver* observer) {
  DCHECK(observer);
  observers_.RemoveObserver(observer);
}

void SarWatcherStub::AddSensor(int id, uint32_t role) {
  for (auto& observer : observers_) {
    observer.OnNewSensor(id, role);
  }
}

void SarWatcherStub::SendEvent(int id, UserProximity proximity) {
  for (auto& observer : observers_) {
    observer.OnProximityEvent(id, proximity);
  }
}

}  // namespace system
}  // namespace power_manager
