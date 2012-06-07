// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_MONITOR_RECONFIGURE_H_
#define POWER_MANAGER_MONITOR_RECONFIGURE_H_

#include <X11/extensions/Xrandr.h>

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

  // Sets the |is_projecting_| ivar, calling the projection callback if the
  // value changes.
  void SetIsProjecting(bool is_projecting);

  // Returns whether an external monitor is connected.
  virtual bool is_projecting() const { return is_projecting_; }

  // Sets projection callback function and data.
  void SetProjectionCallback(void (*func)(void*), void* data);

  // Public interface for turning on/off all the screens (displays).
  void SetScreenOn();
  void SetScreenOff();

  // Public interface for turning on/off the internal panel.
  void SetInternalPanelOn();
  void SetInternalPanelOff();

  // Return whether the device has panel output connected.
  bool HasInternalPanelConnection();

 private:
  // Setup |display_|, |window_|, |screen_info_| and |mode_map_| value.
  // Note:  Must be balanced with |ClearXrandr()|.
  bool SetupXrandr();

  // Clear |display_|, |window_|, |screen_info_| and |mode_map_| value.
  // Note:  Must be balanced with |SetupXrandr()|.
  void ClearXrandr();

  // Set the the state of |internal_panel_connection_|.
  void CheckInternalPanelConnection();

  // Sends the DBus signal to set the screen power signal (which is caught by
  // Chrome to enable/disable outputs).
  // |power_state| If the state is to be set on or off.
  // |output_selection| Either internal output or all.
  void SendSetScreenPowerSignal(ScreenPowerState power_state,
      ScreenPowerOutputSelection output_selection);

  // X Resources needed between functions.
  Display* display_;
  Window window_;
  XRRScreenResources* screen_info_;

  // The status of internal panel connection:
  // RR_Connected, RR_Disconnected, RR_UnknownConnection.
  Connection internal_panel_connection_;

  // Whether the internal panel output is enabled.
  bool is_internal_panel_enabled_;

  // Are we projecting?
  bool is_projecting_;

  // Callback that is invoked when projection status |is_projecting_| changes.
  void (*projection_callback_)(void*);
  // Used for passing data to the callback.
  void* projection_callback_data_;

  // Not owned.
  BacklightController* backlight_ctl_;

  DISALLOW_COPY_AND_ASSIGN(MonitorReconfigure);
};

}  // namespace power_manager

#endif  // POWER_MANAGER_MONITOR_RECONFIGURE_H_
