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
using std::string;
using std::vector;

const char DBusSignalEmitterInterface::kSignalSuccess[] = "success";
const char DBusSignalEmitterInterface::kSignalFailure[] = "failure";

DBusSignalEmitterInterface::~DBusSignalEmitterInterface() {}

DBusSignalEmitter::DBusSignalEmitter() {}

DBusSignalEmitter::~DBusSignalEmitter() {}

void DBusSignalEmitter::EmitSignal(const char* signal_name) {
  EmitSignalWithStringArgs(signal_name, vector<string>());
}

void DBusSignalEmitter::EmitSignalWithStringArgs(
    const char* signal_name,
    const vector<string>& payload) {
  EmitSignalFrom(chromium::kChromiumInterface, signal_name, payload);
  EmitSignalFrom(login_manager::kSessionManagerInterface, signal_name, payload);
}

void DBusSignalEmitter::EmitSignalWithBoolean(const char* signal_name,
                                              bool status) {
  EmitSignalFrom(chromium::kChromiumInterface, signal_name,
                 vector<string>(1, status ? kSignalSuccess : kSignalFailure));
  EmitSignalFrom(login_manager::kSessionManagerInterface, signal_name,
                 vector<string>(1, status ? kSignalSuccess : kSignalFailure));
}

void DBusSignalEmitter::EmitSignalFrom(const char* interface,
                                       const char* signal_name,
                                       const vector<string>& payload) {
  chromeos::dbus::Proxy proxy(chromeos::dbus::GetSystemBusConnection(),
                              login_manager::kSessionManagerServicePath,
                              interface);
  if (!proxy) {
    LOG(ERROR) << "No proxy; can't signal " << interface;
    return;
  }
  DBusMessage* signal = ::dbus_message_new_signal(
      login_manager::kSessionManagerServicePath, interface, signal_name);
  if (!payload.empty()) {
    for (vector<string>::const_iterator it = payload.begin();
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
