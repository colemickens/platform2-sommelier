// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/monitor_reconfigure.h"

#include <gdk/gdkx.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>

#include "base/logging.h"
#include "power_manager/backlight_controller.h"

using std::sort;
using std::vector;

namespace {

// Name of the internal output.
const char kLcdOutputName[] = "LVDS";
// The screen DPI we pass to X11.
const float kScreenDpi = 96.0;
// An inch in mm.
const float kInchInMm = 25.4;

}  // namespace

namespace power_manager {

MonitorReconfigure::MonitorReconfigure()
    : display_(NULL),
      window_(None),
      is_projecting_(false),
      projection_callback_(NULL),
      projection_callback_data_(NULL),
      backlight_ctl_(NULL) {
}

MonitorReconfigure::MonitorReconfigure(
    BacklightController* backlight_ctl)
    : display_(NULL),
      window_(None),
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
                 RRScreenChangeNotifyMask);

  gdk_window_add_filter(NULL, GdkEventFilterThunk, this);
  LOG(INFO) << "XRandr event filter added";

  // Call Run() on startup to setup screens and initialize display info.
  Run();

  return true;
}

XRRModeInfo* MonitorReconfigure::GetModeInfo(RRMode mode) {
  for (int i = 0; i < screen_info_->nmode; i++)
    if (screen_info_->modes[i].id == mode)
      return &screen_info_->modes[i];

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

void MonitorReconfigure::DisableDisconnectedOutputs() {
  for (int i = 0; i < screen_info_->noutput; i++) {
    XRROutputInfo* output_info = XRRGetOutputInfo(display_,
                                                  screen_info_,
                                                  screen_info_->outputs[i]);

    if ((output_info->connection != RR_Connected) &&
        (output_info->crtc != None)) {
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
                       << " disconnected, failed to disable crtc "
                       << crtc_xid;
        } else {
          LOG(INFO) << "Output " << output_info->name
                    << " disconnected, disabled crtc "
                    << crtc_xid;
        }
      }
    } else {
        LOG(INFO) << "Output " << output_info->name << " left alone";
    }

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

void MonitorReconfigure::Run() {
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

  for (int i = 0; i < screen_info_->nmode; ++i) {
    XRRModeInfo* current_mode = &screen_info_->modes[i];
    mode_map_[current_mode->id] = current_mode;
  }

  DetermineOutputs();

  vector<ResolutionSelector::Mode> lcd_modes;
  if (usable_outputs_.size() >= 1)
    SortModesByResolution(usable_outputs_[0], &lcd_modes);
  else
    lcd_modes.clear();

  vector<ResolutionSelector::Mode> external_modes;
  if (usable_outputs_.size() >= 2)
    SortModesByResolution(usable_outputs_[1], &external_modes);
  else
    external_modes.clear();

  ResolutionSelector selector;
  ResolutionSelector::Mode lcd_resolution;
  ResolutionSelector::Mode external_resolution;
  ResolutionSelector::Mode screen_resolution;

  CHECK(selector.FindBestResolutions(lcd_modes,
                                     external_modes,
                                     &lcd_resolution,
                                     &external_resolution,
                                     &screen_resolution));

  // Disable the backlight before resizing anything on the screen.
  if (lcd_resolution.name.empty() || is_projecting_)
    backlight_ctl_->SetPowerState(BACKLIGHT_IDLE_OFF);

  // Grab the server during mode changes.
  XGrabServer(display_);

  // Disable all disconnected outputs before we try to set the screen
  // resolution; otherwise xrandr will complain if we're trying to set the
  // screen to a smaller size than what the now-unplugged device was using.
  DisableDisconnectedOutputs();

  const bool was_projecting = is_projecting_;

  is_projecting_ = !external_resolution.name.empty();

  // Set the fb resolution if we have at least one usable output.
  // Otherwise we still go through the following code to cleanup.
  if (usable_outputs_.size() >= 1)
    SetScreenResolution(screen_resolution);

  std::vector<RRMode> resolutions;
  resolutions.push_back(lcd_resolution.id);
  resolutions.push_back(external_resolution.id);

  EnableUsableOutputs(resolutions);

  // Now let the server go and sync all changes.
  XUngrabServer(display_);
  XSync(display_, False);

  if (was_projecting != is_projecting_ && projection_callback_ != NULL)
    projection_callback_(projection_callback_data_);

  // Enable the backlight. We do not want to do this before the resize is
  // done so that we can hide the resize when going off->on.
  if (!lcd_resolution.name.empty() || is_projecting_)
    backlight_ctl_->SetPowerState(BACKLIGHT_ACTIVE);

  usable_outputs_.clear();
  for (unsigned i = 0; i < usable_outputs_info_.size(); i++)
    if (usable_outputs_info_[i])
      XRRFreeOutputInfo(usable_outputs_info_[i]);
  usable_outputs_info_.clear();
  XRRFreeScreenResources(screen_info_);

  XCloseDisplay(display_);
  return;
}

void MonitorReconfigure::SetProjectionCallback(void (*func)(void*),
                                               void* data) {
  projection_callback_ = func;
  projection_callback_data_ = data;
}

void MonitorReconfigure::DetermineOutputs() {
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
      // ... and also add the crtc we intend to use for it.
      used_crtcs.insert(crtc_xid);
      LOG(INFO) << "Added output " << output_info->name
                << " with crtc " << crtc_xid;
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
                                 mode->id));
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
  if (xevent->type == rr_event_base_ + RRScreenChangeNotify) {
    Run();
    // Remove this event so that no other program acts upon it.
    return GDK_FILTER_REMOVE;
  }
  return GDK_FILTER_CONTINUE;
}

}  // namespace power_manager
