// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/monitor_reconfigure.h"

#include "base/logging.h"
#include "base/string_util.h"
#include "chromeos/dbus/service_constants.h"
#include "power_manager/power_constants.h"
#include "power_manager/backlight_controller.h"
#include "power_manager/powerd.h"
#include "power_manager/util.h"

namespace power_manager {

MonitorReconfigure::MonitorReconfigure()
    : is_internal_panel_enabled_(true) {
}

MonitorReconfigure::~MonitorReconfigure() {
}

void MonitorReconfigure::SetScreenOn() {
  LOG(INFO) << "MonitorReconfigure::SetScreenOn()";
  DisableTouchDevices();
  SendSetScreenPowerSignal(POWER_STATE_ON, OUTPUT_SELECTION_ALL_DISPLAYS);
  EnableTouchDevices(true);
}

void MonitorReconfigure::SetScreenOff() {
  LOG(INFO) << "MonitorReconfigure::SetScreenOff()";
  DisableTouchDevices();
  SendSetScreenPowerSignal(POWER_STATE_OFF, OUTPUT_SELECTION_ALL_DISPLAYS);
  EnableTouchDevices(false);
}

void MonitorReconfigure::SetInternalPanelOn() {
  if (is_internal_panel_enabled_)
    return;

  LOG(INFO) << "MonitorReconfigure::SetInternalPanelOn()";
  is_internal_panel_enabled_ = true;
  DisableTouchDevices();
  SendSetScreenPowerSignal(POWER_STATE_ON, OUTPUT_SELECTION_INTERNAL_ONLY);
  EnableTouchDevices(true);
}

void MonitorReconfigure::SetInternalPanelOff() {
  if (!is_internal_panel_enabled_)
    return;

  LOG(INFO) << "MonitorReconfigure::SetInternalPanelOff()";
  is_internal_panel_enabled_ = false;
  DisableTouchDevices();
  SendSetScreenPowerSignal(POWER_STATE_OFF, OUTPUT_SELECTION_INTERNAL_ONLY);
  EnableTouchDevices(false);
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

void MonitorReconfigure::DisableTouchDevices() {
#ifdef TOUCH_DEVICE
  DBusMessage* message = dbus_message_new_method_call(
      kRootPowerManagerServiceName,
      kPowerManagerServicePath,
      kRootPowerManagerInterface,
      kDisableTouchDevicesMethod);
  LOG(INFO) << "DisableTouchDevices";
  CHECK(message);
  dbus_message_append_args(message,
                           DBUS_TYPE_INVALID);
  DBusConnection* connection = dbus_g_connection_get_connection(
      chromeos::dbus::GetSystemBusConnection().g_connection());
  DBusError error;
  dbus_error_init(&error);
  DBusMessage* reply = dbus_connection_send_with_reply_and_block(
      connection, message, -1, &error);
  if (!reply) {
    LOG(WARNING) << "Error sending " << kDisableTouchDevicesMethod
                 << " method call: " << error.message;
    dbus_error_free(&error);
    return;
  }
#endif // TOUCH_DEVICE
}

void MonitorReconfigure::EnableTouchDevices(bool display_on) {
#ifdef TOUCH_DEVICE
  dbus_bool_t dbus_display_on = display_on ? TRUE : FALSE;
  DBusMessage* message = dbus_message_new_method_call(
      kRootPowerManagerServiceName,
      kPowerManagerServicePath,
      kRootPowerManagerInterface,
      kEnableTouchDevicesMethod);
  CHECK(message);
  LOG(INFO) << "EnableTouchDevices";
  dbus_message_append_args(message,
                           DBUS_TYPE_BOOLEAN, &dbus_display_on,
                           DBUS_TYPE_INVALID);
  DBusConnection* connection = dbus_g_connection_get_connection(
      chromeos::dbus::GetSystemBusConnection().g_connection());
  if (dbus_connection_send(connection, message, NULL) == FALSE) {
    LOG(WARNING) << "Error sending " << kEnableTouchDevicesMethod
                 << " method call.";
  }
#endif // TOUCH_DEVICE
}

}  // namespace power_manager
