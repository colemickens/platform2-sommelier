// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWERD_MONITOR_RECONFIGURE_H_
#define POWER_MANAGER_POWERD_MONITOR_RECONFIGURE_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"

namespace power_manager {

enum ScreenPowerOutputSelection {
  OUTPUT_SELECTION_ALL_DISPLAYS,
  OUTPUT_SELECTION_INTERNAL_ONLY,
};

enum ScreenPowerState {
  POWER_STATE_ON,
  POWER_STATE_OFF,
};

class MonitorReconfigureInterface {
 public:
  MonitorReconfigureInterface() {}
  virtual ~MonitorReconfigureInterface() {}

  // Manually sets the internal panel status flag, which is initialized to true
  // by default.  The panel may be in a different state at startup.
  virtual void set_is_internal_panel_enabled(bool enabled) = 0;

  // Sends a D-Bus signal to Chrome telling to set the outputs described by
  // |selection| to |state|.
  virtual void SetScreenPowerState(ScreenPowerOutputSelection selection,
                                   ScreenPowerState state) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(MonitorReconfigureInterface);
};

// Sends messages to Chrome to turn displays on or off.
class MonitorReconfigure : public MonitorReconfigureInterface {
 public:
  MonitorReconfigure();
  virtual ~MonitorReconfigure();

  // MonitorReconfigureInterface implementation:
  virtual void set_is_internal_panel_enabled(bool enabled) OVERRIDE {
    is_internal_panel_enabled_ = enabled;
  }
  virtual void SetScreenPowerState(ScreenPowerOutputSelection selection,
                                   ScreenPowerState state) OVERRIDE;

 private:
  // Whether the internal panel output is enabled.
  bool is_internal_panel_enabled_;

  DISALLOW_COPY_AND_ASSIGN(MonitorReconfigure);
};

}  // namespace power_manager

#endif  // POWER_MANAGER_POWERD_MONITOR_RECONFIGURE_H_
