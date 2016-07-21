// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/system/dbus_wrapper_stub.h"

#include <memory>

#include <base/logging.h>
#include <dbus/message.h>

namespace power_manager {
namespace system {

DBusWrapperStub::DBusWrapperStub() : service_published_(false) {}

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

dbus::Bus* DBusWrapperStub::GetBus() {
  return nullptr;
}

dbus::ObjectProxy* DBusWrapperStub::GetObjectProxy(
    const std::string& service_name,
    const std::string& object_path) {
  // TODO(derat): Return canned proxies.
  return nullptr;
}

void DBusWrapperStub::RegisterForServiceAvailability(
    dbus::ObjectProxy* proxy,
    dbus::ObjectProxy::WaitForServiceToBeAvailableCallback callback) {
  DCHECK(proxy);
  // TODO(derat): Record registered services.
}

void DBusWrapperStub::RegisterForSignal(
    dbus::ObjectProxy* proxy,
    const std::string& interface_name,
    const std::string& signal_name,
    dbus::ObjectProxy::SignalCallback callback) {
  DCHECK(proxy);
  // TODO(derat): Record registered signals.
}

void DBusWrapperStub::ExportMethod(
    const std::string& method_name,
    dbus::ExportedObject::MethodCallCallback callback) {
  CHECK(!service_published_) << "Method " << method_name
                             << " exported after service already published";
  // TODO(derat): Record exported methods.
}

bool DBusWrapperStub::PublishService() {
  CHECK(!service_published_) << "Service already published";
  service_published_ = true;
  return true;
}

void DBusWrapperStub::EmitSignal(dbus::Signal* signal) {
  DCHECK(signal);
  // TODO(derat): Record emitted signals.
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

std::unique_ptr<dbus::Response> DBusWrapperStub::CallMethodSync(
    dbus::ObjectProxy* proxy,
    dbus::MethodCall* method_call,
    base::TimeDelta timeout) {
  DCHECK(proxy);
  DCHECK(method_call);
  // TODO(derat): Return canned response.
  return std::unique_ptr<dbus::Response>();
}

void DBusWrapperStub::CallMethodAsync(
    dbus::ObjectProxy* proxy,
    dbus::MethodCall* method_call,
    base::TimeDelta timeout,
    dbus::ObjectProxy::ResponseCallback callback) {
  DCHECK(proxy);
  DCHECK(method_call);
  // TODO(derat): Invoke callback with canned response.
}

}  // namespace system
}  // namespace power_manager
