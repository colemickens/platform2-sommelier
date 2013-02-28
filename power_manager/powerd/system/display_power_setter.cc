// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/system/display_power_setter.h"

#include <dbus/dbus-glib-lowlevel.h>

#include "base/logging.h"
#include "chromeos/dbus/dbus.h"
#include "power_manager/common/util.h"

namespace power_manager {
namespace system {

namespace {

std::string DisplayPowerStateToString(chromeos::DisplayPowerState state) {
  switch (state) {
    case chromeos::DISPLAY_POWER_ALL_ON:
      return "all displays on";
    case chromeos::DISPLAY_POWER_ALL_OFF:
      return "all displays off";
    case chromeos::DISPLAY_POWER_INTERNAL_OFF_EXTERNAL_ON:
      return "internal display off and external displays on";
    case chromeos::DISPLAY_POWER_INTERNAL_ON_EXTERNAL_OFF:
      return "internal display on and external displays off";
    default:
      return "[unknown]";
  }
}

}  // namespace

DisplayPowerSetter::DisplayPowerSetter() : timeout_id_(0) {}

DisplayPowerSetter::~DisplayPowerSetter() {
  util::RemoveTimeout(&timeout_id_);
}

void DisplayPowerSetter::SetDisplayPower(chromeos::DisplayPowerState state,
                                         base::TimeDelta delay) {
  util::RemoveTimeout(&timeout_id_);
  if (delay.InMilliseconds() == 0) {
    SendStateToChrome(state);
  } else {
    timeout_id_ = g_timeout_add(delay.InMilliseconds(),
                                HandleTimeoutThunk,
                                CreateHandleTimeoutArgs(this, state));
  }
}

void DisplayPowerSetter::SendStateToChrome(chromeos::DisplayPowerState state) {
  LOG(INFO) << "Asking Chrome to turn " << DisplayPowerStateToString(state);
  DBusMessage* message = dbus_message_new_method_call(
      chromeos::kLibCrosServiceName,
      chromeos::kLibCrosServicePath,
      chromeos::kLibCrosServiceInterface,
      chromeos::kSetDisplayPower);
  CHECK(message);
  dbus_message_append_args(message, DBUS_TYPE_INT32, &state, DBUS_TYPE_INVALID);

  DBusConnection* connection = dbus_g_connection_get_connection(
      chromeos::dbus::GetSystemBusConnection().g_connection());
  CHECK(connection);
  dbus_connection_send(connection, message, NULL);
  dbus_message_unref(message);
}

gboolean DisplayPowerSetter::HandleTimeout(chromeos::DisplayPowerState state) {
  SendStateToChrome(state);
  timeout_id_ = 0;
  return FALSE;
}

}  // system
}  // power_manager
