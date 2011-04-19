// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_MONITOR_RECONFIGURE_H_
#define POWER_MANAGER_MONITOR_RECONFIGURE_H_

#include <map>
#include <vector>

#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>

#include "power_manager/resolution_selector.h"
#include "power_manager/udev_controller.h"

namespace power_manager {

class BacklightController;
class MonitorReconfigure;

// Delegate class for udev events.
class MonitorReconfigureDelegate : public UdevDelegate {
 public:
  MonitorReconfigureDelegate(MonitorReconfigure* monitor_reconfigure)
    : monitor_reconfigure_(monitor_reconfigure) {}

  virtual void Run(GIOChannel* source, GIOCondition condition);
 private:
  MonitorReconfigure* monitor_reconfigure_;
};

// MonitorReconfigure is the class responsible for setting the external
// monitor to the max resolution based on the modes supported by the native
// monitor and the external monitor.
class MonitorReconfigure {
 public:
  // We need a default constructor for unit tests.
  MonitorReconfigure();
  // |backlight_ctl| is a pointer to the backlight controller for the
  // internal screen.
  MonitorReconfigure(BacklightController* backlight_ctl);
  ~MonitorReconfigure();

  // Initialization.
  bool Init();

  // Main entry point.
  void Run();

 private:
  // Initializes the |lcd_output_| and |external_output_| members.
  void DetermineOutputs();

  // Returns whether an external monitor is connected.
  bool IsExternalMonitorConnected();

  // Sorts |output_info|'s modes by decreasing number of pixels, storing the
  // results in |modes_out|.
  void SortModesByResolution(RROutput output,
                             std::vector<ResolutionSelector::Mode>* modes_out);

  // Set the resolution for a particular device or for the screen.
  bool SetDeviceResolution(RROutput output,
                           const XRROutputInfo* output_info,
                           const ResolutionSelector::Mode& resolution);
  bool SetScreenResolution(const ResolutionSelector::Mode& resolution);

  // Disable output to a device.
  bool DisableDevice(const XRROutputInfo* output_info);

  // Mapping between mode XIDs and mode information structures.
  std::map<int, XRRModeInfo*> mode_map_;

  // X Resources needed between functions.
  Display* display_;
  Window window_;
  XRRScreenResources* screen_info_;

  RROutput lcd_output_;
  XRROutputInfo* lcd_output_info_;

  RROutput external_output_;
  XRROutputInfo* external_output_info_;

  // Not owned.
  BacklightController* backlight_ctl_;

  // Delegate for the Udev controller.
  MonitorReconfigureDelegate* delegate_;

  // Udev controller.
  UdevController* controller_;

  DISALLOW_COPY_AND_ASSIGN(MonitorReconfigure);
};

}  // namespace power_manager

#endif  // POWER_MANAGER_MONITOR_RECONFIGURE_H_
