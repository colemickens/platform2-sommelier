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
#include "power_manager/backlight_controller.h"

using std::max;
using std::sort;
using std::vector;
using base::Time;
using base::TimeDelta;

namespace {

// Name of the internal output.
const char kLcdOutputName[] = "LVDS";
// The screen DPI we pass to X11.
const float kScreenDpi = 96.0;
// An inch in mm.
const float kInchInMm = 25.4;
// Duplicate RRScreenChangeNotify events can be received when the
// actual number of outputs does not change. Use this interval threshold
// to filter out duplicate monitor reconfigure.
const TimeDelta kReconfigureInterval = TimeDelta::FromSeconds(3);
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

}  // namespace

namespace power_manager {

MonitorReconfigure::MonitorReconfigure()
    : display_(NULL),
      window_(None),
      noutput_(-1),
      last_configuration_time_(Time::Now()),
      lvds_connection_(RR_UnknownConnection),
      is_projecting_(false),
      projection_callback_(NULL),
      projection_callback_data_(NULL),
      backlight_ctl_(NULL) {
}

MonitorReconfigure::MonitorReconfigure(
    BacklightController* backlight_ctl)
    : display_(NULL),
      window_(None),
      noutput_(-1),
      last_configuration_time_(Time::Now()),
      lvds_connection_(RR_UnknownConnection),
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
    backlight_ctl_->set_monitor_reconfigure(this);

  // Call Run() on startup to setup screens and initialize display info.
  Run(false);

  return true;
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
  DisableAllOutputs();
  last_configuration_time_= Time::Now();
}

void MonitorReconfigure::DisableOutput(XRROutputInfo* output_info) {
  for (int c = 0; c < output_info->ncrtc; c++) {
    RRCrtc crtc_xid = output_info->crtcs[c];
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

void MonitorReconfigure::EnableUsableOutputs(
    const std::vector<RRMode>& resolutions) {
  std::set<RRCrtc> used_crtcs;

  for (unsigned i = 0; i < usable_outputs_.size(); i++) {
    XRROutputInfo* output_info = usable_outputs_info_[i];
    RRMode resolution = resolutions[i];
    XRRModeInfo* mode_info = GetModeInfo(resolution);

    if (!mode_info) {
      LOG(WARNING) << "No mode " << resolution << " found!";
      continue;
    }

    RRCrtc crtc_xid = FindUsableCrtc(used_crtcs,
                                     output_info->crtcs,
                                     output_info->ncrtc);

    CHECK(crtc_xid != None);

    int result = XRRSetCrtcConfig(display_,
                                  screen_info_,
                                  crtc_xid,
                                  CurrentTime,
                                  0, 0,  // x, y
                                  resolution,
                                  RR_Rotate_0,
                                  &usable_outputs_[i],
                                  1);    // noutputs

    if (result != RRSetConfigSuccess) {
      LOG(WARNING) << "Output " << output_info->name
                   << " failed to enable resolution " << resolution
                   << " " << mode_info->width << "x" << mode_info->height;
    } else {
      LOG(INFO) << "Output " << output_info->name
              << " enabled resolution " << resolution
              << " " << mode_info->width << "x" << mode_info->height
              <<  " with crtc " << crtc_xid;
      // Success, this crtc is now used.
      used_crtcs.insert(crtc_xid);
    }
  }
}

int MonitorReconfigure::GetConnectedOutputsNum() {
  int count = 0;
  for (int i = 0; i < screen_info_->noutput; i++) {
    XRROutputInfo* output_info = XRRGetOutputInfo(display_,
                                                  screen_info_,
                                                  screen_info_->outputs[i]);

    if (output_info->connection == RR_Connected)
      count++;
  }

  return count;
}

void MonitorReconfigure::RecordLVDSConnection() {
  lvds_connection_ = RR_Disconnected;
  for (int i = 0; i < screen_info_->noutput; i++) {
    XRROutputInfo* output_info = XRRGetOutputInfo(display_,
                                                  screen_info_,
                                                  screen_info_->outputs[i]);
    if (strncmp(output_info->name,
                kLcdOutputName,
                strlen(kLcdOutputName)) == 0) {
      if (output_info->connection == RR_Connected)
        lvds_connection_ = RR_Connected;
      break;
    }
  }
}

bool MonitorReconfigure::NeedReconfigure() {
  bool need_reconfigure = true;
  int old_noutput = noutput_;
  noutput_ = GetConnectedOutputsNum();

  if (backlight_ctl_ && backlight_ctl_->state() == BACKLIGHT_IDLE_OFF) {
    LOG(INFO) << "System in IDLE_OFF state, skip the reconfigure";
    return false;
  }

  if (old_noutput == -1) {   // First time run monitor_reconfigure.
    LOG(INFO) << "First time MonitorReconfigure::Run()";
    struct sysinfo info;
    sysinfo(&info);

    // old_noutput == -1 happens when:
    // 1) System boots and powerd runs first time. In this case, if we only have
    // one output, we rely on that kernel/X has setup the screen for the output
    // and we don't reconfigure (to avoid blanking the welcome splash).  If we
    // have more than one outputs, then we do the re-configuration.
    // 2) Powerd restarts,e.g. Guest session exits. We reconfigure in this case.
    // System uptime is used to tell whether this is the first time powerd runs
    // with a boot or a restart.
    if (info.uptime >= kSystemUptimeThreshold) {  // Powerd restarts.
      LOG(INFO) << "Monitor reconfigure runs at system uptime "
                << info.uptime << ", possibly powerd restarts ? ";
    } else {  // First time powerd starts.
      if (noutput_ == 1) {
        LOG(INFO) << "Only have one output, skip reconfigure";
        need_reconfigure = false;
      }
    }
  } else {  // Not the first run.
    if (noutput_ != old_noutput) {      // Monitor added/removed. Reconfigure.
      LOG(INFO) << "Number of outputs changed from " << old_noutput << " to "
                << noutput_ << ", need reconfigure monitors";
    } else { // Number of outputs does not change.
      // Duplicate RR*NotifyEvent are fired within short time period, just
      // ignore them.
      if (Time::Now() - last_configuration_time_ < kReconfigureInterval) {
        LOG(INFO) << "Last time monitor rconfigured within "
                  << kReconfigureInterval.InSeconds()
                  << " secs, skip reconfiguration";
        need_reconfigure = false;
      } else {
        LOG(INFO) << "Output number does not change, Waking up from suspend?";
        // This is the case of waking from suspend and outputs do not change.
        // If we only have LVDS connection, skip the reconfigure and just resume
        // to the state of before suspend.
        if (noutput_ == 1 && lvds_connection_ == RR_Connected) {
          LOG(INFO) << "Only have LVDS connection, skip reconfigure";
          need_reconfigure = false;
        } else {
          LOG(INFO) << "Have " << noutput_ << " outputs, need reconfigure";
        }
      }
    }
  }
  return need_reconfigure;
}

void MonitorReconfigure::RunExtended() {
  unsigned resolution_width = 0, resolution_height = 0;
  // Grab the server during mode changes.
  XGrabServer(display_);

  DisableAllOutputs();

  // First pass to compute the total screen size
  for (int o = 0; o < screen_info_->noutput; o++) {
    XRROutputInfo* output_info = XRRGetOutputInfo(display_,
                                                  screen_info_,
                                                  screen_info_->outputs[o]);
    switch (output_info->connection) {
      case RR_Connected:
      case RR_UnknownConnection: {
        LOG(INFO) << "Output " << output_info->name << " is connected";
        // If we have no modes for this output, drop it.
        if (output_info->nmode == 0)
           break;

        RRMode mode = output_info->modes[0];
        XRRModeInfo* mode_info = GetModeInfo(mode);

        if (!mode_info) {
          LOG(WARNING) << "Mode info not found";
        } else {
          LOG(INFO) << "Output "
                    << output_info->name
                    << "'s native mode is "
                    << mode_info->width << "x" << mode_info->height;
                    resolution_width = max(resolution_width, mode_info->width);
                    resolution_height += mode_info->height;
        }
        break;
      }
      case RR_Disconnected: {
        LOG(INFO) << "Output " << output_info->name << " is not connected";
        break;
      }
    }
  }

  LOG(INFO) << "Total screen resolution "
            << resolution_width << "x" << resolution_height;

  // For the screen dimensions, compute a fake size that corresponds to 96dpi.
  // We do this for two reasons:
  // - Screen size reporting isn't always accurate,
  // - ChromeOS chrome doesn't work well at non-standard DPIs.
  XRRSetScreenSize(display_, window_, resolution_width, resolution_height,
                   resolution_width * kInchInMm / kScreenDpi,
                   resolution_height * kInchInMm / kScreenDpi);

  std::set<RRCrtc> used_crtcs;

  // Second pass to set the modes.
  int position_y = 0;
  int num_used_outputs = 0;

  for (int o = 0; o < screen_info_->noutput; o++) {
    RROutput output = screen_info_->outputs[o];
    XRROutputInfo* output_info = XRRGetOutputInfo(display_,
                                                  screen_info_,
                                                  output);

    switch (output_info->connection) {
      case RR_Connected:
      case RR_UnknownConnection: {
        // Connected CRTC, enable it and use its native resolution.

        // If we have no modes for this output, drop it.
        if (output_info->nmode == 0) {
          LOG(WARNING) << "No mode for output " << output_info->name;
          break;
        }

        RRMode mode = output_info->modes[0];
        XRRModeInfo* mode_info = GetModeInfo(mode);
        RRCrtc crtc_xid = FindUsableCrtc(used_crtcs,
                                         output_info->crtcs,
                                         output_info->ncrtc);

        CHECK(crtc_xid != None);

        if (!mode_info)
          break;

        int r = XRRSetCrtcConfig(display_,
                                 screen_info_,
                                 crtc_xid,
                                 CurrentTime,
                                 0, position_y,
                                 mode,
                                 RR_Rotate_0,
                                 &output,
                                 1);
        if (r != RRSetConfigSuccess) {
          LOG(WARNING) << "Failed to enable output " << output_info->name;
        } else {
          LOG(INFO) << "Enabled output " << output_info->name
                    << " at " << mode_info->width << "x" << mode_info->height
                    << "+0+" << position_y << " with crtc " << crtc_xid;
          position_y += mode_info->height;
          used_crtcs.insert(crtc_xid);
          num_used_outputs++;
        }
        break;
      }
    }
  }

  // To safeguard the case where DPMS is not properly tracked after
  // re-enabling outputs, always turn on the DPMS after usable outputs
  // are enabled.
  CHECK(DPMSEnable(display_));
  CHECK(DPMSForceLevel(display_, DPMSModeOn));

  // If we have an LVDS output, and another output is in use, we are projecting.
  // TODO(marcheu): handle the case where the panel isn't LDVS but DP; we have
  // to find a way to tell a laptop's DP from a desktop's DP. This will be an
  // issue on Arrow.
  is_projecting_ = (lvds_connection_ == RR_Connected) && num_used_outputs > 1;

  // Now let the server go and sync all changes.
  XUngrabServer(display_);
  XSync(display_, False);
}

void MonitorReconfigure::RunClone() {
  for (int i = 0; i < screen_info_->nmode; ++i) {
    XRRModeInfo* current_mode = &screen_info_->modes[i];
    mode_map_[current_mode->id] = current_mode;
  }

  DetermineOutputs();

  vector<ResolutionSelector::Mode> lcd_modes;
  if (lvds_connection_ == RR_Connected) {
    CHECK(usable_outputs_.size() >= 1);
    SortModesByResolution(usable_outputs_[0], &lcd_modes);
  } else {
    LOG(INFO) << "NO LCD modes.";
    lcd_modes.clear();
  }

  vector<ResolutionSelector::Mode> external_modes;
  if (lvds_connection_ == RR_Connected && usable_outputs_.size() >= 2) {
    LOG(INFO) << "Have both LCD and external modes.";
    SortModesByResolution(usable_outputs_[1], &external_modes);
  } else if (lvds_connection_ == RR_Disconnected &&
             usable_outputs_.size() >= 1) {
    LOG(INFO) << "Have only external modes.";
    SortModesByResolution(usable_outputs_[0], &external_modes);
  } else {
    LOG(INFO) << "NO external modes.";
    external_modes.clear();
  }

  ResolutionSelector selector;
  ResolutionSelector::Mode lcd_resolution;
  ResolutionSelector::Mode external_resolution;
  ResolutionSelector::Mode screen_resolution;

  CHECK(selector.FindBestResolutions(lcd_modes,
                                     external_modes,
                                     &lcd_resolution,
                                     &external_resolution,
                                     &screen_resolution));

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
  if (usable_outputs_.size() >= 1)
    SetScreenResolution(screen_resolution);

  std::vector<RRMode> resolutions;
  if (lvds_connection_ == RR_Connected)
    resolutions.push_back(lcd_resolution.id);
  resolutions.push_back(external_resolution.id);

  EnableUsableOutputs(resolutions);

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

  usable_outputs_.clear();
  for (unsigned i = 0; i < usable_outputs_info_.size(); i++)
    if (usable_outputs_info_[i])
      XRRFreeOutputInfo(usable_outputs_info_[i]);
  usable_outputs_info_.clear();
  XRRFreeScreenResources(screen_info_);

  XCloseDisplay(display_);
  return;

}


void MonitorReconfigure::Run(bool force_reconfigure) {
  usable_outputs_.clear();
  usable_outputs_info_.clear();
  display_ = XOpenDisplay(NULL);

  // Give up if we can't open the display.
  if (!display_) {
    LOG(WARNING) << "Could not open display";
    return;
  }

  window_ = RootWindow(display_, DefaultScreen(display_));
  screen_info_ = XRRGetScreenResources(display_, window_);

  // Give up if we can't obtain the XRandr information.
  if (!screen_info_) {
    LOG(WARNING) << "Could not get XRandr information";
    XCloseDisplay(display_);
    return;
  }

  if (lvds_connection_ == RR_UnknownConnection)
    RecordLVDSConnection();

  if (!force_reconfigure && !NeedReconfigure())
    return;

  last_configuration_time_= Time::Now();

#ifdef EXTENDED_DESKTOP
  RunExtended();
#else
  RunClone();
#endif

}

void MonitorReconfigure::SetProjectionCallback(void (*func)(void*),
                                               void* data) {
  projection_callback_ = func;
  projection_callback_data_ = data;
}

void MonitorReconfigure::DetermineOutputs() {
  for (int i = 0; i < screen_info_->noutput; i++) {
    XRROutputInfo* output_info = XRRGetOutputInfo(display_,
                                                  screen_info_,
                                                  screen_info_->outputs[i]);

    if (output_info->connection == RR_Connected) {
      // Add this output to the list of usable outputs...
      usable_outputs_.push_back(screen_info_->outputs[i]);
      usable_outputs_info_.push_back(output_info);
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
      strncmp(usable_outputs_info_[1]->name,
              kLcdOutputName,
              strlen(kLcdOutputName)) == 0) {
    std::swap(usable_outputs_[0], usable_outputs_[1]);
    std::swap(usable_outputs_info_[0], usable_outputs_info_[1]);
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
