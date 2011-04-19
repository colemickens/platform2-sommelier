// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/monitor_reconfigure.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>

#include "base/logging.h"
#include "power_manager/backlight_controller.h"
#include "power_manager/udev_controller.h"

using std::sort;
using std::vector;

namespace {
// Name of the internal output.
const char kLcdOutputName[] = "LVDS1";
// The screen DPI we pass to X11.
const float kScreenDpi = 96.0;
// An inch in mm.
const float kInchInMm = 25.4;
}

namespace power_manager {

void MonitorReconfigureDelegate::Run(GIOChannel* /* source */,
                                     GIOCondition /* condition */) {
  monitor_reconfigure_->Run();
}

MonitorReconfigure::MonitorReconfigure()
    : display_(NULL),
      window_(0),
      lcd_output_(0),
      lcd_output_info_(NULL),
      external_output_(0),
      external_output_info_(NULL),
      backlight_ctl_(NULL),
      delegate_(NULL),
      controller_(NULL) {}

MonitorReconfigure::MonitorReconfigure(
    BacklightController* backlight_ctl)
    : display_(NULL),
      window_(0),
      lcd_output_(0),
      lcd_output_info_(NULL),
      external_output_(0),
      external_output_info_(NULL),
      backlight_ctl_(backlight_ctl),
      delegate_(NULL),
      controller_(NULL) {}

MonitorReconfigure::~MonitorReconfigure() {
}

bool MonitorReconfigure::Init() {
  DCHECK(delegate_ == NULL);
  delegate_ = new MonitorReconfigureDelegate(this);
  controller_ = new UdevController(delegate_, "drm");
  return controller_->Init();
}

void MonitorReconfigure::Run() {
  lcd_output_ = 0;
  lcd_output_info_ = NULL;
  external_output_ = 0;
  external_output_info_ = NULL;
  display_ = XOpenDisplay(NULL);
  CHECK(display_) << "Could not open display";

  window_ = RootWindow(display_, DefaultScreen(display_));
  screen_info_ = XRRGetScreenResources(display_, window_);

  for (int i = 0; i < screen_info_->nmode; ++i) {
    XRRModeInfo* current_mode = &screen_info_->modes[i];
    mode_map_[current_mode->id] = current_mode;
  }
  DetermineOutputs();
  vector<ResolutionSelector::Mode> lcd_modes;
  SortModesByResolution(lcd_output_, &lcd_modes);
  DCHECK(!lcd_modes.empty());

  vector<ResolutionSelector::Mode> external_modes;
  if (IsExternalMonitorConnected()) {
    SortModesByResolution(external_output_, &external_modes);
    DCHECK(!external_modes.empty());
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
  CHECK(!lcd_resolution.name.empty() || !external_resolution.name.empty());

  // Disable the backlight before resizing anything on the screen.
  if (lcd_resolution.name.empty())
    backlight_ctl_->SetPowerState(BACKLIGHT_ACTIVE_OFF);

  // Grab the server during mode changes.
  XGrabServer(display_);

  // Disable the LCD if we were told to do so (because we're using a higher
  // resolution that'd be clipped on the LCD).
  // Otherwise enable the LCD if appropriate.
  if (lcd_resolution.name.empty())
    DisableDevice(lcd_output_info_);
  else
    SetDeviceResolution(lcd_output_, lcd_output_info_, lcd_resolution);

  // If there's no external output connected, disable the device before we try
  // to set the screen resolution; otherwise xrandr will complain if we're
  // trying to set the screen to a smaller size than what the now-unplugged
  // device was using.
  // Otherwise enable the external device if appropriate.
  if (external_resolution.name.empty())
    DisableDevice(external_output_info_);
  else
    SetDeviceResolution(external_output_, external_output_info_,
                        external_resolution);

  // Set the fb resolution.
  SetScreenResolution(screen_resolution);

  // Now let the server go and sync all changes.
  XUngrabServer(display_);
  XSync(display_, False);

  XRRFreeOutputInfo(lcd_output_info_);
  XRRFreeOutputInfo(external_output_info_);
  XRRFreeScreenResources(screen_info_);

  // Enable the backlight. We do not want to do this before the resize is
  // done so that we can hide the resize when going off->on.
  if (!lcd_resolution.name.empty())
    backlight_ctl_->SetPowerState(BACKLIGHT_ACTIVE_ON);

  XCloseDisplay(display_);
}

void MonitorReconfigure::DetermineOutputs() {
  CHECK(screen_info_->noutput > 1) << "Expected at least two outputs";
  CHECK(!lcd_output_info_);
  CHECK(!external_output_info_);

  RROutput first_output = screen_info_->outputs[0];
  RROutput second_output = screen_info_->outputs[1];

  XRROutputInfo* first_output_info =
      XRRGetOutputInfo(display_, screen_info_, first_output);
  XRROutputInfo* second_output_info =
      XRRGetOutputInfo(display_, screen_info_, second_output);

  if (strcmp(first_output_info->name, kLcdOutputName) == 0) {
    lcd_output_ = first_output;
    lcd_output_info_ = first_output_info;
    external_output_ = second_output;
    external_output_info_ = second_output_info;
  } else {
    lcd_output_ = second_output;
    lcd_output_info_ = second_output_info;
    external_output_ = first_output;
    external_output_info_ = first_output_info;
  }

  LOG(INFO) << "LCD name: " << lcd_output_info_->name << " (xid "
            << lcd_output_ << ")";
  for (int i = 0; i < lcd_output_info_->nmode; ++i) {
    XRRModeInfo* mode = mode_map_[lcd_output_info_->modes[i]];
    LOG(INFO) << "  Mode: " << mode->width << "x" << mode->height
              << " (xid " << mode->id << ")";
  }

  LOG(INFO) << "External name: " << external_output_info_->name
            << " (xid " << external_output_ << ")";
  for (int i = 0; i < external_output_info_->nmode; ++i) {
    XRRModeInfo* mode = mode_map_[external_output_info_->modes[i]];
    LOG(INFO) << "  Mode: " << mode->width << "x" << mode->height
              << " (xid " << mode->id << ")";
  }
}

bool MonitorReconfigure::IsExternalMonitorConnected() {
  return (external_output_info_->connection == RR_Connected);
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
  if (!output_info->crtc)
    return true;
  LOG(INFO) << "Disabling output " << output_info->name;
  Status s = XRRSetCrtcConfig(display_, screen_info_, output_info->crtc,
    CurrentTime, 0, 0, None, RR_Rotate_0, NULL, 0);
  return (s == RRSetConfigSuccess);
}

}  // namespace power_manager
