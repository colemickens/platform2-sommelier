// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
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
#include "base/time.h"
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
  enum DualHeadMode {
    kModeClone,
    kModeFirstMonitorPrimary,
    kModeSecondMonitorPrimary,
    kModeLast,
  };

  // We need a default constructor for unit tests.
  MonitorReconfigure();

  // |backlight_ctl| is a pointer to the backlight controller for the internal
  // screen.
  MonitorReconfigure(BacklightController* backlight_ctl);

  virtual ~MonitorReconfigure();

  // Initialization.
  bool Init();

  // Main entry point.
  void Run(bool force_reconfigure);

  // Shortcut Ctrl+F4 will trigger SwitchMode() to cycle through different
  // dual head modes.
  void SwitchMode();

  // Returns whether an external monitor is connected.
  virtual bool is_projecting() const { return is_projecting_; }

  // Sets projection callback function and data.
  void SetProjectionCallback(void (*func)(void*), void* data);

  // Public interface for turning on/off the screens (displays).
  void SetScreenOn();
  void SetScreenOff();

 private:
  // Get the XRRModeInfo for |mode|.
  XRRModeInfo* GetModeInfo(RRMode mode);

  // Setup dual head mode when there are 2 outputs available.
  void RunExtended();

  // Check whether resolution |width|x|height| is supported.
  bool CheckValidResolution(int width, int height);

  // Fall back to single head mode when RunExtended() can't configure
  // dual head mode.
  void FallBackToSingleHead();

  // If there is only 1 output, it will be enabled with |lcd_resolution1|.
  // If there are 2 outpus, Clone mode will be setup and each output will
  // be enabled with |lcd_resolution| and |external_resolution| accordingly.
  void RunClone(const ResolutionSelector::Mode& lcd_resolution,
                const ResolutionSelector::Mode& external_resolution,
                const ResolutionSelector::Mode& screen_resolution);

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

  // Policy about whether the reconfigure is needed.
  bool NeedReconfigure();

  // Disable the output.
  void DisableOutput(XRROutputInfo* output_info);

  // Disables all the the outputs (both connected and disconnected).
  void DisableAllOutputs();

  // Enables the |idx|th output from |usable_outputs_| with resolution
  // |mode|. If |position_x| is not NULL, then the scan out crtc for
  // this output has x offset |position_x| in the fb, otherwise the
  // x offset within the fb is 0. The same policy applies to |position_y|.
  void EnableUsableOutput(int idx, RRMode mode,
                          int* position_x, int* position_y);

  // Get the number of connected outputs.
  int GetConnectedOutputsNum();

  // Set the the state of |lvds_connection_|.
  void RecordLVDSConnection();

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

  // The list of usable (connected) outputs and their info and assigned crtc.
  std::vector<RROutput> usable_outputs_;
  std::vector<XRROutputInfo*> usable_outputs_info_;
  std::vector<RRCrtc> usable_outputs_crtc_;

  // Number of connected outputs.
  int noutput_;

  // The timestamp of last monitor reconfiguration.
  base::Time last_configuration_time_;

  // The status of LVDS connection:
  // RR_Connected, RR_Disconnected, RR_UnknownConnection.
  Connection lvds_connection_;

  // Current dual head mode.
  DualHeadMode dual_head_mode_;

  // Whether dual head mode should be switched when Run() is called. It is
  // set to be true when monitor reconfigure is triggered by pressing Ctrl+F4.
  bool need_switch_mode_;

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
