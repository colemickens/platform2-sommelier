// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/system/dbus_wrapper_stub.h"

#include <memory>

#include <base/logging.h>

namespace power_manager {
namespace system {

DBusWrapperStub::DBusWrapperStub() {}

DBusWrapperStub::~DBusWrapperStub() {}

bool DBusWrapperStub::GetSentSignal(size_t index,
                                    const std::string& expected_signal_name,
                                    google::protobuf::MessageLite* protobuf) {
  if (index >= sent_signals_.size()) {
    LOG(ERROR) << "Got request to return " << expected_signal_name << " signal "
               << "at position " << index << ", but only "
               << sent_signals_.size() << " were sent";
    return false;
  }

  SignalInfo* info = sent_signals_[index];
  if (info->signal_name != expected_signal_name) {
    LOG(ERROR) << "Expected " << expected_signal_name << " signal at position "
               << index << " but had " << info->signal_name << " instead";
    return false;
  }

  if (protobuf) {
    if (info->protobuf_type != protobuf->GetTypeName()) {
      LOG(ERROR) << info->signal_name << " signal at position " << index
                 << " has " << info->protobuf_type << " protobuf instead of "
                 << "expected " << protobuf->GetTypeName();
      return false;
    }

    if (!protobuf->ParseFromString(sent_signals_[index]->serialized_data)) {
      LOG(ERROR) << "Unable to parse " << info->protobuf_type
                 << " protobuf from " << info->signal_name
                 << " signal at position " << index;
      return false;
    }
  }

  return true;
}

void DBusWrapperStub::ClearSentSignals() {
  sent_signals_.clear();
}

void DBusWrapperStub::EmitBareSignal(const std::string& signal_name) {
  std::unique_ptr<SignalInfo> info(new SignalInfo);
  info->signal_name = signal_name;
  sent_signals_.push_back(info.release());
}

void DBusWrapperStub::EmitSignalWithProtocolBuffer(
    const std::string& signal_name,
    const google::protobuf::MessageLite& protobuf) {
  std::unique_ptr<SignalInfo> info(new SignalInfo);
  info->signal_name = signal_name;
  info->protobuf_type = protobuf.GetTypeName();
  protobuf.SerializeToString(&(info->serialized_data));
  sent_signals_.push_back(info.release());
}

}  // namespace system
}  // namespace power_manager
