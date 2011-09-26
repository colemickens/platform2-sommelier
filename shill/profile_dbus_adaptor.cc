// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/profile_dbus_adaptor.h"

#include <map>
#include <string>
#include <vector>

#include <base/logging.h>
#include <dbus-c++/dbus.h>

#include "shill/error.h"
#include "shill/profile.h"

using std::map;
using std::string;
using std::vector;

namespace shill {

// static
const char ProfileDBusAdaptor::kInterfaceName[] = SHILL_INTERFACE;
// static
const char ProfileDBusAdaptor::kPath[] = "/profile/";

ProfileDBusAdaptor::ProfileDBusAdaptor(DBus::Connection* conn, Profile *profile)
    : DBusAdaptor(conn, kPath + profile->GetFriendlyName()),
      profile_(profile) {
}

ProfileDBusAdaptor::~ProfileDBusAdaptor() {
  profile_ = NULL;
}

void ProfileDBusAdaptor::EmitBoolChanged(const string &name, bool value) {
  PropertyChanged(name, DBusAdaptor::BoolToVariant(value));
}

void ProfileDBusAdaptor::EmitUintChanged(const string &name,
                                         uint32 value) {
  PropertyChanged(name, DBusAdaptor::Uint32ToVariant(value));
}

void ProfileDBusAdaptor::EmitIntChanged(const string &name, int value) {
  PropertyChanged(name, DBusAdaptor::Int32ToVariant(value));
}

void ProfileDBusAdaptor::EmitStringChanged(const string &name,
                                           const string &value) {
  PropertyChanged(name, DBusAdaptor::StringToVariant(value));
}

map<string, ::DBus::Variant> ProfileDBusAdaptor::GetProperties(
    ::DBus::Error &error) {
  map<string, ::DBus::Variant> properties;
  DBusAdaptor::GetProperties(profile_->store(), &properties, &error);
  return properties;
}

void ProfileDBusAdaptor::SetProperty(const string &name,
                                     const ::DBus::Variant &value,
                                     ::DBus::Error &error) {
  if (DBusAdaptor::DispatchOnType(profile_->mutable_store(),
                                  name,
                                  value,
                                  &error)) {
    PropertyChanged(name, value);
  }
}

map<string, ::DBus::Variant> ProfileDBusAdaptor::GetEntry(
    const std::string& /*name*/,
    ::DBus::Error &/*error*/) {
  return map<string, ::DBus::Variant>();
}

void ProfileDBusAdaptor::DeleteEntry(const std::string& /*name*/,
                                     ::DBus::Error &/*error*/) {
}

}  // namespace shill
