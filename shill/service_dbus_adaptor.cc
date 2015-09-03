//
// Copyright (C) 2012 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include "shill/service_dbus_adaptor.h"

#include <map>
#include <string>

#include "shill/error.h"
#include "shill/logging.h"
#include "shill/service.h"

using std::map;
using std::string;
using std::vector;

namespace shill {

namespace Logging {
static auto kModuleLogScope = ScopeLogger::kDBus;
static string ObjectID(ServiceDBusAdaptor* s) { return s->GetRpcIdentifier(); }
}

// static
const char ServiceDBusAdaptor::kPath[] = "/service/";

ServiceDBusAdaptor::ServiceDBusAdaptor(DBus::Connection* conn, Service* service)
    : DBusAdaptor(conn, kPath + service->unique_name()), service_(service) {}

ServiceDBusAdaptor::~ServiceDBusAdaptor() {
  service_ = nullptr;
}

void ServiceDBusAdaptor::EmitBoolChanged(const string& name, bool value) {
  SLOG(this, 2) << __func__ << ": Service " << service_->unique_name()
                << " " << name;
  PropertyChanged(name, DBusAdaptor::BoolToVariant(value));
}

void ServiceDBusAdaptor::EmitUint8Changed(const string& name, uint8_t value) {
  SLOG(this, 2) << __func__ << ": Service " << service_->unique_name()
                << " " << name;
  PropertyChanged(name, DBusAdaptor::ByteToVariant(value));
}

void ServiceDBusAdaptor::EmitUint16Changed(const string& name, uint16_t value) {
  SLOG(this, 2) << __func__ << ": Service " << service_->unique_name()
                << " " << name;
  PropertyChanged(name, DBusAdaptor::Uint16ToVariant(value));
}

void ServiceDBusAdaptor::EmitUint16sChanged(const string& name,
                                            const Uint16s& value) {
  SLOG(this, 2) << __func__ << ": Service " << service_->unique_name()
                << " " << name;
  PropertyChanged(name, DBusAdaptor::Uint16sToVariant(value));
}

void ServiceDBusAdaptor::EmitUintChanged(const string& name, uint32_t value) {
  SLOG(this, 2) << __func__ << ": Service " << service_->unique_name()
                << " " << name;
  PropertyChanged(name, DBusAdaptor::Uint32ToVariant(value));
}

void ServiceDBusAdaptor::EmitIntChanged(const string& name, int value) {
  SLOG(this, 2) << __func__ << ": Service " << service_->unique_name()
                << " " << name;
  PropertyChanged(name, DBusAdaptor::Int32ToVariant(value));
}

void ServiceDBusAdaptor::EmitRpcIdentifierChanged(const string& name,
                                                  const string& value) {
  SLOG(this, 2) << __func__ << ": Service " << service_->unique_name()
                << " " << name;
  PropertyChanged(name, DBusAdaptor::PathToVariant(value));
}

void ServiceDBusAdaptor::EmitStringChanged(const string& name,
                                           const string& value) {
  SLOG(this, 2) << __func__ << ": Service " << service_->unique_name()
                << " " << name;
  PropertyChanged(name, DBusAdaptor::StringToVariant(value));
}

void ServiceDBusAdaptor::EmitStringmapChanged(const string& name,
                                              const Stringmap& value) {
  SLOG(this, 2) << __func__ << ": Service " << service_->unique_name()
                << " " << name;
  PropertyChanged(name, DBusAdaptor::StringmapToVariant(value));
}

map<string, DBus::Variant> ServiceDBusAdaptor::GetProperties(
    DBus::Error& error) {  // NOLINT
  SLOG(this, 2) << __func__ << ": Service " << service_->unique_name();
  map<string, DBus::Variant> properties;
  DBusAdaptor::GetProperties(service_->store(), &properties, &error);
  return properties;
}

void ServiceDBusAdaptor::SetProperty(const string& name,
                                     const DBus::Variant& value,
                                     DBus::Error& error) {  // NOLINT
  SLOG(this, 2) << __func__ << ": Service " << service_->unique_name()
                << " " << name;
  DBusAdaptor::SetProperty(service_->mutable_store(), name, value, &error);
}

void ServiceDBusAdaptor::SetProperties(const map<string, DBus::Variant>& args,
                                       DBus::Error& error) {  // NOLINT
  SLOG(this, 2) << __func__ << ": Service " << service_->unique_name();
  KeyValueStore args_store;
  Error key_value_store_error;
  DBusAdaptor::ArgsToKeyValueStore(args, &args_store, &key_value_store_error);
  if (key_value_store_error.ToDBusError(&error)) {
    return;
  }
  Error configure_error;
  service_->Configure(args_store, &configure_error);
  configure_error.ToDBusError(&error);
}

void ServiceDBusAdaptor::ClearProperty(const string& name,
                                       DBus::Error& error) {  // NOLINT
  SLOG(this, 2) << __func__ << ": Service " << service_->unique_name()
                << " " << name;
  DBusAdaptor::ClearProperty(service_->mutable_store(), name, &error);
  if (!error.is_set()) {
    service_->OnPropertyChanged(name);
  }
}

vector<bool> ServiceDBusAdaptor::ClearProperties(
    const vector<string>& names, DBus::Error& /*error*/) {  // NOLINT
  SLOG(this, 2) << __func__ << ": Service " << service_->unique_name();
  vector<bool> results;
  vector<string>::const_iterator it;
  for (it = names.begin(); it != names.end(); ++it) {
    DBus::Error error;
    ClearProperty(*it, error);
    results.push_back(!error.is_set());
  }
  return results;
}

void ServiceDBusAdaptor::Connect(DBus::Error& error) {  // NOLINT
  SLOG(this, 2) << __func__ << ": Service " << service_->unique_name();
  Error e;
  service_->UserInitiatedConnect(&e);
  e.ToDBusError(&error);
}

void ServiceDBusAdaptor::Disconnect(DBus::Error& error) {  // NOLINT
  SLOG(this, 2) << __func__ << ": Service " << service_->unique_name();
  Error e;
  service_->UserInitiatedDisconnect(&e);
  e.ToDBusError(&error);
}

void ServiceDBusAdaptor::Remove(DBus::Error& error) {  // NOLINT
  SLOG(this, 2) << __func__ << ": Service " << service_->unique_name();
  Error e;
  service_->Remove(&e);
  e.ToDBusError(&error);
}

void ServiceDBusAdaptor::ActivateCellularModem(const string& carrier,
                                               DBus::Error& error) {  // NOLINT
  SLOG(this, 2) << __func__ << ": Service " << service_->unique_name();
  Error e(Error::kOperationInitiated);
  DBus::Tag* tag = new DBus::Tag();
  service_->ActivateCellularModem(carrier, &e, GetMethodReplyCallback(tag));
  ReturnResultOrDefer(tag, e, &error);
}

void ServiceDBusAdaptor::CompleteCellularActivation(
    DBus::Error& error) {  // NOLINT
  SLOG(this, 2) << __func__ << ": Service " << service_->unique_name();
  Error e;
  service_->CompleteCellularActivation(&e);
  e.ToDBusError(&error);
}

map<DBus::Path, string> ServiceDBusAdaptor::GetLoadableProfileEntries(
    DBus::Error& error) {  // NOLINT
  SLOG(this, 2) << __func__ << ": Service " << service_->unique_name();
  map<string, string> profile_entry_strings =
      service_->GetLoadableProfileEntries();
  map<DBus::Path, string> profile_entries;
  for (const auto& entry : profile_entry_strings) {
    profile_entries[DBus::Path(entry.first)] = entry.second;
  }
  return profile_entries;
}

}  // namespace shill
