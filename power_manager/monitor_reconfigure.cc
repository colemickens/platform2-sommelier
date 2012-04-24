// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/monitor_reconfigure.h"

#include <sys/sysinfo.h>

#include <gdk/gdkx.h>
#include <X11/extensions/dpms.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>

#include "base/logging.h"
#include "base/string_util.h"
#include "power_manager/backlight_controller.h"

using std::max;
using std::sort;
using std::vector;
using base::Time;
using base::TimeDelta;

namespace {

// Name of the internal output.
const char kInternalPanelName[] = "LVDS";
// The screen DPI we pass to X11.
const float kScreenDpi = 96.0;
// An inch in mm.
const float kInchInMm = 25.4;
// When doing monitor recofigure, if system uptime is less than
// system_uptime_threshold, we consider it is the first time powerd runs
// after boot.
const long kSystemUptimeThreshold = 30;

const char* RRNotifySubtypeToString(int subtype) {
  switch (subtype) {
    case RRNotify_OutputChange:
      return "RRNotify_OutputChange";
    case RRNotify_CrtcChange:
      return "RRNotify_CrtcChange";
    case RRNotify_OutputProperty:
      return "RRNotify_OutputProperty";
    default:
      return "Unknown RRNotify";
  }
}

// TODO(miletus@) : May need a different implementation for Arrow.
bool IsInternalPanel(XRROutputInfo* output_info) {
  return StartsWithASCII(output_info->name, kInternalPanelName, true);
}

}  // namespace

namespace power_manager {

MonitorReconfigure::ConnectionState::ConnectionState(
    RRCrtc crtc,
    RRMode mode,
    int position_x,
    int position_y)
    : crtc(crtc),
      mode(mode),
      position_x(position_x),
      position_y(position_y) {
}

MonitorReconfigure::ConnectionState::ConnectionState()
    : crtc(0),
      mode(0),
      position_x(-1),
      position_y(-1) {
}

MonitorReconfigure::MonitorReconfigure()
    : display_(NULL),
      window_(None),
      screen_info_(NULL),
      last_configuration_time_(Time::Now()),
      internal_panel_connection_(RR_UnknownConnection),
      is_internal_panel_enabled_(true),
      dual_head_mode_(kModeClone),
      need_switch_mode_(false),
      is_projecting_(false),
      projection_callback_(NULL),
      projection_callback_data_(NULL),
      backlight_ctl_(NULL) {
}

MonitorReconfigure::MonitorReconfigure(
    BacklightController* backlight_ctl)
    : display_(NULL),
      window_(None),
      screen_info_(NULL),
      last_configuration_time_(Time::Now()),
      internal_panel_connection_(RR_UnknownConnection),
      is_internal_panel_enabled_(true),
      dual_head_mode_(kModeClone),
      need_switch_mode_(false),
      is_projecting_(false),
      projection_callback_(NULL),
      projection_callback_data_(NULL),
      backlight_ctl_(backlight_ctl) {
}

MonitorReconfigure::~MonitorReconfigure() {
}

bool MonitorReconfigure::Init() {
  if (!XRRQueryExtension(GDK_DISPLAY(), &rr_event_base_, &rr_error_base_))
    return false;

  int rr_major_version, rr_minor_version;
  XRRQueryVersion(GDK_DISPLAY(), &rr_major_version, &rr_minor_version);
  XRRSelectInput(GDK_DISPLAY(),
                 DefaultRootWindow(GDK_DISPLAY()),
                 RRScreenChangeNotifyMask | RROutputChangeNotifyMask);

  gdk_window_add_filter(NULL, GdkEventFilterThunk, this);
  LOG(INFO) << "XRandr event filter added";

  if (backlight_ctl_)
    backlight_ctl_->SetMonitorReconfigure(this);

  CheckInternalPanelConnection();

  // Call Run() on startup to setup screens and initialize display info.
  Run(false);

  return true;
}

bool MonitorReconfigure::SetupXrandr() {
  CHECK(NULL == display_);
  display_ = XOpenDisplay(NULL);

  // Give up if we can't open the display.
  if (!display_) {
    LOG(WARNING) << "Could not open display";
    return false;
  }

  CHECK(None == window_);
  window_ = RootWindow(display_, DefaultScreen(display_));
  CHECK(NULL == screen_info_);
  screen_info_ = XRRGetScreenResources(display_, window_);

  // Give up if we can't obtain the XRandr information.
  if (!screen_info_) {
    LOG(WARNING) << "Could not get XRandr information";
    XCloseDisplay(display_);
    display_ = NULL;
    window_ = None;
    return false;
  }

  for (int i = 0; i < screen_info_->nmode; ++i) {
    XRRModeInfo* current_mode = &screen_info_->modes[i];
    mode_map_[current_mode->id] = current_mode;
  }

  return true;
}

void MonitorReconfigure::ClearXrandr() {
  CHECK(NULL != screen_info_);
  XRRFreeScreenResources(screen_info_);
  screen_info_ = NULL;
  CHECK(NULL != display_);
  XCloseDisplay(display_);
  display_ = NULL;
  CHECK(None != window_);
  window_ = None;
  mode_map_.clear();
}

void MonitorReconfigure::ClearUsableOutputsInfo() {
  usable_outputs_.clear();
  for (unsigned i = 0; i < usable_outputs_info_.size(); i++)
    if (usable_outputs_info_[i])
      XRRFreeOutputInfo(usable_outputs_info_[i]);
  usable_outputs_info_.clear();
  usable_outputs_crtc_.clear();
}

XRRModeInfo* MonitorReconfigure::GetModeInfo(RRMode mode) {
  for (int i = 0; i < screen_info_->nmode; i++)
    if (screen_info_->modes[i].id == mode)
      return &screen_info_->modes[i];

  LOG(WARNING) << "mode " << mode << " not found!\n";
  return NULL;
}

RRCrtc MonitorReconfigure::FindUsableCrtc(const std::set<RRCrtc>& used_crtcs,
                                          const RRCrtc* crtcs,
                                          int ncrtc) {
  for (int i = 0; i < ncrtc; i++)
    if (used_crtcs.find(crtcs[i]) == used_crtcs.end())
      return crtcs[i];

  return None;
}

void MonitorReconfigure::SetScreenOn() {
  LOG(INFO) << "MonitorReconfigure::SetScreenOn()";
  Run(true);
}

void MonitorReconfigure::SetScreenOff() {
  LOG(INFO) << "MonitorReconfigure::SetScreenOff()";
  if (SetupXrandr()) {
    DisableAllOutputs();
    last_configuration_time_= Time::Now();
    ClearXrandr();
  }
}

void MonitorReconfigure::SetInternalPanelOn() {
  if (is_internal_panel_enabled_)
    return;

  LOG(INFO) << "MonitorReconfigure::SetInternalPanelOn()";

  if (!SetupXrandr())
    return;

  for (int i = 0; i < screen_info_->noutput; i++) {
    XRROutputInfo* output_info = XRRGetOutputInfo(display_,
                                                  screen_info_,
                                                  screen_info_->outputs[i]);
    if (IsInternalPanel(output_info)) {
      if (output_info->connection == RR_Connected) {
        // Restore the panel output.
        usable_outputs_.push_back(screen_info_->outputs[i]);
        usable_outputs_info_.push_back(output_info);

        RRCrtc crtc;
        RRMode mode;
        int x, y;

        // If we don't have a valid saved panel state, we restore the
        // panel to a default state.
        if (internal_panel_state_.crtc == 0) {
          LOG(INFO) << "Restore panel to a default state";
          if (output_info->ncrtc == 0 || output_info->nmode == 0) {
            LOG(WARNING) << "Panel does not have usable crtc/mode at all";
            break;
          }
          // Use the highest resolution.
          vector<ResolutionSelector::Mode> lcd_modes;
          SortModesByResolution(usable_outputs_[0], &lcd_modes);
          mode = lcd_modes[0].id;
          // Use the first usable crtc.
          crtc = output_info->crtcs[0];
          x = y = 0;
        } else {  // Use saved panel state.
          LOG(INFO) << "Restore panel to a saved state.";
          crtc = internal_panel_state_.crtc;
          mode = internal_panel_state_.mode;
          x =  internal_panel_state_.position_x;
          y =  internal_panel_state_.position_y;
        }

        usable_outputs_crtc_.push_back(crtc);
        EnableUsableOutput(0, mode, &x, &y);
        last_configuration_time_= Time::Now();
        CHECK(DPMSEnable(display_));
        CHECK(DPMSForceLevel(display_, DPMSModeOn));
        // So ClearUsableOutputsInfo() won't double free it.
        usable_outputs_info_.pop_back();
      }
    }
    XRRFreeOutputInfo(output_info);
  }
  ClearUsableOutputsInfo();
  ClearXrandr();
}

void MonitorReconfigure::SetInternalPanelOff() {
  LOG(INFO) << "MonitorReconfigure::SetInternalPanelOff()";
  if (!SetupXrandr())
    return;

  for (int i = 0; i < screen_info_->noutput; i++) {
    XRROutputInfo* output_info = XRRGetOutputInfo(display_,
                                                  screen_info_,
                                                  screen_info_->outputs[i]);
    if (IsInternalPanel(output_info) && output_info->crtc != None) {
      DisableOutputCrtc(output_info, output_info->crtc);
      is_internal_panel_enabled_ = false;
      last_configuration_time_= Time::Now();
    }
    XRRFreeOutputInfo(output_info);
  }
  ClearXrandr();
}

void MonitorReconfigure::DisableOutputCrtc(XRROutputInfo* output_info,
                                           RRCrtc crtc_xid) {
  int result = XRRSetCrtcConfig(display_,
                                screen_info_,
                                crtc_xid,
                                CurrentTime,
                                0, 0,  // x, y
                                None,  // mode
                                RR_Rotate_0,
                                NULL,  // outputs
                                0);    // noutputs

  if (result != RRSetConfigSuccess) {
    LOG(WARNING) << "Output " << output_info->name
                 << " failed to disable crtc " << crtc_xid;
  } else {
    LOG(INFO) << "Output " << output_info->name
              << " disabled crtc " << crtc_xid;
  }
}

void MonitorReconfigure::DisableOutput(XRROutputInfo* output_info) {
  for (int c = 0; c < output_info->ncrtc; c++) {
    RRCrtc crtc_xid = output_info->crtcs[c];
    DisableOutputCrtc(output_info, crtc_xid);
  }
  // Bookkeeping panel state.
  if (IsInternalPanel(output_info))
    is_internal_panel_enabled_ = false;
}

void MonitorReconfigure::DisableAllOutputs() {
  for (int i = 0; i < screen_info_->noutput; i++) {
    XRROutputInfo* output_info = XRRGetOutputInfo(display_,
                                                  screen_info_,
                                                  screen_info_->outputs[i]);
    DisableOutput(output_info);
    XRRFreeOutputInfo(output_info);
  }
}

void MonitorReconfigure::EnableUsableOutput(int idx,
                                            RRMode mode,
                                            int* position_x,
                                            int* position_y) {
    RROutput output = usable_outputs_[idx];
    XRROutputInfo* output_info = usable_outputs_info_[idx];
    XRRModeInfo* mode_info = GetModeInfo(mode);
    CHECK(NULL != mode_info);
    RRCrtc crtc_xid = usable_outputs_crtc_[idx];

    if (crtc_xid == None) {
      LOG(WARNING) << "No valid crtc for output " << output_info->name;
      return;
    }

    int x = position_x ? *position_x : 0;
    int y = position_y ? *position_y : 0;

    int result = XRRSetCrtcConfig(display_,
                                  screen_info_,
                                  crtc_xid,
                                  CurrentTime,
                                  x, y,  // x, y
                                  mode,
                                  RR_Rotate_0,
                                  &output,
                                  1);    // noutputs

    if (result != RRSetConfigSuccess) {
      LOG(WARNING) << "Output " << output_info->name
                   << " failed to enable resolution " << mode
                   << " " << mode_info->width << "x" << mode_info->height;
    } else {
      LOG(INFO) << "Output " << output_info->name << " enabled resolution "
                << mode_info->width << "x" << mode_info->height
                << "+" << x << "+" << y << " with crtc " << crtc_xid;

      // If panel output is enabled, we save its connection state.
      if (IsInternalPanel(output_info)) {
        internal_panel_state_ = ConnectionState(crtc_xid, mode, x, y);
        is_internal_panel_enabled_ = true;
      }

      if (position_x)
        *position_x += mode_info->width;
      if (position_y)
        *position_y += mode_info->height;
    }
}

bool MonitorReconfigure::HasInternalPanelConnection() {
  return internal_panel_connection_ == RR_Connected;
}

// TODO(miletus@) : Need to have a different way to check panel connection for
// Arrow since Arrow uses DP for panel connection.
void MonitorReconfigure::CheckInternalPanelConnection() {
  if (internal_panel_connection_ != RR_UnknownConnection)
    return;

  if (!SetupXrandr())
    return;

  internal_panel_connection_ = RR_Disconnected;
  for (int i = 0; i < screen_info_->noutput; i++) {
    XRROutputInfo* output_info = XRRGetOutputInfo(display_,
                                                  screen_info_,
                                                  screen_info_->outputs[i]);

    if (IsInternalPanel(output_info)) {
      if (output_info->connection == RR_Connected)
        internal_panel_connection_ = RR_Connected;
      XRRFreeOutputInfo(output_info);
      break;
    }
    XRRFreeOutputInfo(output_info);
  }

  // |is_internal_panel_enabled_| deafaults to be true, but if we don't have
  // panel connection at all, we mark it as not enabled.
  if (internal_panel_connection_ == RR_Disconnected)
    is_internal_panel_enabled_ = false;

  ClearXrandr();
}

bool MonitorReconfigure::AreOutputsSame(const vector<OutputInfo>& outputs1,
                                        const vector<OutputInfo>& outputs2) {
  if (outputs1.size() != outputs2.size())
    return false;

  for (size_t i = 0; i < outputs1.size(); i++) {
    const OutputInfo& o1 = outputs1[i];
    const OutputInfo& o2 = outputs2[i];

    if (o1.name != o2.name || o1.modes.size() != o2.modes.size())
      return false;

    for (size_t j = 0; j < o1.modes.size(); j++)
      if (o1.modes[j].width != o2.modes[j].width ||
          o1.modes[j].height != o2.modes[j].height ||
          o1.modes[j].dotClock != o2.modes[j].dotClock)
        return false;
  }
  return true;
}

bool MonitorReconfigure::NeedReconfigure(
    const vector<OutputInfo>& current_outputs) {
  bool need_reconfigure = true;
  int old_noutput = saved_outputs_.size();
  int noutput = current_outputs.size();

  if (backlight_ctl_ && backlight_ctl_->GetPowerState() == BACKLIGHT_IDLE_OFF) {
    LOG(INFO) << "System in IDLE_OFF state, skip the reconfigure";
    return false;
  }

  if (old_noutput == 0) {   // First time run monitor_reconfigure.
    LOG(INFO) << "First time MonitorReconfigure::Run()";
    struct sysinfo info;
    sysinfo(&info);

    // old_noutput == 0 happens when:
    // 1) System boots and powerd runs first time. In this case, if we only have
    // one output, we rely on that kernel/X has setup the screen for the output
    // and we don't reconfigure (to avoid blanking the welcome splash).  If we
    // have more than one output, then we do the re-configuration.
    // 2) Powerd restarts,e.g. Guest session exits. We reconfigure in this case.
    // System uptime is used to tell whether this is the first time powerd runs
    // with a boot or a restart.
    if (info.uptime >= kSystemUptimeThreshold) {  // Powerd restarts.
      LOG(INFO) << "Monitor reconfigure runs at system uptime "
                << info.uptime << ", possibly powerd restarts ? ";
    } else {  // First time powerd starts.
      if (noutput == 1) {
        LOG(INFO) << "Only have one output, skip reconfigure";
        need_reconfigure = false;
      }
    }
  } else {  // Not the first run.
    if (!AreOutputsSame(saved_outputs_, current_outputs)) {
      LOG(INFO) << "Outputs changed, need reconfigure.";
    } else {
      LOG(INFO) << "Outputs did not change, skip reconfigure.";
      need_reconfigure = false;
    }
  }

  return need_reconfigure;
}

void MonitorReconfigure::FallBackToSingleHead() {
  // Keep only one usable output.
  usable_outputs_.pop_back();
  usable_outputs_info_.pop_back();
  usable_outputs_crtc_.pop_back();

  vector<ResolutionSelector::Mode> lcd_modes;
  ResolutionSelector::Mode lcd_resolution;
  ResolutionSelector::Mode external_resolution;

  // Use the highest resolution from the first output.
  SortModesByResolution(usable_outputs_[0], &lcd_modes);
  lcd_resolution = lcd_modes[0];

  RunClone(lcd_resolution, external_resolution, lcd_resolution);
}


bool MonitorReconfigure::CheckValidResolution(int width, int height) {
  int minWidth, minHeight, maxWidth, maxHeight;
  bool is_resolution_valid = true;
  // TODO(miletus@): To test against max GL viewport size.
  if (XRRGetScreenSizeRange(display_, window_, &minWidth, &minHeight,
                            &maxWidth, &maxHeight) == 1) {
    LOG(INFO) << "Max supported screen resolution "
              << maxWidth << "x" << maxHeight;
    if (width < minWidth || width > maxWidth ||
        height < minHeight || height > maxHeight) {
      LOG(WARNING) << "Screen resolution " << width << "x" << height
                   << " exceeds max supported resolution";
      is_resolution_valid = false;
    }
  } else {
    LOG(WARNING) << "Can't get max supported screen resolution";
    is_resolution_valid = false;
  }

  return is_resolution_valid;
}


void MonitorReconfigure::RunExtended() {
  unsigned resolution_width = 0, resolution_height = 0;
  // Grab the server during mode changes.
  XGrabServer(display_);
  DisableAllOutputs();
  // First pass to compute the total screen size
  for (unsigned int o = 0; o < usable_outputs_.size(); o++) {
    XRROutputInfo* output_info = usable_outputs_info_[o];
    if (output_info->nmode == 0)
      break;

    RRMode mode = output_info->modes[0];
    XRRModeInfo* mode_info = GetModeInfo(mode);
    // We pulled this mode from our existing usable_outputs_ structure so we
    // better find it.
    CHECK(NULL != mode_info);

    LOG(INFO) << "Output "
              << output_info->name
              << "'s native mode is "
              << mode_info->width << "x" << mode_info->height;
    resolution_width = max(resolution_width, mode_info->width);
    resolution_height += mode_info->height;
  }

  LOG(INFO) << "Total screen resolution "
            << resolution_width << "x" << resolution_height;

  if (!CheckValidResolution(resolution_width, resolution_height)) {
      LOG(WARNING) << "The extended screen resolution is not valid, "
                   << "fall back to single head mode";
      XUngrabServer(display_);
      return FallBackToSingleHead();
  }

  // For the screen dimensions, compute a fake size that corresponds to 96dpi.
  // We do this for two reasons:
  // - Screen size reporting isn't always accurate,
  // - ChromeOS chrome doesn't work well at non-standard DPIs.
  XRRSetScreenSize(display_, window_, resolution_width, resolution_height,
                   resolution_width * kInchInMm / kScreenDpi,
                   resolution_height * kInchInMm / kScreenDpi);

  // Second pass to set the modes.
  int position_y = 0;
  int num_used_outputs = usable_outputs_.size();

  if (dual_head_mode_ == kModeFirstMonitorPrimary) {
    LOG(INFO) << "Setting dual head mode : FIRST MONITOR PRIMARY";
    // If the first display is set as primary display, then scan out the first
    // display from (0,0). And second display from (0, height_of_first_display).
    for (int o = 0; o < num_used_outputs; o++)
      EnableUsableOutput(o, usable_outputs_info_[o]->modes[0],
                         NULL, &position_y);
  } else {
    // If the second display is set as primary display, then scan out the second
    // display from (0,0). And first display from (0, height_of_second_display).
    LOG(INFO) << "Setting dual head mode : SECOND MONITOR PRIMARY";
    for (int o = num_used_outputs - 1; o >= 0; o--)
      EnableUsableOutput(o, usable_outputs_info_[o]->modes[0],
                         NULL, &position_y);
  }

  // To safeguard the case where DPMS is not properly tracked after
  // re-enabling outputs, always turn on the DPMS after usable outputs
  // are enabled.
  CHECK(DPMSEnable(display_));
  CHECK(DPMSForceLevel(display_, DPMSModeOn));

  // If we have an panel output and another output is in use, we are projecting.
  is_projecting_ = (internal_panel_connection_ == RR_Connected) &&
      num_used_outputs > 1;

  // Now let the server go and sync all changes.
  XUngrabServer(display_);
  XSync(display_, False);
}

void MonitorReconfigure::RunClone(
    const ResolutionSelector::Mode& lcd_resolution,
    const ResolutionSelector::Mode& external_resolution,
    const ResolutionSelector::Mode& screen_resolution) {

  LOG(INFO) << "RunClone() with " << usable_outputs_.size()
            << " usable outputs";

  // Grab the server during mode changes.
  XGrabServer(display_);

  // Disable all the outputs and then later on enable those we want.
  // Disconnected outputs need to be disabled because when output
  // connection is removed, i915 driver does not remove the associated
  // crtc from the output unless we tell it to do it through Xrandr.
  // Connected outputs are disabled in case it changes associated crtc
  // when later on usable outputs are enabled.
  DisableAllOutputs();

  const bool was_projecting = is_projecting_;

  is_projecting_ = !external_resolution.name.empty();

  // Set the fb resolution if we have at least one usable output.
  // Otherwise we still go through the following code to cleanup.
  SetScreenResolution(screen_resolution);

  if (usable_outputs_.size() >= 1)
    EnableUsableOutput(0, lcd_resolution.id, NULL, NULL);

  if (usable_outputs_.size() >= 2)
    EnableUsableOutput(1, external_resolution.id, NULL, NULL);

  // To safeguard the case where DPMS is not properly tracked after
  // re-enabling outputs, always turn on the DPMS after usable outputs
  // are enabled.
  CHECK(DPMSEnable(display_));
  CHECK(DPMSForceLevel(display_, DPMSModeOn));

  // Now let the server go and sync all changes.
  XUngrabServer(display_);
  XSync(display_, False);

  if (was_projecting != is_projecting_ && projection_callback_ != NULL)
    projection_callback_(projection_callback_data_);
}

bool MonitorReconfigure::NeedSwitchMode() {
  if (!SetupXrandr())
    return false;

  bool ret = true;

  std::vector<OutputInfo> current_outputs;
  DetermineOutputs(&current_outputs);

  // Don't switch mode if we only have internal panel as output.
  if (internal_panel_connection_ == RR_Connected &&
      current_outputs.size() == 1)
    ret = false;

  ClearUsableOutputsInfo();
  ClearXrandr();

  return ret;
}

void  MonitorReconfigure::SwitchMode() {
  LOG(INFO) << "Ctrl+F4 pressed to switch mode";

  if (!NeedSwitchMode()) {
    LOG(INFO) << "Mode switch is not needed";
    return;
  }

  need_switch_mode_ = true;
  Run(true);
  need_switch_mode_ = false;
}

void MonitorReconfigure::Run(bool force_reconfigure) {
  ClearUsableOutputsInfo();

  if (!SetupXrandr())
    return;

  std::vector<OutputInfo> current_outputs;
  DetermineOutputs(&current_outputs);

  if (usable_outputs_.size() == 0 ||
      (!force_reconfigure && !NeedReconfigure(current_outputs))) {
    saved_outputs_.swap(current_outputs);
    ClearUsableOutputsInfo();
    ClearXrandr();
    return;
  }

  last_configuration_time_= Time::Now();

  vector<ResolutionSelector::Mode> lcd_modes;
    SortModesByResolution(usable_outputs_[0], &lcd_modes);

  vector<ResolutionSelector::Mode> external_modes;
  if (usable_outputs_.size() >= 2)
    SortModesByResolution(usable_outputs_[1], &external_modes);

  ResolutionSelector selector;
  ResolutionSelector::Mode lcd_resolution;
  ResolutionSelector::Mode external_resolution;
  ResolutionSelector::Mode screen_resolution;

  if (usable_outputs_.size() == 1) {
    // If there is only one output, find the best resolution of the output
    // and RunClone().
    CHECK(selector.FindBestResolutions(lcd_modes,
                                       external_modes,
                                       &lcd_resolution,
                                       &external_resolution,
                                       &screen_resolution));
    RunClone(lcd_resolution, external_resolution, screen_resolution);
  } else {
    // If Ctrl+F4 triggered monitor reconfigure, we move to the next
    // dual head mode.
    if (need_switch_mode_)
      dual_head_mode_ = static_cast<DualHeadMode>(
          (dual_head_mode_ + 1) % kModeLast);

    // Dual head mode.
    if (dual_head_mode_ == kModeFirstMonitorPrimary ||
        dual_head_mode_ == kModeSecondMonitorPrimary) {
      RunExtended();
    } else {
      // If Clone mode is needed, check that if common resolution exists
      // between the 2 displays. If not, move to the next dual head mode.
      if (selector.FindCommonResolutions(lcd_modes,
                                         external_modes,
                                         &lcd_resolution,
                                         &external_resolution,
                                         &screen_resolution)) {
        RunClone(lcd_resolution, external_resolution, screen_resolution);
      } else {
        dual_head_mode_ = kModeFirstMonitorPrimary;
        RunExtended();
      }
    }
  }

  saved_outputs_.swap(current_outputs);
  ClearUsableOutputsInfo();
  ClearXrandr();
}

void MonitorReconfigure::SetProjectionCallback(void (*func)(void*),
                                               void* data) {
  projection_callback_ = func;
  projection_callback_data_ = data;
}

void MonitorReconfigure::DetermineOutputs(vector<OutputInfo>* current_outputs) {
  current_outputs->clear();
  std::set<RRCrtc> used_crtcs;
  for (int i = 0; i < screen_info_->noutput; i++) {
    XRROutputInfo* output_info = XRRGetOutputInfo(display_,
                                                  screen_info_,
                                                  screen_info_->outputs[i]);

    if (output_info->connection == RR_Connected) {
      // Add this output to the list of usable outputs...
      usable_outputs_.push_back(screen_info_->outputs[i]);
      usable_outputs_info_.push_back(output_info);
      RRCrtc crtc_xid = FindUsableCrtc(used_crtcs,
                                       output_info->crtcs,
                                       output_info->ncrtc);
      CHECK(crtc_xid != None);
      usable_outputs_crtc_.push_back(crtc_xid);
      used_crtcs.insert(crtc_xid);

      OutputInfo info;
      info.name = output_info->name;
      for (int i = 0; i < output_info->nmode; i++) {
        ModeInfo mode;
        XRRModeInfo* xmode = GetModeInfo(output_info->modes[i]);
        // We pulled this mode from our existing usable_outputs_ structure so we
        // better find it.
        CHECK(NULL != xmode);
        mode.width = xmode->width;
        mode.height = xmode->height;
        mode.dotClock = xmode->dotClock;
        info.modes.push_back(mode);
      }
      sort(info.modes.begin(), info.modes.end(), ModeInfoComparator());
      current_outputs->push_back(info);

      // Stop at 2 connected outputs, the resolution logic can't handle more
      // than that.
      if (usable_outputs_.size() >= 2)
        break;
    } else {
      XRRFreeOutputInfo(output_info);
    }
  }

  if (usable_outputs_.empty())
    return;

  // If the second output is LVDS, put it in first place.
  if (usable_outputs_.size() == 2 &&
      IsInternalPanel(usable_outputs_info_[1])) {
    std::swap(usable_outputs_[0], usable_outputs_[1]);
    std::swap(usable_outputs_info_[0], usable_outputs_info_[1]);
    std::swap(usable_outputs_crtc_[0], usable_outputs_crtc_[1]);
  }
}

void MonitorReconfigure::SortModesByResolution(
    RROutput output,
    vector<ResolutionSelector::Mode>* modes_out) {
  modes_out->clear();

  XRROutputInfo* output_info = XRRGetOutputInfo(display_, screen_info_, output);
  for (int i = 0; i < output_info->nmode; ++i) {
    const XRRModeInfo* mode = mode_map_[output_info->modes[i]];
    DCHECK(mode);
    LOG(INFO) << "(" << output_info->name << ")"
              << " Adding mode " << mode->width << "x" << mode->height
              << " (xid " << mode->id << ")";
    modes_out->push_back(
        ResolutionSelector::Mode(mode->width, mode->height, mode->name,
                                 mode->id, i < output_info->npreferred));
  }

  sort(modes_out->begin(), modes_out->end(),
       ResolutionSelector::ModeResolutionComparator());

  XRRFreeOutputInfo(output_info);
}

bool MonitorReconfigure::SetScreenResolution(
    const ResolutionSelector::Mode& resolution) {
  // Do not switch resolutions if we don't need to, this avoids blinking.
  int screen = DefaultScreen(display_);
  if ( (resolution.width != DisplayWidth(display_, screen))||
     (resolution.height != DisplayHeight(display_, screen)) ) {
    LOG(INFO) << "Setting resolution "
              << resolution.width << "x" << resolution.height;
    XRRSetScreenSize(display_, window_,
                     resolution.width,
                     resolution.height,
                     resolution.width * kInchInMm / kScreenDpi,
                     resolution.height * kInchInMm / kScreenDpi);
  } else {
    LOG(INFO) << "Leaving resolution "
              << resolution.width << "x" << resolution.height << " alone";
  }
  return true;
}

GdkFilterReturn MonitorReconfigure::GdkEventFilter(GdkXEvent* gxevent,
                                                   GdkEvent*) {
  XEvent* xevent = static_cast<XEvent*>(gxevent);
  XRRNotifyEvent *aevent = reinterpret_cast<XRRNotifyEvent*>(xevent);

  if (xevent->type == rr_event_base_ + RRScreenChangeNotify ||
      xevent->type == rr_event_base_ + RRNotify) {
    switch (xevent->type - rr_event_base_) {
      case RRScreenChangeNotify:
        LOG(INFO) << "RRScreenChangeNotify event received.";
        XRRUpdateConfiguration(xevent);
        break;
      case RRNotify:
        LOG(INFO) << RRNotifySubtypeToString(aevent->subtype)
                  << " event received.";
        break;
      default:
        LOG(INFO) << "Unknown GDK event received.";
    }
    Run(false);
    // Remove this event so that no other program acts upon it.
    return GDK_FILTER_REMOVE;
  }
  return GDK_FILTER_CONTINUE;
}

}  // namespace power_manager
