// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/dbus_properties_proxy.h"

#include <base/logging.h>

#include "shill/modem.h"

namespace shill {

using std::map;
using std::string;
using std::vector;

DBusPropertiesProxy::DBusPropertiesProxy(DBus::Connection *connection,
                                         Modem *modem,
                                         const string &path,
                                         const string &service)
    : proxy_(connection, modem, path, service) {}

DBusPropertiesProxy::~DBusPropertiesProxy() {}

map<string, DBus::Variant> DBusPropertiesProxy::GetAll(
    const string &interface_name) {
  return proxy_.GetAll(interface_name);
}

DBusPropertiesProxy::Proxy::Proxy(DBus::Connection *connection,
                                  Modem *modem,
                                  const string &path,
                                  const string &service)
    : DBus::ObjectProxy(*connection, path, service.c_str()),
      modem_(modem) {}

DBusPropertiesProxy::Proxy::~Proxy() {}

void DBusPropertiesProxy::Proxy::MmPropertiesChanged(
    const string &interface,
    const map<string, DBus::Variant> &properties) {
  LOG(INFO) << "MmPropertiesChanged: " << interface;
}

void DBusPropertiesProxy::Proxy::PropertiesChanged(
    const string &interface,
    const map<string, DBus::Variant> &changed_properties,
    const vector<string> &invalidated_properties) {
  LOG(INFO) << "PropertiesChanged: " << interface;
}

}  // namespace shill
