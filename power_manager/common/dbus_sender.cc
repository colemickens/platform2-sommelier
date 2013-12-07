// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/common/dbus_sender.h"

#include <dbus/dbus-glib-lowlevel.h>
#include <google/protobuf/message_lite.h>
#include <unistd.h>

#include <climits>

#include "base/logging.h"
#include "power_manager/common/util_dbus.h"

namespace power_manager {

DBusSender::DBusSender() {}

DBusSender::~DBusSender() {}

void DBusSender::Init(const std::string& path, const std::string& interface) {
  path_ = path;
  interface_ = interface;
}

void DBusSender::EmitBareSignal(const std::string& signal_name) {
  EmitSignalInternal(signal_name, NULL);
}

void DBusSender::EmitSignalWithProtocolBuffer(
    const std::string& signal_name,
    const google::protobuf::MessageLite& protobuf) {
  EmitSignalInternal(signal_name, &protobuf);
}

void DBusSender::EmitSignalInternal(
    const std::string& signal_name,
    const google::protobuf::MessageLite* protobuf) {
  DBusMessage* signal = dbus_message_new_signal(path_.c_str(),
                                                interface_.c_str(),
                                                signal_name.c_str());
  CHECK(signal);
  if (protobuf)
    util::AppendProtocolBufferToDBusMessage(*protobuf, signal);
  CHECK(dbus_connection_send(util::GetSystemDBusConnection(), signal, NULL));
  dbus_message_unref(signal);
}

}  // namespace power_manager
