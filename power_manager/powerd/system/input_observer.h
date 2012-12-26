// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWERD_SYSTEM_INPUT_OBSERVER_H_
#define POWER_MANAGER_POWERD_SYSTEM_INPUT_OBSERVER_H_

namespace power_manager {
namespace system {

// Interface for classes interested in observing input events announced by the
// Input class.
class InputObserver {
 public:
  virtual ~InputObserver() {}

  // Called when an input event is received.
  virtual void OnInputEvent(InputType type, int state) = 0;
};

}  // namespace system
}  // namespace power_manager

#endif  // POWER_MANAGER_POWERD_SYSTEM_INPUT_OBSERVER_H_
