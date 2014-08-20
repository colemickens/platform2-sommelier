// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWERD_SYSTEM_UDEV_SUBSYSTEM_OBSERVER_H_
#define POWER_MANAGER_POWERD_SYSTEM_UDEV_SUBSYSTEM_OBSERVER_H_

#include <string>

#include "power_manager/powerd/system/udev.h"

namespace power_manager {
namespace system {

// Interface for receiving notification of udev events from UdevInterface.
class UdevSubsystemObserver {
 public:
  virtual ~UdevSubsystemObserver() {}

  // Called when an event has been received from an observed subsystem.
  virtual void OnUdevEvent(const std::string& subsystem,
                           const std::string& sysname,
                           UdevAction action) = 0;
};

}  // namespace system
}  // namespace power_manager

#endif  // POWER_MANAGER_POWERD_SYSTEM_UDEV_SUBSYSTEM_OBSERVER_H_
