// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/monitor_reconfigure.h"

#include "base/logging.h"
#include "base/string_util.h"
#include "chromeos/dbus/service_constants.h"
#include "power_manager/backlight_controller.h"
#include "power_manager/powerd.h"
#include "power_manager/util.h"

namespace {

// Name of the internal output.
const char kInternalPanelName[] = "LVDS";

// TODO(miletus@) : May need a different implementation for Arrow.
bool IsInternalPanel(XRROutputInfo* output_info) {
  return StartsWithASCII(output_info->name, kInternalPanelName, true);
}

}  // namespace

namespace power_manager {

MonitorReconfigure::MonitorReconfigure()
    : display_(NULL),
      window_(None),
      screen_info_(NULL),
      internal_panel_connection_(RR_UnknownConnection),
      is_internal_panel_enabled_(true),
      is_projecting_(false),
      backlight_ctl_(NULL) {
}

MonitorReconfigure::MonitorReconfigure(BacklightController* backlight_ctl)
    : display_(NULL),
      window_(None),
      screen_info_(NULL),
      internal_panel_connection_(RR_UnknownConnection),
      is_internal_panel_enabled_(true),
      is_projecting_(false),
      backlight_ctl_(backlight_ctl) {
}

MonitorReconfigure::~MonitorReconfigure() {
}

bool MonitorReconfigure::Init() {
  display_ = util::GetDisplay();

  if (backlight_ctl_)
    backlight_ctl_->SetMonitorReconfigure(this);

  CheckInternalPanelConnection();

  return true;
}

bool MonitorReconfigure::SetupXrandr() {
  CHECK(None == window_);
  window_ = RootWindow(display_, DefaultScreen(display_));
  CHECK(NULL == screen_info_);
  screen_info_ = XRRGetScreenResources(display_, window_);

  // Give up if we can't obtain the XRandr information.
  if (!screen_info_) {
    LOG(WARNING) << "Could not get XRandr information";
    window_ = None;
    return false;
  }

  return true;
}

void MonitorReconfigure::ClearXrandr() {
  CHECK(NULL != screen_info_);
  XRRFreeScreenResources(screen_info_);
  screen_info_ = NULL;
  CHECK(None != window_);
  window_ = None;
}

void MonitorReconfigure::SetScreenOn() {
  LOG(INFO) << "MonitorReconfigure::SetScreenOn()";
  SendSetScreenPowerSignal(POWER_STATE_ON,
                           OUTPUT_SELECTION_ALL_DISPLAYS);
}

void MonitorReconfigure::SetScreenOff() {
  LOG(INFO) << "MonitorReconfigure::SetScreenOff()";
  SendSetScreenPowerSignal(POWER_STATE_OFF,
                           OUTPUT_SELECTION_ALL_DISPLAYS);
}

void MonitorReconfigure::SetInternalPanelOn() {
  if (is_internal_panel_enabled_)
    return;

  LOG(INFO) << "MonitorReconfigure::SetInternalPanelOn()";
  is_internal_panel_enabled_ = true;
  SendSetScreenPowerSignal(POWER_STATE_ON,
                           OUTPUT_SELECTION_INTERNAL_ONLY);
}

void MonitorReconfigure::SetInternalPanelOff() {
  LOG(INFO) << "MonitorReconfigure::SetInternalPanelOff()";
  is_internal_panel_enabled_ = false;
  SendSetScreenPowerSignal(POWER_STATE_OFF,
                           OUTPUT_SELECTION_INTERNAL_ONLY);
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

void MonitorReconfigure::SetIsProjecting(bool is_projecting) {
  bool old_value = is_projecting_;
  is_projecting_ = is_projecting;
  if ((is_projecting_ != old_value) && (projection_callback_ != NULL))
    projection_callback_(projection_callback_data_);
}

void MonitorReconfigure::SetProjectionCallback(void (*func)(void*),
                                               void* data) {
  projection_callback_ = func;
  projection_callback_data_ = data;
}

void MonitorReconfigure::SendSetScreenPowerSignal(ScreenPowerState power_state,
    ScreenPowerOutputSelection output_selection) {
  chromeos::dbus::Proxy proxy(chromeos::dbus::GetSystemBusConnection(),
                              kPowerManagerServicePath,
                              kPowerManagerInterface);
  DBusMessage* signal = dbus_message_new_signal(kPowerManagerServicePath,
                                                kPowerManagerInterface,
                                                kSetScreenPowerSignal);
  CHECK(NULL != signal);
  dbus_bool_t set_power_on = (POWER_STATE_ON == power_state);
  dbus_bool_t is_all_displays
      = (OUTPUT_SELECTION_ALL_DISPLAYS == output_selection);
  dbus_message_append_args(signal,
                           DBUS_TYPE_BOOLEAN, &set_power_on,
                           DBUS_TYPE_BOOLEAN, &is_all_displays,
                           DBUS_TYPE_INVALID);
  dbus_g_proxy_send(proxy.gproxy(), signal, NULL);
  dbus_message_unref(signal);
}

}  // namespace power_manager
