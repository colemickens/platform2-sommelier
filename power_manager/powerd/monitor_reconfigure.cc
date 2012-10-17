// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/monitor_reconfigure.h"

#include "base/logging.h"
#include "base/string_util.h"
#include "chromeos/dbus/dbus.h"
#include "chromeos/dbus/service_constants.h"
#include "power_manager/common/util.h"
#include "power_manager/powerd/backlight_controller.h"
#include "power_manager/powerd/powerd.h"

namespace power_manager {

namespace {

const char* ScreenPowerStateToString(ScreenPowerState state) {
  switch (state) {
    case POWER_STATE_ON:
      return "on";
    case POWER_STATE_OFF:
      return "off";
    default:
      return "unknown";
  }
}

const char* ScreenPowerOutputSelectionToString(
    ScreenPowerOutputSelection selection) {
  switch (selection) {
    case OUTPUT_SELECTION_ALL_DISPLAYS:
      return "all displays";
    case OUTPUT_SELECTION_INTERNAL_ONLY:
      return "internal display";
    default:
      return "unknown";
  }
}

}  // namespace

MonitorReconfigure::MonitorReconfigure()
    : is_internal_panel_enabled_(true) {
}

MonitorReconfigure::~MonitorReconfigure() {
}

void MonitorReconfigure::SetScreenPowerState(
    ScreenPowerOutputSelection selection,
    ScreenPowerState state) {
  dbus_bool_t is_all_displays = (selection == OUTPUT_SELECTION_ALL_DISPLAYS);
  dbus_bool_t set_power_on = (state == POWER_STATE_ON);

  if (selection == OUTPUT_SELECTION_INTERNAL_ONLY) {
    if (is_internal_panel_enabled_ == set_power_on)
      return;
    is_internal_panel_enabled_ = set_power_on;
  }

  LOG(INFO) << "Sending signal asking Chrome to turn "
            << ScreenPowerOutputSelectionToString(selection) << " "
            << ScreenPowerStateToString(state);

  chromeos::dbus::Proxy proxy(chromeos::dbus::GetSystemBusConnection(),
                              kPowerManagerServicePath,
                              kPowerManagerInterface);
  DBusMessage* signal = dbus_message_new_signal(kPowerManagerServicePath,
                                                kPowerManagerInterface,
                                                kSetScreenPowerSignal);
  dbus_message_append_args(signal,
                           DBUS_TYPE_BOOLEAN, &set_power_on,
                           DBUS_TYPE_BOOLEAN, &is_all_displays,
                           DBUS_TYPE_INVALID);
  dbus_g_proxy_send(proxy.gproxy(), signal, NULL);
  dbus_message_unref(signal);
}

}  // namespace power_manager
