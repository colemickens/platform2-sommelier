// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/dbus/chromeos_device_dbus_adaptor.h"

#include <utility>

#include "shill/device.h"
#include "shill/error.h"
#include "shill/logging.h"

using std::string;
using std::vector;

namespace shill {

namespace Logging {
static auto kModuleLogScope = ScopeLogger::kDBus;
static string ObjectID(ChromeosDeviceDBusAdaptor* d) {
  return d->GetRpcIdentifier().value() + " (" + d->device()->UniqueName() + ")";
}
}  // namespace Logging

// static
const char ChromeosDeviceDBusAdaptor::kPath[] = "/device/";

ChromeosDeviceDBusAdaptor::ChromeosDeviceDBusAdaptor(
    const scoped_refptr<dbus::Bus>& bus,
    Device* device)
    : org::chromium::flimflam::DeviceAdaptor(this),
      ChromeosDBusAdaptor(bus,
                          kPath + SanitizePathElement(device->UniqueName())),
      device_(device) {
  // Register DBus object.
  RegisterWithDBusObject(dbus_object());
  dbus_object()->RegisterAndBlock();
}

ChromeosDeviceDBusAdaptor::~ChromeosDeviceDBusAdaptor() {
  dbus_object()->UnregisterAsync();
  device_ = nullptr;
}

const RpcIdentifier& ChromeosDeviceDBusAdaptor::GetRpcIdentifier() const {
  return dbus_path();
}

void ChromeosDeviceDBusAdaptor::EmitBoolChanged(const string& name,
                                                bool value) {
  SLOG(this, 2) << __func__ << ": " << name;
  SendPropertyChangedSignal(name, brillo::Any(value));
}

void ChromeosDeviceDBusAdaptor::EmitUintChanged(const string& name,
                                                uint32_t value) {
  SLOG(this, 2) << __func__ << ": " << name;
  SendPropertyChangedSignal(name, brillo::Any(value));
}

void ChromeosDeviceDBusAdaptor::EmitUint16Changed(const string& name,
                                                  uint16_t value) {
  SLOG(this, 2) << __func__ << ": " << name;
  SendPropertyChangedSignal(name, brillo::Any(value));
}

void ChromeosDeviceDBusAdaptor::EmitIntChanged(const string& name, int value) {
  SLOG(this, 2) << __func__ << ": " << name;
  SendPropertyChangedSignal(name, brillo::Any(value));
}

void ChromeosDeviceDBusAdaptor::EmitStringChanged(const string& name,
                                                  const string& value) {
  SLOG(this, 2) << __func__ << ": " << name;
  SendPropertyChangedSignal(name, brillo::Any(value));
}

void ChromeosDeviceDBusAdaptor::EmitStringmapChanged(const string& name,
                                                     const Stringmap& value) {
  SLOG(this, 2) << __func__ << ": " << name;
  SendPropertyChangedSignal(name, brillo::Any(value));
}

void ChromeosDeviceDBusAdaptor::EmitStringmapsChanged(const string& name,
                                                      const Stringmaps& value) {
  SLOG(this, 2) << __func__ << ": " << name;
  SendPropertyChangedSignal(name, brillo::Any(value));
}

void ChromeosDeviceDBusAdaptor::EmitStringsChanged(const string& name,
                                                   const Strings& value) {
  SLOG(this, 2) << __func__ << ": " << name;
  SendPropertyChangedSignal(name, brillo::Any(value));
}

void ChromeosDeviceDBusAdaptor::EmitKeyValueStoreChanged(
    const string& name, const KeyValueStore& value) {
  SLOG(this, 2) << __func__ << ": " << name;
  brillo::VariantDictionary dict =
      KeyValueStore::ConvertToVariantDictionary(value);
  SendPropertyChangedSignal(name, brillo::Any(dict));
}

void ChromeosDeviceDBusAdaptor::EmitRpcIdentifierChanged(
    const std::string& name, const RpcIdentifier& value) {
  SLOG(this, 2) << __func__ << ": " << name;
  SendPropertyChangedSignal(name, brillo::Any(value));
}

void ChromeosDeviceDBusAdaptor::EmitRpcIdentifierArrayChanged(
    const string& name, const RpcIdentifiers& value) {
  SLOG(this, 2) << __func__ << ": " << name;
  SendPropertyChangedSignal(name, brillo::Any(value));
}

bool ChromeosDeviceDBusAdaptor::GetProperties(
    brillo::ErrorPtr* error, brillo::VariantDictionary* out_properties) {
  SLOG(this, 2) << __func__;
  return ChromeosDBusAdaptor::GetProperties(device_->store(),
                                            out_properties,
                                            error);
}

bool ChromeosDeviceDBusAdaptor::SetProperty(brillo::ErrorPtr* error,
                                            const string& name,
                                            const brillo::Any& value) {
  SLOG(this, 2) << __func__ << ": " << name;
  return ChromeosDBusAdaptor::SetProperty(device_->mutable_store(),
                                          name,
                                          value,
                                          error);
}

bool ChromeosDeviceDBusAdaptor::ClearProperty(brillo::ErrorPtr* error,
                                              const string& name) {
  SLOG(this, 2) << __func__ << ": " << name;
  return ChromeosDBusAdaptor::ClearProperty(device_->mutable_store(),
                                            name,
                                            error);
}

void ChromeosDeviceDBusAdaptor::Enable(DBusMethodResponsePtr<> response) {
  SLOG(this, 2) << __func__;
  Error e(Error::kOperationInitiated);
  ResultCallback callback = GetMethodReplyCallback(std::move(response));
  device_->SetEnabledPersistent(true, &e, callback);
  ReturnResultOrDefer(callback, e);
}

void ChromeosDeviceDBusAdaptor::Disable(DBusMethodResponsePtr<> response) {
  SLOG(this, 2) << __func__ << ": Device " << device_->UniqueName();
  Error e(Error::kOperationInitiated);
  ResultCallback callback = GetMethodReplyCallback(std::move(response));
  device_->SetEnabledPersistent(false, &e, callback);
  ReturnResultOrDefer(callback, e);
}

void ChromeosDeviceDBusAdaptor::Register(DBusMethodResponsePtr<> response,
                                         const string& network_id) {
  SLOG(this, 2) << __func__ << ": " << network_id;
  Error e(Error::kOperationInitiated);
  ResultCallback callback = GetMethodReplyCallback(std::move(response));
  device_->RegisterOnNetwork(network_id, &e, callback);
  ReturnResultOrDefer(callback, e);
}

void ChromeosDeviceDBusAdaptor::RequirePin(
    DBusMethodResponsePtr<> response, const string& pin, bool require) {
  SLOG(this, 2) << __func__;

  Error e(Error::kOperationInitiated);
  ResultCallback callback = GetMethodReplyCallback(std::move(response));
  device_->RequirePIN(pin, require, &e, callback);
  ReturnResultOrDefer(callback, e);
}

void ChromeosDeviceDBusAdaptor::EnterPin(DBusMethodResponsePtr<> response,
                                         const string& pin) {
  SLOG(this, 2) << __func__;

  Error e(Error::kOperationInitiated);
  ResultCallback callback = GetMethodReplyCallback(std::move(response));
  device_->EnterPIN(pin, &e, callback);
  ReturnResultOrDefer(callback, e);
}

void ChromeosDeviceDBusAdaptor::UnblockPin(DBusMethodResponsePtr<> response,
                                           const string& unblock_code,
                                           const string& pin) {
  SLOG(this, 2) << __func__;

  Error e(Error::kOperationInitiated);
  ResultCallback callback = GetMethodReplyCallback(std::move(response));
  device_->UnblockPIN(unblock_code, pin, &e, callback);
  ReturnResultOrDefer(callback, e);
}

void ChromeosDeviceDBusAdaptor::ChangePin(DBusMethodResponsePtr<> response,
                                          const string& old_pin,
                                          const string& new_pin) {
  SLOG(this, 2) << __func__;

  Error e(Error::kOperationInitiated);
  ResultCallback callback = GetMethodReplyCallback(std::move(response));
  device_->ChangePIN(old_pin, new_pin, &e, callback);
  ReturnResultOrDefer(callback, e);
}

void ChromeosDeviceDBusAdaptor::Reset(DBusMethodResponsePtr<> response) {
  SLOG(this, 2) << __func__;

  Error e(Error::kOperationInitiated);
  ResultCallback callback = GetMethodReplyCallback(std::move(response));
  device_->Reset(&e, callback);
  ReturnResultOrDefer(callback, e);
}

bool ChromeosDeviceDBusAdaptor::PerformTDLSOperation(brillo::ErrorPtr* error,
                                                     const string& operation,
                                                     const string& peer,
                                                     string* out_state) {
  SLOG(this, 2) << __func__;

  Error e;
  *out_state = device_->PerformTDLSOperation(operation, peer, &e);
  return !e.ToChromeosError(error);
}

bool ChromeosDeviceDBusAdaptor::ResetByteCounters(brillo::ErrorPtr* error) {
  device_->ResetByteCounters();
  return true;
}

bool ChromeosDeviceDBusAdaptor::RequestRoam(brillo::ErrorPtr* error,
                                            const std::string& addr) {
  SLOG(this, 2) << __func__ << ": " << addr;
  Error e;
  device_->RequestRoam(addr, &e);
  return !e.ToChromeosError(error);
}

bool ChromeosDeviceDBusAdaptor::AddWakeOnPacketConnection(
    brillo::ErrorPtr* error, const string& ip_endpoint) {
  SLOG(this, 2) << __func__;

  Error e;
  device_->AddWakeOnPacketConnection(ip_endpoint, &e);
  return !e.ToChromeosError(error);
}

bool ChromeosDeviceDBusAdaptor::AddWakeOnPacketOfTypes(
    brillo::ErrorPtr* error, const std::vector<std::string>& packet_types) {
  SLOG(this, 2) << __func__;

  Error e;
  device_->AddWakeOnPacketOfTypes(packet_types, &e);
  return !e.ToChromeosError(error);
}

bool ChromeosDeviceDBusAdaptor::RemoveWakeOnPacketConnection(
    brillo::ErrorPtr* error, const string& ip_endpoint) {
  SLOG(this, 2) << __func__;

  Error e;
  device_->RemoveWakeOnPacketConnection(ip_endpoint, &e);
  return !e.ToChromeosError(error);
}

bool ChromeosDeviceDBusAdaptor::RemoveWakeOnPacketOfTypes(
    brillo::ErrorPtr* error, const std::vector<std::string>& packet_types) {
  SLOG(this, 2) << __func__;

  Error e;
  device_->RemoveWakeOnPacketOfTypes(packet_types, &e);
  return !e.ToChromeosError(error);
}

bool ChromeosDeviceDBusAdaptor::RemoveAllWakeOnPacketConnections(
    brillo::ErrorPtr* error) {
  SLOG(this, 2) << __func__;

  Error e;
  device_->RemoveAllWakeOnPacketConnections(&e);
  return !e.ToChromeosError(error);
}

}  // namespace shill
