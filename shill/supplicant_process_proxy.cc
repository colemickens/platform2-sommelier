// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "supplicant_process_proxy.h"

#include <map>
#include <string>

#include <base/logging.h>
#include <dbus-c++/dbus.h>

using std::map;
using std::string;

namespace shill {

SupplicantProcessProxy::SupplicantProcessProxy(
    const char *dbus_path, const char *dbus_addr)
    : connection_(DBus::Connection::SystemBus()),
      proxy_(&connection_, dbus_path, dbus_addr) {}

SupplicantProcessProxy::~SupplicantProcessProxy() {}

::DBus::Path SupplicantProcessProxy::CreateInterface(
    const map<string, ::DBus::Variant> &args) {
  return proxy_.CreateInterface(args);
}

void SupplicantProcessProxy::RemoveInterface(const ::DBus::Path &path) {
  return proxy_.RemoveInterface(path);
}

::DBus::Path SupplicantProcessProxy::GetInterface(const string &ifname) {
  return proxy_.GetInterface(ifname);
}

// definitions for private class SupplicantProcessProxy::Proxy

SupplicantProcessProxy::Proxy::Proxy(
    DBus::Connection *bus, const char *dbus_path, const char *dbus_addr)
    : DBus::ObjectProxy(*bus, dbus_path, dbus_addr) {}

SupplicantProcessProxy::Proxy::~Proxy() {}

void SupplicantProcessProxy::Proxy::InterfaceAdded(
    const ::DBus::Path& path,
    const map<string, ::DBus::Variant> &properties) {
  LOG(INFO) << __func__;
  // XXX
}

void SupplicantProcessProxy::Proxy::InterfaceRemoved(
    const ::DBus::Path& path) {
  LOG(INFO) << __func__;
  // XXX
}

void SupplicantProcessProxy::Proxy::PropertiesChanged(
    const map<string, ::DBus::Variant>& properties) {
  LOG(INFO) << __func__;
  // XXX
}

}  // namespace shill
