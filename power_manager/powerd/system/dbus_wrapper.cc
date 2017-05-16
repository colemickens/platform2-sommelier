// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/system/dbus_wrapper.h"

#include <memory>

#include <base/bind.h>
#include <base/logging.h>
#include <base/memory/ptr_util.h>
#include <chromeos/dbus/service_constants.h>
#include <dbus/bus.h>
#include <dbus/message.h>
#include <google/protobuf/message_lite.h>

namespace power_manager {
namespace system {
namespace {

// Handles the result of an attempt to connect to a D-Bus signal, logging an
// error on failure.
void HandleSignalConnected(const std::string& interface,
                           const std::string& signal,
                           bool success) {
  if (!success)
    LOG(ERROR) << "Failed to connect to signal " << interface << "." << signal;
}

}  // namespace

DBusWrapper::DBusWrapper(scoped_refptr<dbus::Bus> bus,
                         dbus::ExportedObject* exported_object)
  : bus_(bus), exported_object_(exported_object) {}

DBusWrapper::~DBusWrapper() {}

std::unique_ptr<DBusWrapper> DBusWrapper::Create() {
  dbus::Bus::Options options;
  options.bus_type = dbus::Bus::SYSTEM;
  scoped_refptr<dbus::Bus> bus(new dbus::Bus(options));
  if (!bus->Connect()) {
    LOG(ERROR) << "Failed to connect to system bus";
    return nullptr;
  }
  dbus::ExportedObject* exported_object =
      bus->GetExportedObject(dbus::ObjectPath(kPowerManagerServicePath));
  if (!exported_object) {
    LOG(ERROR) << "Failed to export " << kPowerManagerServicePath << " object";
    return nullptr;
  }
  return base::WrapUnique(new DBusWrapper(bus, exported_object));
}

dbus::Bus* DBusWrapper::GetBus() {
  return bus_.get();
}

dbus::ObjectProxy* DBusWrapper::GetObjectProxy(const std::string& service_name,
                                               const std::string& object_path) {
  return bus_->GetObjectProxy(service_name, dbus::ObjectPath(object_path));
}

void DBusWrapper::RegisterForServiceAvailability(
    dbus::ObjectProxy* proxy,
    dbus::ObjectProxy::WaitForServiceToBeAvailableCallback callback) {
  DCHECK(proxy);
  proxy->WaitForServiceToBeAvailable(callback);
}

void DBusWrapper::RegisterForSignal(
    dbus::ObjectProxy* proxy,
    const std::string& interface_name,
    const std::string& signal_name,
    dbus::ObjectProxy::SignalCallback callback) {
  DCHECK(proxy);
  proxy->ConnectToSignal(interface_name,
                         signal_name,
                         callback,
                         base::Bind(&HandleSignalConnected));
}

void DBusWrapper::ExportMethod(
    const std::string& method_name,
    dbus::ExportedObject::MethodCallCallback callback) {
  CHECK(exported_object_->ExportMethodAndBlock(
      kPowerManagerInterface, method_name, callback));
}

bool DBusWrapper::PublishService() {
  return bus_->RequestOwnershipAndBlock(kPowerManagerServiceName,
                                        dbus::Bus::REQUIRE_PRIMARY);
}

void DBusWrapper::EmitSignal(dbus::Signal* signal) {
  DCHECK(exported_object_);
  DCHECK(signal);
  exported_object_->SendSignal(signal);
}

void DBusWrapper::EmitBareSignal(const std::string& signal_name) {
  dbus::Signal signal(kPowerManagerInterface, signal_name);
  EmitSignal(&signal);
}

void DBusWrapper::EmitSignalWithProtocolBuffer(
    const std::string& signal_name,
    const google::protobuf::MessageLite& protobuf) {
  dbus::Signal signal(kPowerManagerInterface, signal_name);
  dbus::MessageWriter writer(&signal);
  writer.AppendProtoAsArrayOfBytes(protobuf);
  EmitSignal(&signal);
}

std::unique_ptr<dbus::Response> DBusWrapper::CallMethodSync(
    dbus::ObjectProxy* proxy,
    dbus::MethodCall* method_call,
    base::TimeDelta timeout) {
  DCHECK(proxy);
  DCHECK(method_call);
  return std::unique_ptr<dbus::Response>(
      proxy->CallMethodAndBlock(method_call, timeout.InMilliseconds()));
}

void DBusWrapper::CallMethodAsync(
    dbus::ObjectProxy* proxy,
    dbus::MethodCall* method_call,
    base::TimeDelta timeout,
    dbus::ObjectProxy::ResponseCallback callback) {
  DCHECK(proxy);
  DCHECK(method_call);
  proxy->CallMethod(method_call, timeout.InMilliseconds(), callback);
}

}  // namespace system
}  // namespace power_manager
