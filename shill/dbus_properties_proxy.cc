// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/dbus_properties_proxy.h"

#include <base/logging.h>

namespace shill {

using std::string;
using std::vector;

DBusPropertiesProxy::DBusPropertiesProxy(DBusPropertiesProxyListener *listener,
                                         DBus::Connection *connection,
                                         const string &path,
                                         const string &service)
    : proxy_(listener, connection, path, service) {}

DBusPropertiesProxy::~DBusPropertiesProxy() {}

DBusPropertiesMap DBusPropertiesProxy::GetAll(const string &interface_name) {
  return proxy_.GetAll(interface_name);
}

DBusPropertiesProxy::Proxy::Proxy(DBusPropertiesProxyListener *listener,
                                  DBus::Connection *connection,
                                  const string &path,
                                  const string &service)
    : DBus::ObjectProxy(*connection, path, service.c_str()),
      listener_(listener) {}

DBusPropertiesProxy::Proxy::~Proxy() {}

void DBusPropertiesProxy::Proxy::MmPropertiesChanged(
    const string &interface,
    const DBusPropertiesMap &properties) {
  VLOG(2) << __func__ << "(" << interface << ")";
  listener_->OnModemManagerPropertiesChanged(interface, properties);
}

void DBusPropertiesProxy::Proxy::PropertiesChanged(
    const string &interface,
    const DBusPropertiesMap &changed_properties,
    const vector<string> &invalidated_properties) {
  VLOG(2) << __func__ << "(" << interface << ")";
  listener_->OnDBusPropertiesChanged(
      interface, changed_properties, invalidated_properties);
}

}  // namespace shill
