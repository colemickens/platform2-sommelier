// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_MONITOR_RECONFIGURE_H_
#define POWER_MANAGER_MONITOR_RECONFIGURE_H_

#include <gdk/gdkevents.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>

#include <map>
#include <set>
#include <vector>

#include "base/basictypes.h"
#include "power_manager/resolution_selector.h"
#include "power_manager/signal_callback.h"

namespace power_manager {

class BacklightController;
class MonitorReconfigure;

// MonitorReconfigure is the class responsible for setting the external monitor
// to the max resolution based on the modes supported by the native monitor and
// the external monitor.
class MonitorReconfigure {
 public:
  // We need a default constructor for unit tests.
  MonitorReconfigure();

  // |backlight_ctl| is a pointer to the backlight controller for the internal
  // screen.
  MonitorReconfigure(BacklightController* backlight_ctl);

  virtual ~MonitorReconfigure();

  // Initialization.
  bool Init();

  // Main entry point.
  void Run();

  // Returns whether an external monitor is connected.
  virtual bool is_projecting() const { return is_projecting_; }

  // Sets projection callback function and data.
  void SetProjectionCallback(void (*func)(void*), void* data);

 private:
  // Get the XRRModeInfo for |mode|.
  XRRModeInfo* GetModeInfo(RRMode mode);

  // Find a usable Crtc, i.e. one in |crtcs| but not in |used_crtcs|.
  RRCrtc FindUsableCrtc(const std::set<RRCrtc>& used_crtcs,
                        const RRCrtc* crtcs,
                        int ncrtcs);

  // Initializes the |usable_outputs_| and |usable_outputs_info_| members.
  void DetermineOutputs();

  // Sorts |output_info|'s modes by decreasing number of pixels, storing the
  // results in |modes_out|.
  void SortModesByResolution(RROutput output,
                             std::vector<ResolutionSelector::Mode>* modes_out);

  // Set the resolution for the screen.
  bool SetScreenResolution(const ResolutionSelector::Mode& resolution);

  // Disable output and all of its CRTCs.
  void DisableOutput(XRROutputInfo* output_info);

  // Disable the LCD panel output.
  void DisableLCDOutput();

  // Disables all disconnected outputs.
  void DisableDisconnectedOutputs();

  // Enables all connected and usable outputs.
  void EnableUsableOutputs(const std::vector<RRMode>& resolutions);

  // Callback to watch for xrandr hotplug events.
  SIGNAL_CALLBACK_2(MonitorReconfigure, GdkFilterReturn, GdkEventFilter,
                    GdkXEvent*, GdkEvent*);

  // Event and error base numbers for Xrandr.
  int rr_event_base_;
  int rr_error_base_;

  // Mapping between mode XIDs and mode information structures.
  std::map<int, XRRModeInfo*> mode_map_;

  // X Resources needed between functions.
  Display* display_;
  Window window_;
  XRRScreenResources* screen_info_;

  // The list of usable (connected) outputs and their info.
  std::vector<RROutput> usable_outputs_;
  std::vector<XRROutputInfo*> usable_outputs_info_;

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
