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
      lcd_output_(0),
      lcd_output_info_(NULL),
      external_output_(0),
      external_output_info_(NULL),
      is_projecting_(false),
      projection_callback_(NULL),
      projection_callback_data_(NULL),
      backlight_ctl_(NULL) {}

MonitorReconfigure::MonitorReconfigure(
    BacklightController* backlight_ctl)
    : display_(NULL),
      window_(None),
      lcd_output_(0),
      lcd_output_info_(NULL),
      external_output_(0),
      external_output_info_(NULL),
      is_projecting_(false),
      projection_callback_(NULL),
      projection_callback_data_(NULL),
      backlight_ctl_(backlight_ctl) {}

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

void MonitorReconfigure::Run() {
  lcd_output_ = 0;
  lcd_output_info_ = NULL;
  external_output_ = 0;
  external_output_info_ = NULL;
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

  // Give up if we can't find any outputs.
  if (!DetermineOutputs()) {
    LOG(WARNING) << "Unable to determine outputs";
    XRRFreeScreenResources(screen_info_);
    return;
  }

  vector<ResolutionSelector::Mode> lcd_modes;
  SortModesByResolution(lcd_output_, &lcd_modes);
  if (lcd_modes.empty()) {
    LOG(WARNING) << "LCD modes empty";
    XRRFreeScreenResources(screen_info_);
    return;
  }

  vector<ResolutionSelector::Mode> external_modes;
  if (external_output_info_ &&
      external_output_info_->connection == RR_Connected)
    SortModesByResolution(external_output_, &external_modes);
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
  if (lcd_resolution.name.empty())
    backlight_ctl_->SetPowerState(BACKLIGHT_IDLE_OFF);

  // Grab the server during mode changes.
  XGrabServer(display_);

  // Disable the LCD if we were told to do so (because we're using a higher
  // resolution that'd be clipped on the LCD).
  // Otherwise enable the LCD if appropriate.
  if (lcd_resolution.name.empty())
    DisableDevice(lcd_output_info_);
  else
    SetDeviceResolution(lcd_output_, lcd_output_info_, lcd_resolution);

  const bool was_projecting = is_projecting_;

  // If there's no external output connected, disable the device before we try
  // to set the screen resolution; otherwise xrandr will complain if we're
  // trying to set the screen to a smaller size than what the now-unplugged
  // device was using.
  // Otherwise enable the external device if appropriate.
  if (external_resolution.name.empty()) {
    DisableDevice(external_output_info_);
    is_projecting_ = false;
  } else {
    SetDeviceResolution(external_output_, external_output_info_,
                        external_resolution);
    is_projecting_ = true;
  }

  // Set the fb resolution.
  SetScreenResolution(screen_resolution);

  // Now let the server go and sync all changes.
  XUngrabServer(display_);
  XSync(display_, False);

  if (was_projecting != is_projecting_ && projection_callback_ != NULL)
    projection_callback_(projection_callback_data_);

  // Enable the backlight. We do not want to do this before the resize is
  // done so that we can hide the resize when going off->on.
  if (!lcd_resolution.name.empty() || is_projecting_)
    backlight_ctl_->SetPowerState(BACKLIGHT_ACTIVE);

  if (lcd_output_info_)
    XRRFreeOutputInfo(lcd_output_info_);
  if (external_output_info_)
    XRRFreeOutputInfo(external_output_info_);
  XRRFreeScreenResources(screen_info_);

  XCloseDisplay(display_);
}

void MonitorReconfigure::SetProjectionCallback(void (*func)(void*),
                                               void* data) {
  projection_callback_ = func;
  projection_callback_data_ = data;
}

bool MonitorReconfigure::DetermineOutputs() {
  lcd_output_ = 0;
  external_output_ = 0;

  if (screen_info_->noutput == 0) {
    LOG(WARNING) << "No outputs found\n";
    return false;
  }

  // Take the first two outputs and assign the first to
  // the panel and the second to the external output.
  lcd_output_ = screen_info_->outputs[0];
  lcd_output_info_ = XRRGetOutputInfo(display_, screen_info_, lcd_output_);

  if (screen_info_->noutput > 1) {
    external_output_ = screen_info_->outputs[1];
    external_output_info_ = XRRGetOutputInfo(display_,
                                             screen_info_,
                                             external_output_);

    // If the second found outputs matches "LVDS*" use it for panel instead.
    if (strncmp(external_output_info_->name,
                kLcdOutputName,
                strlen(kLcdOutputName)) == 0) {
      RROutput tmp_output = lcd_output_;
      XRROutputInfo* tmp_output_info = lcd_output_info_;

      lcd_output_ = external_output_;
      lcd_output_info_ = external_output_info_;

      external_output_ = tmp_output;
      external_output_info_ = tmp_output_info;
    }
  }

  LOG(INFO) << "LCD name: " << lcd_output_info_->name << " (xid "
            << lcd_output_ << ")";
  for (int i = 0; i < lcd_output_info_->nmode; ++i) {
    XRRModeInfo* mode = mode_map_[lcd_output_info_->modes[i]];
    LOG(INFO) << "  Mode: " << mode->width << "x" << mode->height
              << " (xid " << mode->id << ")";
  }

  if (external_output_) {
    LOG(INFO) << "External name: " << external_output_info_->name
              << " (xid " << external_output_ << ")";
    for (int i = 0; i < external_output_info_->nmode; ++i) {
      XRRModeInfo* mode = mode_map_[external_output_info_->modes[i]];
      LOG(INFO) << "  Mode: " << mode->width << "x" << mode->height
                << " (xid " << mode->id << ")";
    }
  }

  return true;
}

void MonitorReconfigure::SortModesByResolution(
    RROutput output,
    vector<ResolutionSelector::Mode>* modes_out) {
  modes_out->clear();

  XRROutputInfo* output_info = XRRGetOutputInfo(display_, screen_info_, output);
  for (int i = 0; i < output_info->nmode; ++i) {
    const XRRModeInfo* mode = mode_map_[output_info->modes[i]];
    DCHECK(mode);
    LOG(INFO) << "Adding mode " << mode->width << " " << mode->height
              << " (xid " << mode->id << ")";
    modes_out->push_back(
        ResolutionSelector::Mode(mode->width, mode->height, mode->name,
                                 mode->id));
  }

  sort(modes_out->begin(), modes_out->end(),
       ResolutionSelector::ModeResolutionComparator());

  XRRFreeOutputInfo(output_info);
}

bool MonitorReconfigure::SetDeviceResolution(
    RROutput output,
    const XRROutputInfo* output_info,
    const ResolutionSelector::Mode& resolution) {
  Status s = XRRSetCrtcConfig(display_, screen_info_, output_info->crtcs[0],
                              CurrentTime, 0, 0, resolution.id, RR_Rotate_0,
                              &output, 1);
  return (s == RRSetConfigSuccess);
}

bool MonitorReconfigure::SetScreenResolution(
    const ResolutionSelector::Mode& resolution) {
  LOG(INFO) << "Setting resolution " << resolution.name.c_str() << " "
            << resolution.width << "x" << resolution.height;
  // Do not switch resolutions if we don't need to, this avoids blinking.
  int screen = DefaultScreen(display_);
  if ( (resolution.width != DisplayWidth(display_, screen))||
     (resolution.height != DisplayHeight(display_, screen)) )
    XRRSetScreenSize(display_, window_,
                     resolution.width,
                     resolution.height,
                     resolution.width * kInchInMm / kScreenDpi,
                     resolution.height * kInchInMm / kScreenDpi);
  return true;
}

bool MonitorReconfigure::DisableDevice(const XRROutputInfo* output_info) {
  if (!output_info)
    return true;
  if (!output_info->crtc)
    return true;
  LOG(INFO) << "Disabling output " << output_info->name;
  Status s = XRRSetCrtcConfig(display_, screen_info_, output_info->crtc,
    CurrentTime, 0, 0, None, RR_Rotate_0, NULL, 0);
  return (s == RRSetConfigSuccess);
}

GdkFilterReturn MonitorReconfigure::GdkEventFilter(GdkXEvent* gxevent,
                                                   GdkEvent*) {
  XEvent* xevent = static_cast<XEvent*>(gxevent);
  if (xevent->type == rr_event_base_ + RRScreenChangeNotify) {
    Run();
    // Remove this event so that other programs pick it up and acts upon it.
    return GDK_FILTER_REMOVE;
  }
  return GDK_FILTER_CONTINUE;
}

}  // namespace power_manager
