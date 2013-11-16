// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/system/display_power_setter.h"

#include <dbus/dbus-glib-lowlevel.h>

#include "base/bind.h"
#include "base/logging.h"
#include "power_manager/common/util.h"
#include "power_manager/common/util_dbus.h"

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

// Allocates a new DBusMessage for calling |method| in Chrome.
DBusMessage* CreateChromeMethodCall(const std::string& method) {
  DBusMessage* message = dbus_message_new_method_call(
      chromeos::kLibCrosServiceName,
      chromeos::kLibCrosServicePath,
      chromeos::kLibCrosServiceInterface,
      method.c_str());
  CHECK(message);
  return message;
}

// Sends |message| asynchronously and unrefs it.
void SendAndUnrefMessage(DBusMessage* message) {
  dbus_connection_send(util::GetSystemDBusConnection(), message, NULL);
  dbus_message_unref(message);
}

}  // namespace

DisplayPowerSetter::DisplayPowerSetter() {}

DisplayPowerSetter::~DisplayPowerSetter() {}

void DisplayPowerSetter::SetDisplayPower(chromeos::DisplayPowerState state,
                                         base::TimeDelta delay) {
  if (delay.InMilliseconds() == 0) {
    timer_.Stop();
    SendStateToChrome(state);
  } else {
    timer_.Start(FROM_HERE, delay,
        base::Bind(&DisplayPowerSetter::SendStateToChrome,
                   base::Unretained(this), state));
  }
}

void DisplayPowerSetter::SetDisplaySoftwareDimming(bool dimmed) {
  LOG(INFO) << "Asking Chrome to " << (dimmed ? "dim" : "undim")
            << " the display in software";
  DBusMessage* message =
      CreateChromeMethodCall(chromeos::kSetDisplaySoftwareDimming);
  int dimmed_int = dimmed;
  dbus_message_append_args(
      message, DBUS_TYPE_BOOLEAN, &dimmed_int, DBUS_TYPE_INVALID);
  SendAndUnrefMessage(message);
}

void DisplayPowerSetter::SendStateToChrome(chromeos::DisplayPowerState state) {
  LOG(INFO) << "Asking Chrome to turn " << DisplayPowerStateToString(state);
  DBusMessage* message = CreateChromeMethodCall(chromeos::kSetDisplayPower);
  dbus_message_append_args(message, DBUS_TYPE_INT32, &state, DBUS_TYPE_INVALID);
  SendAndUnrefMessage(message);
}

}  // system
}  // power_manager
