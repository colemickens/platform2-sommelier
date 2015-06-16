// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/ipconfig_dbus_adaptor.h"

#include <map>
#include <string>
#include <vector>

#include <base/strings/stringprintf.h>
#include <dbus-c++/dbus.h>

#include "shill/error.h"
#include "shill/ipconfig.h"
#include "shill/logging.h"

using base::StringPrintf;
using std::map;
using std::string;
using std::vector;

namespace shill {

namespace Logging {
static auto kModuleLogScope = ScopeLogger::kDBus;
static string ObjectID(IPConfigDBusAdaptor* i) { return i->GetRpcIdentifier(); }
}

// static
const char IPConfigDBusAdaptor::kPath[] = "/ipconfig/";

IPConfigDBusAdaptor::IPConfigDBusAdaptor(DBus::Connection* conn,
                                         IPConfig* config)
    : DBusAdaptor(conn, StringPrintf("%s%s_%u_%s",
                                     kPath,
                                     SanitizePathElement(
                                         config->device_name()).c_str(),
                                     config->serial(),
                                     config->type().c_str())),
      ipconfig_(config) {
}

IPConfigDBusAdaptor::~IPConfigDBusAdaptor() {
  ipconfig_ = nullptr;
}

void IPConfigDBusAdaptor::EmitBoolChanged(const string& name, bool value) {
  SLOG(this, 2) << __func__ << ": " << name;
  PropertyChanged(name, DBusAdaptor::BoolToVariant(value));
}

void IPConfigDBusAdaptor::EmitUintChanged(const string& name,
                                          uint32_t value) {
  SLOG(this, 2) << __func__ << ": " << name;
  PropertyChanged(name, DBusAdaptor::Uint32ToVariant(value));
}

void IPConfigDBusAdaptor::EmitIntChanged(const string& name, int value) {
  SLOG(this, 2) << __func__ << ": " << name;
  PropertyChanged(name, DBusAdaptor::Int32ToVariant(value));
}

void IPConfigDBusAdaptor::EmitStringChanged(const string& name,
                                            const string& value) {
  SLOG(this, 2) << __func__ << ": " << name;
  PropertyChanged(name, DBusAdaptor::StringToVariant(value));
}

void IPConfigDBusAdaptor::EmitStringsChanged(const string& name,
                                             const vector<string>& value) {
  SLOG(this, 2) << __func__ << ": " << name;
  PropertyChanged(name, DBusAdaptor::StringsToVariant(value));
}

map<string, DBus::Variant> IPConfigDBusAdaptor::GetProperties(
    DBus::Error& error) {  // NOLINT
  SLOG(this, 2) << __func__;
  map<string, DBus::Variant> properties;
  DBusAdaptor::GetProperties(ipconfig_->store(), &properties, &error);
  return properties;
}

void IPConfigDBusAdaptor::SetProperty(const string& name,
                                      const DBus::Variant& value,
                                      DBus::Error& error) {  // NOLINT
  SLOG(this, 2) << __func__ << ": " << name;
  if (DBusAdaptor::SetProperty(ipconfig_->mutable_store(),
                               name,
                               value,
                               &error)) {
    PropertyChanged(name, value);
  }
}

void IPConfigDBusAdaptor::ClearProperty(const string& name,
                                        DBus::Error& error) {  // NOLINT
  SLOG(this, 2) << __func__ << ": " << name;
  DBusAdaptor::ClearProperty(ipconfig_->mutable_store(), name, &error);
}

void IPConfigDBusAdaptor::Remove(DBus::Error& /*error*/) {  // NOLINT
  SLOG(this, 2) << __func__;
}

void IPConfigDBusAdaptor::Refresh(DBus::Error& error) {  // NOLINT
  SLOG(this, 2) << __func__;
  Error e;
  ipconfig_->Refresh(&e);
  e.ToDBusError(&error);
}

}  // namespace shill
