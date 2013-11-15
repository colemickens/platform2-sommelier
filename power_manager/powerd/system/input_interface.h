// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWERD_SYSTEM_INPUT_INTERFACE_H_
#define POWER_MANAGER_POWERD_SYSTEM_INPUT_INTERFACE_H_

#include "base/basictypes.h"
#include "power_manager/common/power_constants.h"

namespace power_manager {
namespace system {

class InputObserver;

// An interface for querying vaguely-input-related state.
class InputInterface {
 public:
  InputInterface() {}
  virtual ~InputInterface() {}

  // Adds or removes an observer.
  virtual void AddObserver(InputObserver* observer) = 0;
  virtual void RemoveObserver(InputObserver* observer) = 0;

  // Queries the system for the current lid state. LID_NOT_PRESENT is
  // returned on error.
  virtual LidState QueryLidState() = 0;

  // Checks if any USB input devices are connected.
  virtual bool IsUSBInputDeviceConnected() const = 0;

  // Returns true if a display (internal or external) is connected to the
  // device.
  virtual bool IsDisplayConnected() const = 0;

  // Returns the (1-indexed) number of the currently-active virtual terminal.
  virtual int GetActiveVT() = 0;

  // Enables or disables special wakeup input devices.
  virtual bool SetWakeInputsState(bool enable) = 0;

  // Enables or disables touch devices.
  virtual void SetTouchDevicesState(bool enable) = 0;

  DISALLOW_COPY_AND_ASSIGN(InputInterface);
};

}  // namespace system
}  // namespace power_manager

#endif  // POWER_MANAGER_POWERD_SYSTEM_INPUT_INTERFACE_H_
