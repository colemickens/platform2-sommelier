// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/dbus/chromeos_service_dbus_adaptor.h"

#include <map>
#include <string>
#include <utility>

#include "shill/error.h"
#include "shill/logging.h"
#include "shill/service.h"

using std::map;
using std::string;
using std::vector;

namespace {
const char kDBusRpcReasonString[] = "D-Bus RPC";
}  // namespace

namespace shill {

namespace Logging {
static auto kModuleLogScope = ScopeLogger::kDBus;
static string ObjectID(ChromeosServiceDBusAdaptor* s) {
  return s->GetRpcIdentifier() + " (" + s->service()->unique_name() + ")";
}
}

// static
const char ChromeosServiceDBusAdaptor::kPath[] = "/service/";

ChromeosServiceDBusAdaptor::ChromeosServiceDBusAdaptor(
    const scoped_refptr<dbus::Bus>& bus,
    Service* service)
    : org::chromium::flimflam::ServiceAdaptor(this),
      ChromeosDBusAdaptor(bus, kPath + service->unique_name()),
      service_(service) {
  // Register DBus object.
  RegisterWithDBusObject(dbus_object());
  dbus_object()->RegisterAndBlock();
}

ChromeosServiceDBusAdaptor::~ChromeosServiceDBusAdaptor() {
  dbus_object()->UnregisterAsync();
  service_ = nullptr;
}

void ChromeosServiceDBusAdaptor::EmitBoolChanged(const string& name,
                                                 bool value) {
  SLOG(this, 2) << __func__ << ": " << name;
  SendPropertyChangedSignal(name, brillo::Any(value));
}

void ChromeosServiceDBusAdaptor::EmitUint8Changed(const string& name,
                                                  uint8_t value) {
  SLOG(this, 2) << __func__ << ": " << name;
  SendPropertyChangedSignal(name, brillo::Any(value));
}

void ChromeosServiceDBusAdaptor::EmitUint16Changed(const string& name,
                                                   uint16_t value) {
  SLOG(this, 2) << __func__ << ": " << name;
  SendPropertyChangedSignal(name, brillo::Any(value));
}

void ChromeosServiceDBusAdaptor::EmitUint16sChanged(const string& name,
                                                    const Uint16s& value) {
  SLOG(this, 2) << __func__ << ": " << name;
  SendPropertyChangedSignal(name, brillo::Any(value));
}

void ChromeosServiceDBusAdaptor::EmitUintChanged(const string& name,
                                                 uint32_t value) {
  SLOG(this, 2) << __func__ << ": " << name;
  SendPropertyChangedSignal(name, brillo::Any(value));
}

void ChromeosServiceDBusAdaptor::EmitIntChanged(const string& name, int value) {
  SLOG(this, 2) << __func__ << ": " << name;
  SendPropertyChangedSignal(name, brillo::Any(value));
}

void ChromeosServiceDBusAdaptor::EmitRpcIdentifierChanged(
    const string& name, const RpcIdentifier& value) {
  SLOG(this, 2) << __func__ << ": " << name;
  SendPropertyChangedSignal(name, brillo::Any(dbus::ObjectPath(value)));
}

void ChromeosServiceDBusAdaptor::EmitStringChanged(const string& name,
                                                   const string& value) {
  SLOG(this, 2) << __func__ << ": " << name;
  SendPropertyChangedSignal(name, brillo::Any(value));
}

void ChromeosServiceDBusAdaptor::EmitStringmapChanged(const string& name,
                                                      const Stringmap& value) {
  SLOG(this, 2) << __func__ << ": " << name;
  SendPropertyChangedSignal(name, brillo::Any(value));
}

bool ChromeosServiceDBusAdaptor::GetProperties(
    brillo::ErrorPtr* error, brillo::VariantDictionary* properties) {
  SLOG(this, 2) << __func__;
  return ChromeosDBusAdaptor::GetProperties(service_->store(),
                                            properties,
                                            error);
}

bool ChromeosServiceDBusAdaptor::SetProperty(
    brillo::ErrorPtr* error, const string& name, const brillo::Any& value) {
  SLOG(this, 2) << __func__ << ": " << name;
  return ChromeosDBusAdaptor::SetProperty(service_->mutable_store(),
                                          name,
                                          value,
                                          error);
}

bool ChromeosServiceDBusAdaptor::SetProperties(
    brillo::ErrorPtr* error, const brillo::VariantDictionary& args) {
  SLOG(this, 2) << __func__;
  KeyValueStore args_store =
      KeyValueStore::ConvertFromVariantDictionary(args);
  Error configure_error;
  service_->Configure(args_store, &configure_error);
  return !configure_error.ToChromeosError(error);
}

bool ChromeosServiceDBusAdaptor::ClearProperty(brillo::ErrorPtr* error,
                                              const string& name) {
  SLOG(this, 2) << __func__ << ": " << name;
  bool status = ChromeosDBusAdaptor::ClearProperty(service_->mutable_store(),
                                                   name,
                                                   error);
  if (status) {
    service_->OnPropertyChanged(name);
  }
  return status;
}

bool ChromeosServiceDBusAdaptor::ClearProperties(brillo::ErrorPtr* /*error*/,
                                                 const vector<string>& names,
                                                 vector<bool>* results) {
  SLOG(this, 2) << __func__;
  for (const auto& name : names) {
    results->push_back(ClearProperty(nullptr, name));
  }
  return true;
}

bool ChromeosServiceDBusAdaptor::Connect(brillo::ErrorPtr* error) {
  SLOG(this, 2) << __func__;
  Error e;
  service_->UserInitiatedConnect(kDBusRpcReasonString, &e);
  return !e.ToChromeosError(error);
}

bool ChromeosServiceDBusAdaptor::Disconnect(brillo::ErrorPtr* error) {
  SLOG(this, 2) << __func__;
  Error e;
  service_->UserInitiatedDisconnect(kDBusRpcReasonString, &e);
  return !e.ToChromeosError(error);
}

bool ChromeosServiceDBusAdaptor::Remove(brillo::ErrorPtr* error) {
  SLOG(this, 2) << __func__;
  Error e;
  service_->Remove(&e);
  return !e.ToChromeosError(error);
}

void ChromeosServiceDBusAdaptor::ActivateCellularModem(
    DBusMethodResponsePtr<> response, const string& carrier) {
  SLOG(this, 2) << __func__;
  Error e(Error::kOperationInitiated);
  ResultCallback callback = GetMethodReplyCallback(std::move(response));
  service_->ActivateCellularModem(carrier, &e, callback);
  ReturnResultOrDefer(callback, e);
}

bool ChromeosServiceDBusAdaptor::CompleteCellularActivation(
    brillo::ErrorPtr* error) {
  SLOG(this, 2) << __func__;
  Error e;
  service_->CompleteCellularActivation(&e);
  return !e.ToChromeosError(error);
}

bool ChromeosServiceDBusAdaptor::GetLoadableProfileEntries(
    brillo::ErrorPtr* /*error*/, map<dbus::ObjectPath, string>* entries) {
  SLOG(this, 2) << __func__;
  map<RpcIdentifier, string> profile_entry_strings =
      service_->GetLoadableProfileEntries();
  for (const auto& entry : profile_entry_strings) {
    (*entries)[dbus::ObjectPath(entry.first)] = entry.second;
  }
  return true;
}

}  // namespace shill
