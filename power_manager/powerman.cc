// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gdk/gdkx.h>

#include "base/string_util.h"
#include "chromeos/dbus/dbus.h"
#include "chromeos/dbus/service_constants.h"
#include "power_manager/powerman.h"
#include "power_manager/util.h"

namespace power_manager {

PowerManDaemon::PowerManDaemon(bool use_input_for_lid,
                               bool use_input_for_key_power)
  : loop_(NULL),
    use_input_for_lid_(use_input_for_lid),
    use_input_for_key_power_(use_input_for_key_power),
    lidstate_(false),
    power_button_state_(false) {}

void PowerManDaemon::Init() {
  loop_ = g_main_loop_new(NULL, false);
  CHECK(input_.Init(use_input_for_lid_, use_input_for_key_power_))
    <<"Cannot initialize input interface";
  input_.RegisterHandler(&(PowerManDaemon::OnInputEvent), this);
  RegisterDBusMessageHandler();
}

void PowerManDaemon::Run() {
  g_main_loop_run(loop_);
}

void PowerManDaemon::OnInputEvent(void* object, InputType type, int value) {
  PowerManDaemon* daemon = static_cast<PowerManDaemon*>(object);
  switch(type) {
    case LID: {
      // value == 0 is open. value == 1 is closed.
      LOG(INFO) << "PowerM Daemon - lid " << (1 == value?"closed.":"opened.");
      daemon->lidstate_ = (value == 1);
      if (value)
        util::SendSignalToPowerD(util::kLidClosed);
      else
        util::SendSignalToPowerD(util::kLidOpened);
      break;
    }
    case PWRBUTTON: {
      // value == 0 is button up. value == 1 is button down.
      // value == 2 is key repeat.
      LOG(INFO) << "PowerM Daemon - power button " << (0 < value?"down.":"up.");
      daemon->power_button_state_ = value;
      if (0 < value)
        util::SendSignalToPowerD(util::kPowerButtonDown);
      else
        util::SendSignalToPowerD(util::kPowerButtonUp);
      break;
    }
    default: {
      LOG(ERROR) << "Bad input type.";
      NOTREACHED();
      break;
    }
  }
}

DBusHandlerResult PowerManDaemon::DBusMessageHandler(
    DBusConnection*, DBusMessage* message, void* data) {
  PowerManDaemon* daemon = static_cast<PowerManDaemon*>(data);
  if (dbus_message_is_signal(message, util::kLowerPowerManagerInterface,
                             util::kSuspendSignal)) {
    LOG(INFO) << "Suspend event";
    LOG(INFO) << "lid is " << (1 == daemon->lidstate_?"closed.":"opened.");
    daemon->Suspend();
  } else if (dbus_message_is_signal(message, util::kLowerPowerManagerInterface,
                                    util::kShutdownSignal)) {
    LOG(INFO) << "Shutdown event";
    LOG(INFO) << "power button is "
              << (0 < daemon->power_button_state_?"down.":"up.");
    daemon->Shutdown();
  } else {
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
  }
  return DBUS_HANDLER_RESULT_HANDLED;
}

void PowerManDaemon::RegisterDBusMessageHandler() {
  const std::string filter = StringPrintf("type='signal', interface='%s'",
                                          util::kLowerPowerManagerInterface);
  DBusError error;
  dbus_error_init(&error);
  DBusConnection* connection = dbus_g_connection_get_connection(
      chromeos::dbus::GetSystemBusConnection().g_connection());
  dbus_bus_add_match(connection, filter.c_str(), &error);
  if (dbus_error_is_set(&error)) {
    LOG(ERROR) << "Failed to add a filter:" << error.name << ", message="
               << error.message;
    NOTREACHED();
  } else {
    CHECK(dbus_connection_add_filter(connection, &DBusMessageHandler, this,
                                     NULL));
    LOG(INFO) << "DBus monitoring started";
  }
}

void PowerManDaemon::Shutdown() {
  util::Launch("shutdown -P now");
}

void PowerManDaemon::Suspend() {
  util::Launch("powerd_suspend");
}

}

