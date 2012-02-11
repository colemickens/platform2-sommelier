// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/supplicant_bss_proxy.h"

#include <map>
#include <string>

#include <dbus-c++/dbus.h>

#include "shill/wifi_endpoint.h"

using std::map;
using std::string;

namespace shill {

SupplicantBSSProxy::SupplicantBSSProxy(
    WiFiEndpoint *wifi_endpoint,
    DBus::Connection *bus,
    const ::DBus::Path &object_path,
    const char *dbus_addr)
    : proxy_(wifi_endpoint, bus, object_path, dbus_addr) {}

SupplicantBSSProxy::~SupplicantBSSProxy() {}

// definitions for private class SupplicantBSSProxy::Proxy

SupplicantBSSProxy::Proxy::Proxy(
    WiFiEndpoint *wifi_endpoint, DBus::Connection *bus,
    const DBus::Path &dbus_path, const char *dbus_addr)
    : DBus::ObjectProxy(*bus, dbus_path, dbus_addr),
      wifi_endpoint_(wifi_endpoint) {}

SupplicantBSSProxy::Proxy::~Proxy() {}

void SupplicantBSSProxy::Proxy::PropertiesChanged(
    const std::map<string, ::DBus::Variant> &properties) {
  wifi_endpoint_->PropertiesChanged(properties);
}

}  // namespace shill
