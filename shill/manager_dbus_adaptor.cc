// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/manager_dbus_adaptor.h"

#include <map>
#include <string>
#include <vector>

#include <base/logging.h>
#include <dbus-c++/dbus.h>

#include "shill/error.h"
#include "shill/manager.h"

using std::map;
using std::string;
using std::vector;

namespace shill {

// static
const char ManagerDBusAdaptor::kInterfaceName[] = SHILL_INTERFACE;
// static
const char ManagerDBusAdaptor::kPath[] = SHILL_PATH "/Manager";

ManagerDBusAdaptor::ManagerDBusAdaptor(DBus::Connection* conn, Manager *manager)
    : DBusAdaptor(conn, kPath),
      manager_(manager) {
}

ManagerDBusAdaptor::~ManagerDBusAdaptor() {
  manager_ = NULL;
}

void ManagerDBusAdaptor::UpdateRunning() {}

void ManagerDBusAdaptor::EmitBoolChanged(const string &name, bool value) {
  PropertyChanged(name, DBusAdaptor::BoolToVariant(value));
}

void ManagerDBusAdaptor::EmitUintChanged(const string &name,
                                         uint32 value) {
  PropertyChanged(name, DBusAdaptor::Uint32ToVariant(value));
}

void ManagerDBusAdaptor::EmitIntChanged(const string &name, int value) {
  PropertyChanged(name, DBusAdaptor::Int32ToVariant(value));
}

void ManagerDBusAdaptor::EmitStringChanged(const string &name,
                                           const string &value) {
  PropertyChanged(name, DBusAdaptor::StringToVariant(value));
}

void ManagerDBusAdaptor::EmitStateChanged(const string &new_state) {
  StateChanged(new_state);
}

map<string, ::DBus::Variant> ManagerDBusAdaptor::GetProperties(
    ::DBus::Error &error) {
  map<string, ::DBus::Variant> properties;
  DBusAdaptor::GetProperties(manager_->store(), &properties, &error);
  return properties;
}

void ManagerDBusAdaptor::SetProperty(const string &name,
                                     const ::DBus::Variant &value,
                                     ::DBus::Error &error) {
  if (DBusAdaptor::DispatchOnType(manager_->store(), name, value, &error)) {
    PropertyChanged(name, value);
  }
}

string ManagerDBusAdaptor::GetState(::DBus::Error &error) {
  return string();
}

::DBus::Path ManagerDBusAdaptor::CreateProfile(const string &name,
                                               ::DBus::Error &error) {
  return ::DBus::Path();
}

void ManagerDBusAdaptor::RemoveProfile(const string &name,
                                       ::DBus::Error &error) {
}

::DBus::Path ManagerDBusAdaptor::PushProfile(const std::string &,
                                             ::DBus::Error &error) {
  return ::DBus::Path();
}

void ManagerDBusAdaptor::PopProfile(const std::string &, ::DBus::Error &error) {
}

void ManagerDBusAdaptor::PopAnyProfile(::DBus::Error &error) {
}

void ManagerDBusAdaptor::RequestScan(const string &,
                                     ::DBus::Error &error) {
}

void ManagerDBusAdaptor::EnableTechnology(const string &,
                                          ::DBus::Error &error) {
}

void ManagerDBusAdaptor::DisableTechnology(const string &,
                                           ::DBus::Error &error) {
}

::DBus::Path ManagerDBusAdaptor::GetService(
    const map<string, ::DBus::Variant> &,
    ::DBus::Error &error) {
  return ::DBus::Path();
}

::DBus::Path ManagerDBusAdaptor::GetWifiService(
    const map<string, ::DBus::Variant> &,
    ::DBus::Error &error) {
  return ::DBus::Path();
}

void ManagerDBusAdaptor::ConfigureWifiService(
    const map<string, ::DBus::Variant> &,
    ::DBus::Error &error) {
}

::DBus::Path ManagerDBusAdaptor::GetVPNService(
    const map<string, ::DBus::Variant> &,
    ::DBus::Error &error) {
  return ::DBus::Path();
}

void ManagerDBusAdaptor::RegisterAgent(const ::DBus::Path &,
                                       ::DBus::Error &error) {
}

void ManagerDBusAdaptor::UnregisterAgent(const ::DBus::Path &,
                                         ::DBus::Error &error) {
}

int32_t ManagerDBusAdaptor::GetDebugLevel(::DBus::Error &error) {
  return logging::GetMinLogLevel();
}

void ManagerDBusAdaptor::SetDebugLevel(const int32_t &level,
                                      ::DBus::Error &error) {
  if (level < logging::LOG_NUM_SEVERITIES)
    logging::SetMinLogLevel(level);
  else
    LOG(WARNING) << "Ignoring attempt to set log level to " << level;
}

string ManagerDBusAdaptor::GetServiceOrder(::DBus::Error &error) {
  return string();
}

void ManagerDBusAdaptor::SetServiceOrder(const string &,
                                         ::DBus::Error &error) {
}

}  // namespace shill
