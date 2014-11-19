// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/supplicant/supplicant_process_proxy.h"

#include <map>
#include <string>

#include <dbus-c++/dbus.h>

#include "shill/dbus_properties.h"
#include "shill/logging.h"

using std::map;
using std::string;

namespace shill {

namespace Logging {
static auto kModuleLogScope = ScopeLogger::kDBus;
static string ObjectID(const DBus::Path *p) { return *p; }
}

SupplicantProcessProxy::SupplicantProcessProxy(DBus::Connection *bus,
                                               const char *dbus_path,
                                               const char *dbus_addr)
    : proxy_(bus, dbus_path, dbus_addr) {}

SupplicantProcessProxy::~SupplicantProcessProxy() {}

::DBus::Path SupplicantProcessProxy::CreateInterface(
    const map<string, ::DBus::Variant> &args) {
  SLOG(&proxy_.path(), 2) << __func__;
  try {
    return proxy_.CreateInterface(args);
  } catch (const DBus::Error &e) {
    LOG(ERROR) << "DBus exception: " << e.name() << ": " << e.what()
               << " args keys are: " << DBusProperties::KeysToString(args);
    throw;  // Re-throw the exception.
  }
}

void SupplicantProcessProxy::RemoveInterface(const ::DBus::Path &path) {
  SLOG(&proxy_.path(), 2) << __func__;
  try {
    return proxy_.RemoveInterface(path);
  } catch (const DBus::Error &e) {
    LOG(FATAL) << "DBus exception: " << e.name() << ": " << e.what();
  }
}

::DBus::Path SupplicantProcessProxy::GetInterface(const string &ifname) {
  SLOG(&proxy_.path(), 2) << __func__;
  try {
    return proxy_.GetInterface(ifname);
  } catch (const DBus::Error &e) {
    LOG(FATAL) << "DBus exception: " << e.name() << ": " << e.what()
               << " ifname: " << ifname;
    return ::DBus::Path();  // Make the compiler happy.
  }
}

string SupplicantProcessProxy::GetDebugLevel() {
  try {
    return proxy_.DebugLevel();
  } catch (const DBus::Error &e) {
    LOG(FATAL) << "DBus exception: " << e.name() << ": " << e.what();
    return "";  // Make the compiler happy.
  }
}

void SupplicantProcessProxy::SetDebugLevel(const string &level) {
  try {
    proxy_.DebugLevel(level);
  } catch (const DBus::Error &e) {
    LOG(FATAL) << "DBus exception: " << e.name() << ": " << e.what();
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
  SLOG(&path(), 2) << __func__;
  // XXX
}

void SupplicantProcessProxy::Proxy::InterfaceRemoved(
    const ::DBus::Path& /*path*/) {
  SLOG(&path(), 2) << __func__;
  // XXX
}

void SupplicantProcessProxy::Proxy::PropertiesChanged(
    const map<string, ::DBus::Variant>& /*properties*/) {
  SLOG(&path(), 2) << __func__;
  // XXX
}

}  // namespace shill
