// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/service_dbus_adaptor.h"

#include <map>
#include <string>

using std::map;
using std::string;

namespace shill {

// TODO(cmasone): Figure out if we should be trying to own sub-interfaces.
// static
const char ServiceDBusAdaptor::kInterfaceName[] = SHILL_INTERFACE;
// ".Service";
// static
const char ServiceDBusAdaptor::kPath[] = SHILL_PATH "/Service";

ServiceDBusAdaptor::ServiceDBusAdaptor(DBus::Connection& conn, Service *service)
    : DBusAdaptor(conn, kPath),
      service_(service) {}
ServiceDBusAdaptor::~ServiceDBusAdaptor() {}

void ServiceDBusAdaptor::UpdateConnected() {}

map<string, ::DBus::Variant> ServiceDBusAdaptor::GetProperties(
    ::DBus::Error &error) {
  return map<string, ::DBus::Variant>();
}

void ServiceDBusAdaptor::SetProperty(const string& ,
                                     const ::DBus::Variant& ,
                                     ::DBus::Error &error) {
}

void ServiceDBusAdaptor::ClearProperty(const string& , ::DBus::Error &error) {
}

void ServiceDBusAdaptor::Connect(::DBus::Error &error) {
}

void ServiceDBusAdaptor::Disconnect(::DBus::Error &error) {
}

void ServiceDBusAdaptor::Remove(::DBus::Error &error) {
}

void ServiceDBusAdaptor::MoveBefore(const ::DBus::Path& ,
                                    ::DBus::Error &error) {
}

void ServiceDBusAdaptor::MoveAfter(const ::DBus::Path& ,
                                   ::DBus::Error &error) {
}

void ServiceDBusAdaptor::ActivateCellularModem(const string& ,
                                               ::DBus::Error &error) {
}

}  // namespace shill
