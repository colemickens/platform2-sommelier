// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/supplicant_process_proxy.h"

#include <map>
#include <string>

#include <base/logging.h>
#include <dbus-c++/dbus.h>

#include "shill/dbus_properties.h"
#include "shill/scope_logger.h"

using std::map;
using std::string;

namespace shill {

SupplicantProcessProxy::SupplicantProcessProxy(DBus::Connection *bus,
                                               const char *dbus_path,
                                               const char *dbus_addr)
    : proxy_(bus, dbus_path, dbus_addr) {}

SupplicantProcessProxy::~SupplicantProcessProxy() {}

::DBus::Path SupplicantProcessProxy::CreateInterface(
    const map<string, ::DBus::Variant> &args) {
  SLOG(DBus, 2) << __func__;
  try {
    return proxy_.CreateInterface(args);
  } catch (const DBus::Error &e) {
    LOG(FATAL) << "DBus exception: " << e.name() << ": " << e.what()
               << " args keys are: " << DBusProperties::KeysToString(args);
    return ::DBus::Path();  // Make the compiler happy.
  }
}

void SupplicantProcessProxy::RemoveInterface(const ::DBus::Path &path) {
  SLOG(DBus, 2) << __func__;
  try {
    return proxy_.RemoveInterface(path);
  } catch (const DBus::Error &e) {
    LOG(FATAL) << "DBus exception: " << e.name() << ": " << e.what();
  }
}

::DBus::Path SupplicantProcessProxy::GetInterface(const string &ifname) {
  SLOG(DBus, 2) << __func__;
  try {
    return proxy_.GetInterface(ifname);
  } catch (const DBus::Error &e) {
    LOG(FATAL) << "DBus exception: " << e.name() << ": " << e.what()
               << " ifname: " << ifname;
    return ::DBus::Path();  // Make the compiler happy.
  }
}

// definitions for private class SupplicantProcessProxy::Proxy

SupplicantProcessProxy::Proxy::Proxy(
    DBus::Connection *bus, const char *dbus_path, const char *dbus_addr)
    : DBus::ObjectProxy(*bus, dbus_path, dbus_addr) {}

SupplicantProcessProxy::Proxy::~Proxy() {}

void SupplicantProcessProxy::Proxy::InterfaceAdded(
    const ::DBus::Path& /*path*/,
    const map<string, ::DBus::Variant> &/*properties*/) {
  SLOG(DBus, 2) << __func__;
  // XXX
}

void SupplicantProcessProxy::Proxy::InterfaceRemoved(
    const ::DBus::Path& /*path*/) {
  SLOG(DBus, 2) << __func__;
  // XXX
}

void SupplicantProcessProxy::Proxy::PropertiesChanged(
    const map<string, ::DBus::Variant>& /*properties*/) {
  SLOG(DBus, 2) << __func__;
  // XXX
}

}  // namespace shill
