// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWERD_MONITOR_RECONFIGURE_H_
#define POWER_MANAGER_POWERD_MONITOR_RECONFIGURE_H_

#include "base/basictypes.h"

namespace power_manager {

enum ScreenPowerState {
  POWER_STATE_INVALID,
  POWER_STATE_ON,
  POWER_STATE_OFF,
};

enum ScreenPowerOutputSelection {
  OUTPUT_SELECTION_INVALID,
  OUTPUT_SELECTION_ALL_DISPLAYS,
  OUTPUT_SELECTION_INTERNAL_ONLY,
};

// Sends messages to Chrome to turn displays on or off.
class MonitorReconfigure {
 public:
  MonitorReconfigure();
  virtual ~MonitorReconfigure();

  // Manually sets the internal panel status flag, which is initialized to true
  // by default.  The panel may be in a different state at startup.
  void set_is_internal_panel_enabled(bool enabled) {
    is_internal_panel_enabled_ = enabled;
  }

  // Sends a D-Bus signal to Chrome telling to set the outputs described by
  // |selection| to |state|.
  void SetScreenPowerState(ScreenPowerOutputSelection selection,
                           ScreenPowerState state);

 private:
  // Whether the internal panel output is enabled.
  bool is_internal_panel_enabled_;

  DISALLOW_COPY_AND_ASSIGN(MonitorReconfigure);
};

}  // namespace power_manager

#endif  // POWER_MANAGER_POWERD_MONITOR_RECONFIGURE_H_
