// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/manager_dbus_adaptor.h"

#include <map>
#include <string>
#include <vector>

#include <base/logging.h>
#include <dbus-c++/dbus.h>

#include "shill/device.h"
#include "shill/error.h"
#include "shill/key_value_store.h"
#include "shill/manager.h"
#include "shill/wifi_service.h"

using std::map;
using std::string;
using std::vector;

namespace shill {

// static
const char ManagerDBusAdaptor::kInterfaceName[] = SHILL_INTERFACE;
// static
const char ManagerDBusAdaptor::kPath[] = "/";

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

void ManagerDBusAdaptor::EmitStringsChanged(const string &name,
                                            const vector<string> &value) {
  PropertyChanged(name, DBusAdaptor::StringsToVariant(value));
}

void ManagerDBusAdaptor::EmitRpcIdentifierArrayChanged(
    const string &name,
    const vector<string> &value) {
  vector< ::DBus::Path> paths;
  vector<string>::const_iterator it;
  for (it = value.begin(); it != value.end(); ++it) {
    paths.push_back(*it);
  }

  PropertyChanged(name, DBusAdaptor::PathArrayToVariant(paths));
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
  if (DBusAdaptor::SetProperty(manager_->mutable_store(),
                               name,
                               value,
                               &error)) {
    PropertyChanged(name, value);
  }
}

string ManagerDBusAdaptor::GetState(::DBus::Error &/*error*/) {
  return string();
}

::DBus::Path ManagerDBusAdaptor::CreateProfile(const string &name,
                                               ::DBus::Error &error) {
  Error e;
  string path;
  manager_->CreateProfile(name, &path, &e);
  e.ToDBusError(&error);
  return ::DBus::Path(path);
}

void ManagerDBusAdaptor::RemoveProfile(const string &/*name*/,
                                       ::DBus::Error &/*error*/) {
}

::DBus::Path ManagerDBusAdaptor::PushProfile(const std::string &name,
                                             ::DBus::Error &error) {
  Error e;
  string path;
  manager_->PushProfile(name, &path, &e);
  e.ToDBusError(&error);
  return ::DBus::Path(path);
}

void ManagerDBusAdaptor::PopProfile(const std::string &name,
                                    ::DBus::Error &error) {
  Error e;
  manager_->PopProfile(name, &e);
  e.ToDBusError(&error);
}

void ManagerDBusAdaptor::PopAnyProfile(::DBus::Error &error) {
  Error e;
  manager_->PopAnyProfile(&e);
  e.ToDBusError(&error);
}

void ManagerDBusAdaptor::RecheckPortal(::DBus::Error &error) {
  Error e;
  manager_->RecheckPortal(&e);
  e.ToDBusError(&error);
}

void ManagerDBusAdaptor::RequestScan(const string &technology,
                                     ::DBus::Error &error) {
  Error e;
  manager_->RequestScan(technology, &e);
  e.ToDBusError(&error);
}

void ManagerDBusAdaptor::EnableTechnology(const string &,
                                          ::DBus::Error &/*error*/) {
}

void ManagerDBusAdaptor::DisableTechnology(const string &,
                                           ::DBus::Error &/*error*/) {
}

// Called, e.g., to get WiFiService handle for a hidden SSID.
::DBus::Path ManagerDBusAdaptor::GetService(
    const map<string, ::DBus::Variant> &args,
    ::DBus::Error &error) {
  ServiceRefPtr service;
  KeyValueStore args_store;
  Error e;
  DBusAdaptor::ArgsToKeyValueStore(args, &args_store, &e);
  if (e.IsSuccess()) {
    service = manager_->GetService(args_store, &e);
  }
  if (e.ToDBusError(&error)) {
    return "/";  // ensure return is syntactically valid
  }
  return service->GetRpcIdentifier();
}

// Obsolete, use GetService instead.
::DBus::Path ManagerDBusAdaptor::GetVPNService(
    const map<string, ::DBus::Variant> &args,
    ::DBus::Error &error) {
  return GetService(args, error);
}

// Obsolete, use GetService instead.
::DBus::Path ManagerDBusAdaptor::GetWifiService(
    const map<string, ::DBus::Variant> &args,
    ::DBus::Error &error) {
  return GetService(args, error);
}

void ManagerDBusAdaptor::ConfigureWifiService(
    const map<string, ::DBus::Variant> &,
    ::DBus::Error &/*error*/) {
}

void ManagerDBusAdaptor::RegisterAgent(const ::DBus::Path &,
                                       ::DBus::Error &/*error*/) {
}

void ManagerDBusAdaptor::UnregisterAgent(const ::DBus::Path &,
                                         ::DBus::Error &/*error*/) {
}

int32_t ManagerDBusAdaptor::GetDebugLevel(::DBus::Error &/*error*/) {
  return logging::GetMinLogLevel();
}

void ManagerDBusAdaptor::SetDebugLevel(const int32_t &level,
                                       ::DBus::Error &/*error*/) {
  if (level < logging::LOG_NUM_SEVERITIES)
    logging::SetMinLogLevel(level);
  else
    LOG(WARNING) << "Ignoring attempt to set log level to " << level;
}

string ManagerDBusAdaptor::GetServiceOrder(::DBus::Error &/*error*/) {
  return manager_->GetTechnologyOrder();
}

void ManagerDBusAdaptor::SetServiceOrder(const string &order,
                                         ::DBus::Error &error) {
  Error e;
  manager_->SetTechnologyOrder(order, &e);
  e.ToDBusError(&error);
}

std::string ManagerDBusAdaptor::GetDebugTags(::DBus::Error &/*error*/) {
  return "";
}

void ManagerDBusAdaptor::SetDebugTags(const std::string &/*tags*/,
                                      ::DBus::Error &/*error*/) {
}

std::string ManagerDBusAdaptor::ListDebugTags(::DBus::Error &/*error*/) {
  return "";
}

}  // namespace shill
