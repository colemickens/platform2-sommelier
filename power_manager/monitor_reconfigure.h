// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_MONITOR_RECONFIGURE_H_
#define POWER_MANAGER_MONITOR_RECONFIGURE_H_

#include "base/basictypes.h"

namespace power_manager {

class BacklightController;

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

// MonitorReconfigure is the class responsible for setting the external monitor
// to the max resolution based on the modes supported by the native monitor and
// the external monitor.
class MonitorReconfigure {
 public:
  // We need a default constructor for unit tests.
  MonitorReconfigure();

  // |backlight_ctl| is a pointer to the backlight controller for the internal
  // screen.
  explicit MonitorReconfigure(BacklightController* backlight_ctl);

  virtual ~MonitorReconfigure();

  // Initialization.
  bool Init();

  // Public interface for turning on/off all the screens (displays).
  void SetScreenOn();
  void SetScreenOff();

  // Public interface for turning on/off the internal panel.
  void SetInternalPanelOn();
  void SetInternalPanelOff();

 private:
  // Sends the DBus signal to set the screen power signal (which is caught by
  // Chrome to enable/disable outputs).
  // |power_state| If the state is to be set on or off.
  // |output_selection| Either internal output or all.
  void SendSetScreenPowerSignal(ScreenPowerState power_state,
      ScreenPowerOutputSelection output_selection);

  // Whether the internal panel output is enabled.
  bool is_internal_panel_enabled_;

  // Not owned.
  BacklightController* backlight_ctl_;

  DISALLOW_COPY_AND_ASSIGN(MonitorReconfigure);
};

}  // namespace power_manager

#endif  // POWER_MANAGER_MONITOR_RECONFIGURE_H_
