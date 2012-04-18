// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/dbus_properties_proxy.h"

#include "shill/scope_logger.h"

namespace shill {

using std::string;
using std::vector;

DBusPropertiesProxy::DBusPropertiesProxy(DBusPropertiesProxyDelegate *delegate,
                                         DBus::Connection *connection,
                                         const string &path,
                                         const string &service)
    : proxy_(delegate, connection, path, service) {}

DBusPropertiesProxy::~DBusPropertiesProxy() {}

DBusPropertiesMap DBusPropertiesProxy::GetAll(const string &interface_name) {
  return proxy_.GetAll(interface_name);
}

DBusPropertiesProxy::Proxy::Proxy(DBusPropertiesProxyDelegate *delegate,
                                  DBus::Connection *connection,
                                  const string &path,
                                  const string &service)
    : DBus::ObjectProxy(*connection, path, service.c_str()),
      delegate_(delegate) {}

DBusPropertiesProxy::Proxy::~Proxy() {}

void DBusPropertiesProxy::Proxy::MmPropertiesChanged(
    const string &interface,
    const DBusPropertiesMap &properties) {
  SLOG(DBus, 2) << __func__ << "(" << interface << ")";
  delegate_->OnModemManagerPropertiesChanged(interface, properties);
}

void DBusPropertiesProxy::Proxy::PropertiesChanged(
    const string &interface,
    const DBusPropertiesMap &changed_properties,
    const vector<string> &invalidated_properties) {
  SLOG(DBus, 2) << __func__ << "(" << interface << ")";
  delegate_->OnDBusPropertiesChanged(
      interface, changed_properties, invalidated_properties);
}

}  // namespace shill
