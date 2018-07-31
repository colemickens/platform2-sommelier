// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWERD_SYSTEM_SAR_OBSERVER_H_
#define POWER_MANAGER_POWERD_SYSTEM_SAR_OBSERVER_H_

#include "power_manager/common/power_constants.h"

namespace power_manager {
namespace system {

// Interface for classes interested in observing events announced by the
// SarWatcher class.
class SarObserver {
 public:
  SarObserver() = default;
  virtual ~SarObserver() = default;

  // Called when a new proximity sensor is detected. |id| is a unique key
  // that will be used to identify this sensor in all future events; |roles|
  // represents a bitwise combination of SensorRole values.
  virtual void OnNewSensor(int id, uint32_t roles) = 0;

  // Called when a proximity sensor has new information to provide.
  virtual void OnProximityEvent(int id, UserProximity value) = 0;
};

}  // namespace system
}  // namespace power_manager

#endif  // POWER_MANAGER_POWERD_SYSTEM_SAR_OBSERVER_H_
