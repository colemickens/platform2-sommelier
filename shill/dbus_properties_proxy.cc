// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/dbus_properties_proxy.h"

#include "shill/logging.h"

namespace shill {

using std::string;
using std::vector;

namespace Logging {
static auto kModuleLogScope = ScopeLogger::kDBus;
static string ObjectID(const DBus::Path* p) { return *p; }
}

DBusPropertiesProxy::DBusPropertiesProxy(DBus::Connection* connection,
                                         const string& path,
                                         const string& service)
    : proxy_(connection, path, service) {}

DBusPropertiesProxy::~DBusPropertiesProxy() {}

DBusPropertiesMap DBusPropertiesProxy::GetAll(const string& interface_name) {
  SLOG(&proxy_.path(), 2) << __func__ << "(" << interface_name << ")";
  try {
    return proxy_.GetAll(interface_name);
  } catch (const DBus::Error& e) {
    LOG(ERROR) << "DBus exception: " << e.name() << ": " << e.what()
               << " interface name: " << interface_name;
  }
  return DBusPropertiesMap();
}

DBus::Variant DBusPropertiesProxy::Get(const string& interface_name,
                                       const string& property) {
  SLOG(&proxy_.path(), 2) << __func__ << "(" << interface_name << ", "
                << property << ")";
  try {
    return proxy_.Get(interface_name, property);
  } catch (const DBus::Error& e) {
    LOG(ERROR) << "DBus exception: " << e.name() << ": " << e.what()
               << " interface name: " << interface_name
               << ", property: " << property;
  }
  return DBus::Variant();
}

void DBusPropertiesProxy::set_properties_changed_callback(
    const PropertiesChangedCallback& callback) {
  proxy_.set_properties_changed_callback(callback);
}

void DBusPropertiesProxy::set_modem_manager_properties_changed_callback(
    const ModemManagerPropertiesChangedCallback& callback) {
  proxy_.set_modem_manager_properties_changed_callback(callback);
}

DBusPropertiesProxy::Proxy::Proxy(DBus::Connection* connection,
                                  const string& path,
                                  const string& service)
    : DBus::ObjectProxy(*connection, path, service.c_str()) {}

DBusPropertiesProxy::Proxy::~Proxy() {}

void DBusPropertiesProxy::Proxy::MmPropertiesChanged(
    const string& interface,
    const DBusPropertiesMap& properties) {
  SLOG(&path(), 2) << __func__ << "(" << interface << ")";
  mm_properties_changed_callback_.Run(interface, properties);
}

void DBusPropertiesProxy::Proxy::PropertiesChanged(
    const string& interface,
    const DBusPropertiesMap& changed_properties,
    const vector<string>& invalidated_properties) {
  SLOG(&path(), 2) << __func__ << "(" << interface << ")";
  properties_changed_callback_.Run(
      interface, changed_properties, invalidated_properties);
}

void DBusPropertiesProxy::Proxy::set_properties_changed_callback(
    const PropertiesChangedCallback& callback) {
  properties_changed_callback_ = callback;
}

void DBusPropertiesProxy::Proxy::set_modem_manager_properties_changed_callback(
    const ModemManagerPropertiesChangedCallback& callback) {
  mm_properties_changed_callback_ = callback;
}

}  // namespace shill
