// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/common/dbus_sender.h"

#include <dbus/dbus-glib-lowlevel.h>
#include <google/protobuf/message_lite.h>
#include <unistd.h>

#include <climits>

#include "base/logging.h"
#include "chromeos/dbus/dbus.h"

namespace power_manager {

DBusSender::DBusSender(const std::string& path,
                       const std::string& interface)
    : path_(path),
      interface_(interface) {
}

DBusSender::~DBusSender() {}

void DBusSender::EmitSignalWithProtocolBuffer(
    const std::string& signal_name,
    const google::protobuf::MessageLite& protobuf) {
  DBusMessage* signal = dbus_message_new_signal(path_.c_str(),
                                                interface_.c_str(),
                                                signal_name.c_str());

  std::string serialized_protobuf;
  CHECK(protobuf.SerializeToString(&serialized_protobuf))
      << "Unable to serialize " << signal_name << " protocol buffer";
  const uint8* data =
      reinterpret_cast<const uint8*>(serialized_protobuf.data());
  CHECK(serialized_protobuf.size() <= static_cast<size_t>(INT_MAX))
      << "Protocol buffer for " << signal_name << " is "
      << serialized_protobuf.size() << " bytes";
  dbus_message_append_args(signal,
                           DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE,
                           &data, static_cast<int>(serialized_protobuf.size()),
                           DBUS_TYPE_INVALID);

  DBusConnection* connection = dbus_g_connection_get_connection(
      chromeos::dbus::GetSystemBusConnection().g_connection());
  CHECK(dbus_connection_send(connection, signal, NULL));
  dbus_message_unref(signal);
}

}  // namespace power_manager
