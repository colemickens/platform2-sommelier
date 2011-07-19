// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/ipconfig_dbus_adaptor.h"

#include <map>
#include <string>
#include <vector>

#include <base/logging.h>
#include <dbus-c++/dbus.h>

#include "shill/error.h"
#include "shill/ipconfig.h"

using std::map;
using std::string;
using std::vector;

namespace shill {

// static
const char IPConfigDBusAdaptor::kInterfaceName[] = SHILL_INTERFACE;
// static
const char IPConfigDBusAdaptor::kPath[] = "/device/";

IPConfigDBusAdaptor::IPConfigDBusAdaptor(DBus::Connection* conn,
                                         IPConfig *config)
    : DBusAdaptor(conn, kPath + config->device_name() + "_ipconfig"),
      ipconfig_(config) {
}

IPConfigDBusAdaptor::~IPConfigDBusAdaptor() {
  ipconfig_ = NULL;
}

void IPConfigDBusAdaptor::EmitBoolChanged(const string &name, bool value) {
  PropertyChanged(name, DBusAdaptor::BoolToVariant(value));
}

void IPConfigDBusAdaptor::EmitUintChanged(const string &name,
                                          uint32 value) {
  PropertyChanged(name, DBusAdaptor::Uint32ToVariant(value));
}

void IPConfigDBusAdaptor::EmitIntChanged(const string &name, int value) {
  PropertyChanged(name, DBusAdaptor::Int32ToVariant(value));
}

void IPConfigDBusAdaptor::EmitStringChanged(const string &name,
                                            const string &value) {
  PropertyChanged(name, DBusAdaptor::StringToVariant(value));
}

map<string, ::DBus::Variant> IPConfigDBusAdaptor::GetProperties(
    ::DBus::Error &error) {
  map<string, ::DBus::Variant> properties;
  DBusAdaptor::GetProperties(ipconfig_->store(), &properties, &error);
  return properties;
}

void IPConfigDBusAdaptor::SetProperty(const string &name,
                                      const ::DBus::Variant &value,
                                      ::DBus::Error &error) {
  if (DBusAdaptor::DispatchOnType(ipconfig_->store(), name, value, &error)) {
    PropertyChanged(name, value);
  }
}

void IPConfigDBusAdaptor::ClearProperty(const std::string& name,
                                        ::DBus::Error &error) {
}

void IPConfigDBusAdaptor::Remove(::DBus::Error &error) {
}

void IPConfigDBusAdaptor::MoveBefore(const ::DBus::Path& path,
                                     ::DBus::Error &error) {
}

void IPConfigDBusAdaptor::MoveAfter(const ::DBus::Path& path,
                                    ::DBus::Error &error) {
}

}  // namespace shill
