// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/supplicant/supplicant_network_proxy.h"

#include <map>
#include <string>

#include <dbus-c++/dbus.h>

#include "shill/logging.h"

using std::map;
using std::string;

namespace shill {

namespace Logging {
static auto kModuleLogScope = ScopeLogger::kDBus;
static string ObjectID(const DBus::Path* p) { return *p; }
}

SupplicantNetworkProxy::SupplicantNetworkProxy(
    DBus::Connection* bus,
    const ::DBus::Path& object_path,
    const char* dbus_addr)
    : proxy_(bus, object_path, dbus_addr) {}

SupplicantNetworkProxy::~SupplicantNetworkProxy() {}

bool SupplicantNetworkProxy::SetEnabled(bool enabled) {
  SLOG(&proxy_.path(), 2) << __func__;
  try {
    proxy_.Enabled(enabled);
  } catch (const DBus::Error& e) {
    LOG(ERROR) << "DBus exception: " << e.name() << ": " << e.what()
               << "enabled: " << enabled;
    return false;
  }
  return true;
}

// definitions for private class SupplicantNetworkProxy::Proxy

SupplicantNetworkProxy::Proxy::Proxy(
    DBus::Connection* bus, const DBus::Path& dbus_path, const char* dbus_addr)
    : DBus::ObjectProxy(*bus, dbus_path, dbus_addr) {}

SupplicantNetworkProxy::Proxy::~Proxy() {}

void SupplicantNetworkProxy::Proxy::PropertiesChanged(
    const map<string, ::DBus::Variant>& properties) {
  SLOG(&path(), 2) << __func__;
  // TODO(pstew): Some day we could notify someone about this state change.
}

}  // namespace shill
