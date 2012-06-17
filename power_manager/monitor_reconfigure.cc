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

namespace power_manager {

MonitorReconfigure::MonitorReconfigure()
    : is_internal_panel_enabled_(true),
      backlight_ctl_(NULL) {
}

MonitorReconfigure::MonitorReconfigure(BacklightController* backlight_ctl)
    : is_internal_panel_enabled_(true),
      backlight_ctl_(backlight_ctl) {
}

MonitorReconfigure::~MonitorReconfigure() {
}

bool MonitorReconfigure::Init() {
  if (backlight_ctl_)
    backlight_ctl_->SetMonitorReconfigure(this);
  return true;
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
