// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/system/dbus_wrapper.h"

#include <unistd.h>

#include <climits>

#include <base/logging.h>
#include <dbus/exported_object.h>
#include <dbus/message.h>
#include <google/protobuf/message_lite.h>

namespace power_manager {
namespace system {

DBusWrapper::DBusWrapper() : object_(nullptr) {}

DBusWrapper::~DBusWrapper() {}

void DBusWrapper::Init(dbus::ExportedObject* object,
                       const std::string& interface) {
  object_ = object;
  interface_ = interface;
}

void DBusWrapper::EmitBareSignal(const std::string& signal_name) {
  dbus::Signal signal(interface_, signal_name);
  object_->SendSignal(&signal);
}

void DBusWrapper::EmitSignalWithProtocolBuffer(
    const std::string& signal_name,
    const google::protobuf::MessageLite& protobuf) {
  dbus::Signal signal(interface_, signal_name);
  dbus::MessageWriter writer(&signal);
  writer.AppendProtoAsArrayOfBytes(protobuf);
  object_->SendSignal(&signal);
}

}  // namespace system
}  // namespace power_manager
