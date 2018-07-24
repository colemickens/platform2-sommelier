// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/profile_dbus_adaptor.h"

#include <map>
#include <string>
#include <vector>

#include <dbus-c++/dbus.h>

#include "shill/error.h"
#include "shill/logging.h"
#include "shill/profile.h"
#include "shill/service.h"

using std::map;
using std::string;
using std::vector;

namespace shill {

namespace Logging {
static auto kModuleLogScope = ScopeLogger::kDBus;
static string ObjectID(ProfileDBusAdaptor* p) { return p->GetRpcIdentifier(); }
}


// static
const char ProfileDBusAdaptor::kPath[] = "/profile/";

ProfileDBusAdaptor::ProfileDBusAdaptor(DBus::Connection* conn, Profile* profile)
    : DBusAdaptor(conn, kPath + profile->GetFriendlyName()),
      profile_(profile) {
}

ProfileDBusAdaptor::~ProfileDBusAdaptor() {
  profile_ = nullptr;
}

void ProfileDBusAdaptor::EmitBoolChanged(const string& name, bool value) {
  SLOG(this, 2) << __func__ << ": " << name;
  PropertyChanged(name, DBusAdaptor::BoolToVariant(value));
}

void ProfileDBusAdaptor::EmitUintChanged(const string& name,
                                         uint32_t value) {
  SLOG(this, 2) << __func__ << ": " << name;
  PropertyChanged(name, DBusAdaptor::Uint32ToVariant(value));
}

void ProfileDBusAdaptor::EmitIntChanged(const string& name, int value) {
  SLOG(this, 2) << __func__ << ": " << name;
  PropertyChanged(name, DBusAdaptor::Int32ToVariant(value));
}

void ProfileDBusAdaptor::EmitStringChanged(const string& name,
                                           const string& value) {
  SLOG(this, 2) << __func__ << ": " << name;
  PropertyChanged(name, DBusAdaptor::StringToVariant(value));
}

map<string, DBus::Variant> ProfileDBusAdaptor::GetProperties(
    DBus::Error& error) {  // NOLINT
  SLOG(this, 2) << __func__;
  map<string, DBus::Variant> properties;
  DBusAdaptor::GetProperties(profile_->store(), &properties, &error);
  return properties;
}

void ProfileDBusAdaptor::SetProperty(const string& name,
                                     const DBus::Variant& value,
                                     DBus::Error& error) {  // NOLINT
  SLOG(this, 2) << __func__ << ": " << name;
  if (DBusAdaptor::SetProperty(profile_->mutable_store(),
                               name,
                               value,
                               &error)) {
    PropertyChanged(name, value);
  }
}

map<string, DBus::Variant> ProfileDBusAdaptor::GetEntry(
    const std::string& name,
    DBus::Error& error) {  // NOLINT
  SLOG(this, 2) << __func__ << ": " << name;
  Error e;
  ServiceRefPtr service = profile_->GetServiceFromEntry(name, &e);
  map<string, DBus::Variant> properties;
  if (e.IsSuccess()) {
    DBusAdaptor::GetProperties(service->store(), &properties, &error);
  } else {
    e.ToDBusError(&error);
  }
  return properties;
}

void ProfileDBusAdaptor::DeleteEntry(const std::string& name,
                                     DBus::Error& error) {  // NOLINT
  SLOG(this, 2) << __func__ << ": " << name;
  Error e;
  profile_->DeleteEntry(name, &e);
  e.ToDBusError(&error);
}

}  // namespace shill
