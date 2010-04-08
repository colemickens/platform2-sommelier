// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_IDLE_DIMMER_H_
#define POWER_MANAGER_IDLE_DIMMER_H_

#include "power_manager/backlight_interface.h"
#include "power_manager/xidle_monitor.h"

namespace power_manager {

// Adjusts the backlight based on whether the user is idle.
class IdleDimmer : public XIdleMonitor {
 public:
  // Constructor. When the user becomes idle, use the provided backlight
  // object to set the brightness to idle_brightness. When the user becomes
  // active again, restore the user's brightness settings to normal.
  explicit IdleDimmer(int64 idle_brightness, BacklightInterface* backlight);

  void OnIdleEvent(bool is_idle, int64 idle_time_ms);

 private:
  // Backlight used for idle dimming. Non-owned.
  BacklightInterface* backlight_;

  // Whether the monitor has been dimmed due to inactivity.
  bool idle_dim_;

  // The brightness level when the user is idle or active.
  int64 idle_brightness_;
  int64 active_brightness_;
};

}  // namespace power_manager

#endif  // POWER_MANAGER_IDLE_DIMMER_H_
