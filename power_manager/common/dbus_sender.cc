// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/common/dbus_sender.h"

#include <unistd.h>

#include <climits>

#include <base/logging.h>
#include <dbus/exported_object.h>
#include <dbus/message.h>
#include <google/protobuf/message_lite.h>

namespace power_manager {

DBusSender::DBusSender() : object_(NULL) {}

DBusSender::~DBusSender() {}

void DBusSender::Init(dbus::ExportedObject* object,
                      const std::string& interface) {
  object_ = object;
  interface_ = interface;
}

void DBusSender::EmitBareSignal(const std::string& signal_name) {
  dbus::Signal signal(interface_, signal_name);
  object_->SendSignal(&signal);
}

void DBusSender::EmitSignalWithProtocolBuffer(
    const std::string& signal_name,
    const google::protobuf::MessageLite& protobuf) {
  dbus::Signal signal(interface_, signal_name);
  dbus::MessageWriter writer(&signal);
  writer.AppendProtoAsArrayOfBytes(protobuf);
  object_->SendSignal(&signal);
}

}  // namespace power_manager
