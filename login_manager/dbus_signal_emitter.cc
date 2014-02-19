// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/dbus_signal_emitter.h"

#include <string>
#include <vector>

#include <base/logging.h>
#include <dbus/exported_object.h>
#include <dbus/message.h>

namespace login_manager {

const char DBusSignalEmitterInterface::kSignalSuccess[] = "success";
const char DBusSignalEmitterInterface::kSignalFailure[] = "failure";

DBusSignalEmitterInterface::~DBusSignalEmitterInterface() {}

DBusSignalEmitter::DBusSignalEmitter(dbus::ExportedObject* object,
                                     const std::string& interface)
    : object_(object),
      interface_(interface) {
}

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
  dbus::Signal signal(interface_, signal_name);
  if (!payload.empty()) {
    dbus::MessageWriter writer(&signal);
    writer.AppendString(payload);
  }
  object_->SendSignal(&signal);
}

}  // namespace login_manager
