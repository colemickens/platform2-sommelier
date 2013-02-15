// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWERD_SYSTEM_INPUT_OBSERVER_H_
#define POWER_MANAGER_POWERD_SYSTEM_INPUT_OBSERVER_H_

#include "power_manager/common/power_constants.h"

namespace power_manager {
namespace system {

// Interface for classes interested in observing input events announced by the
// Input class.
class InputObserver {
 public:
  virtual ~InputObserver() {}

  // Called when the lid is opened or closed.
  virtual void OnLidEvent(LidState state) = 0;

  // Called when a power button event occurs.
  virtual void OnPowerButtonEvent(ButtonState state) = 0;
};

}  // namespace system
}  // namespace power_manager

#endif  // POWER_MANAGER_POWERD_SYSTEM_INPUT_OBSERVER_H_
