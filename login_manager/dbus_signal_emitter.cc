// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/dbus_signal_emitter.h"

#include <string>
#include <vector>

#include <base/basictypes.h>
#include <base/logging.h>
#include <chromeos/dbus/dbus.h>
#include <chromeos/dbus/service_constants.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <dbus/dbus-glib.h>

namespace login_manager {

const char DBusSignalEmitterInterface::kSignalSuccess[] = "success";
const char DBusSignalEmitterInterface::kSignalFailure[] = "failure";

DBusSignalEmitterInterface::~DBusSignalEmitterInterface() {}

DBusSignalEmitter::DBusSignalEmitter() {}

DBusSignalEmitter::~DBusSignalEmitter() {}

void DBusSignalEmitter::EmitSignal(const std::string& signal_name) {
  EmitSignalWithString(signal_name, std::string());
}

void DBusSignalEmitter::EmitSignalWithSuccessFailure(
    const std::string& signal_name,
    const bool success) {
  EmitSignalWithString(signal_name, success ? kSignalSuccess : kSignalFailure);
}

void DBusSignalEmitter::EmitSignalWithString(const std::string& signal_name,
                                             const std::string& payload) {
  std::vector<std::string> to_pass;
  to_pass.push_back(payload);
  EmitSignalFrom(kSessionManagerInterface, signal_name, to_pass);
}

void DBusSignalEmitter::EmitSignalFrom(
    const std::string& interface,
    const std::string& signal_name,
    const std::vector<std::string>& payload) {
  chromeos::dbus::Proxy proxy(chromeos::dbus::GetSystemBusConnection(),
                              kSessionManagerServicePath,
                              interface.c_str());
  if (!proxy) {
    LOG(ERROR) << "No proxy; can't signal " << interface;
    return;
  }
  DBusMessage* signal =
      ::dbus_message_new_signal(kSessionManagerServicePath,
                                interface.c_str(),
                                signal_name.c_str());
  if (!payload.empty()) {
    for (std::vector<std::string>::const_iterator it = payload.begin();
         it != payload.end();
         ++it) {
      // Need to be able to take the address of the value to append.
      const char* string_arg = it->c_str();
      dbus_message_append_args(signal,
                               DBUS_TYPE_STRING, &string_arg,
                               DBUS_TYPE_INVALID);
    }
  }
  ::dbus_g_proxy_send(proxy.gproxy(), signal, NULL);
  ::dbus_message_unref(signal);
}

}  // namespace login_manager
