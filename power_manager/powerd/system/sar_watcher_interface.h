// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWERD_SYSTEM_SAR_WATCHER_INTERFACE_H_
#define POWER_MANAGER_POWERD_SYSTEM_SAR_WATCHER_INTERFACE_H_

#include <base/macros.h>

namespace power_manager {
namespace system {

class UserProximityObserver;

// An interface for querying proximity information via Specific Absorption
// Rate (SAR) sensors.
class SarWatcherInterface {
 public:
  SarWatcherInterface() = default;
  virtual ~SarWatcherInterface() = default;

  // Adds or removes an observer.
  virtual void AddObserver(UserProximityObserver* observer) = 0;
  virtual void RemoveObserver(UserProximityObserver* observer) = 0;

  // TODO(egranata): add querying mechanisms
 private:
  DISALLOW_COPY_AND_ASSIGN(SarWatcherInterface);
};

}  // namespace system
}  // namespace power_manager

#endif  // POWER_MANAGER_POWERD_SYSTEM_SAR_WATCHER_INTERFACE_H_
