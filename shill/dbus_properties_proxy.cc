// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/dbus_properties_proxy.h"

#include "shill/scope_logger.h"

namespace shill {

using std::string;
using std::vector;

DBusPropertiesProxy::DBusPropertiesProxy(DBus::Connection *connection,
                                         const string &path,
                                         const string &service)
    : proxy_(connection, path, service) {}

DBusPropertiesProxy::~DBusPropertiesProxy() {}

DBusPropertiesMap DBusPropertiesProxy::GetAll(const string &interface_name) {
  SLOG(DBus, 2) << __func__ << "(" << interface_name << ")";
  return proxy_.GetAll(interface_name);
}

void DBusPropertiesProxy::set_properties_changed_callback(
    const PropertiesChangedCallback &callback) {
  proxy_.set_properties_changed_callback(callback);
}

void DBusPropertiesProxy::set_modem_manager_properties_changed_callback(
    const ModemManagerPropertiesChangedCallback &callback) {
  proxy_.set_modem_manager_properties_changed_callback(callback);
}

DBusPropertiesProxy::Proxy::Proxy(DBus::Connection *connection,
                                  const string &path,
                                  const string &service)
    : DBus::ObjectProxy(*connection, path, service.c_str()) {}

DBusPropertiesProxy::Proxy::~Proxy() {}

void DBusPropertiesProxy::Proxy::MmPropertiesChanged(
    const string &interface,
    const DBusPropertiesMap &properties) {
  SLOG(DBus, 2) << __func__ << "(" << interface << ")";
  mm_properties_changed_callback_.Run(interface, properties);
}

void DBusPropertiesProxy::Proxy::PropertiesChanged(
    const string &interface,
    const DBusPropertiesMap &changed_properties,
    const vector<string> &invalidated_properties) {
  SLOG(DBus, 2) << __func__ << "(" << interface << ")";
  properties_changed_callback_.Run(
      interface, changed_properties, invalidated_properties);
}

void DBusPropertiesProxy::Proxy::set_properties_changed_callback(
    const PropertiesChangedCallback &callback) {
  properties_changed_callback_ = callback;
}

void DBusPropertiesProxy::Proxy::set_modem_manager_properties_changed_callback(
    const ModemManagerPropertiesChangedCallback &callback) {
  mm_properties_changed_callback_ = callback;
}

}  // namespace shill
