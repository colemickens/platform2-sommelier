// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_MONITOR_RECONFIGURE_H_
#define POWER_MANAGER_MONITOR_RECONFIGURE_H_

#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/time.h"
#include "power_manager/resolution_selector.h"
#include "power_manager/signal_callback.h"
#include "power_manager/xevent_observer.h"

namespace power_manager {

class BacklightController;
class MonitorReconfigure;

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
class MonitorReconfigure : public XEventObserverInterface {
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
  explicit MonitorReconfigure(BacklightController* backlight_ctl);

  virtual ~MonitorReconfigure();

  // Initialization.
  bool Init();

  // Main entry point.
  void Run(bool force_reconfigure);

  // Shortcut Ctrl+F4 will trigger SwitchMode() to cycle through different
  // dual head modes.
  void SwitchMode();

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
  struct ConnectionState {
    ConnectionState(RRCrtc crtc, RRMode mode, int position_x, int position_y);
    ConnectionState();

    RRCrtc crtc;
    RRMode mode;
    int position_x;
    int position_y;
  };

  struct ModeInfo {
    unsigned int width;
    unsigned int height;
    unsigned long dotClock;
  };

  struct ModeInfoComparator {
    bool operator()(const ModeInfo& mode_a, const ModeInfo& mode_b) const {
      if (mode_a.width * mode_a.height > mode_b.width * mode_b.height)
        return true;
      else if (mode_a.width * mode_a.height < mode_b.width * mode_b.height)
        return false;
      else
        return mode_a.dotClock > mode_b.dotClock;
    }
  };

  struct OutputInfo {
    std::string name;
    std::vector<ModeInfo> modes;
    bool configured;
  };

  // Setup |display_|, |window_|, |screen_info_| and |mode_map_| value.
  // Note:  Must be balanced with |ClearXrandr()|.
  bool SetupXrandr();

  // Clear |display_|, |window_|, |screen_info_| and |mode_map_| value.
  // Note:  Must be balanced with |SetupXrandr()|.
  void ClearXrandr();

  // Clear |usable_outputs_|, |usable_outputs_info_| and |usable_outputs_crtc_|.
  void ClearUsableOutputsInfo();

  // Get the XRRModeInfo for |mode|.  This method can return NULL if the mode
  // isn't known to |screen_info_| but most callers should CHECK against that
  // case.
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
  void DetermineOutputs(std::vector<OutputInfo>* current_outputs);

  // Sorts |output_info|'s modes by decreasing number of pixels, storing the
  // results in |modes_out|.
  void SortModesByResolution(RROutput output,
                             std::vector<ResolutionSelector::Mode>* modes_out);

  // Set the resolution for the screen.
  bool SetScreenResolution(const ResolutionSelector::Mode& resolution);

  // Compare whether two sets of output are the same, if they have the same
  // number of outputs with the same name, and each output support the same
  // sets of modes.
  bool AreOutputsSame(const std::vector<OutputInfo>& outputs1,
                      const std::vector<OutputInfo>& outputs2);

  // Policy about whether mode switch is needed;
  bool NeedSwitchMode();

  // Policy about whether the reconfigure is needed.
  bool NeedReconfigure(const std::vector<OutputInfo>& current_outputs);

  // Disable the output on the specified |crtc|.
  void DisableOutputCrtc(XRROutputInfo* output_info, RRCrtc crtc);

  // Disable the output on all its usable crtcs.
  void DisableOutput(XRROutputInfo* output_info);

  // Disables all the the outputs (both connected and disconnected).
  void DisableAllOutputs();

  // Enables the |idx|th output from |usable_outputs_| with resolution
  // |mode|. If |position_x| is not NULL, then the scan out crtc for
  // this output has x offset |position_x| in the fb, otherwise the
  // x offset within the fb is 0. The same policy applies to |position_y|.
  // Note:  This method CHECKs that |mode| is known to the screen_info_.
  void EnableUsableOutput(int idx, RRMode mode,
                          int* position_x, int* position_y);

  // Set the the state of |internal_panel_connection_|.
  void CheckInternalPanelConnection();

  // Callback to watch for xrandr hotplug events.
  // Inherited from XEventObserver.
  virtual XEventHandlerStatus HandleXEvent(XEvent* event) OVERRIDE;

  // Sends the DBus signal to set the screen power signal (which is caught by
  // Chrome to enable/disable outputs).
  // |power_state| If the state is to be set on or off.
  // |output_selection| Either internal output or all.
  void SendSetScreenPowerSignal(ScreenPowerState power_state,
      ScreenPowerOutputSelection output_selection);

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

  // Saved information about connected outputs. Used to compare with current
  // connected outputs to decide whether reconfigure is needed.
  std::vector<OutputInfo> saved_outputs_;

  // The timestamp of last monitor reconfiguration.
  base::Time last_configuration_time_;

  // The status of internal panel connection:
  // RR_Connected, RR_Disconnected, RR_UnknownConnection.
  Connection internal_panel_connection_;

  // Saved internal panel connection state.
  ConnectionState internal_panel_state_;

  // Whether the internal panel output is enabled.
  bool is_internal_panel_enabled_;

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
