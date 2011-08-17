// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/service_dbus_adaptor.h"

#include <map>
#include <string>

#include <base/logging.h>

#include "shill/error.h"
#include "shill/service.h"

using std::map;
using std::string;

namespace shill {

// static
const char ServiceDBusAdaptor::kInterfaceName[] = SHILL_INTERFACE;
// static
const char ServiceDBusAdaptor::kPath[] = "/service/";

ServiceDBusAdaptor::ServiceDBusAdaptor(DBus::Connection* conn, Service *service)
    : DBusAdaptor(conn, kPath + service->UniqueName()),
      service_(service) {}

ServiceDBusAdaptor::~ServiceDBusAdaptor() {
  service_ = NULL;
}

void ServiceDBusAdaptor::UpdateConnected() {}

void ServiceDBusAdaptor::EmitBoolChanged(const std::string& name, bool value) {
  PropertyChanged(name, DBusAdaptor::BoolToVariant(value));
}

void ServiceDBusAdaptor::EmitUintChanged(const std::string& name,
                                         uint32 value) {
  PropertyChanged(name, DBusAdaptor::Uint32ToVariant(value));
}

void ServiceDBusAdaptor::EmitIntChanged(const std::string& name, int value) {
  PropertyChanged(name, DBusAdaptor::Int32ToVariant(value));
}

void ServiceDBusAdaptor::EmitStringChanged(const std::string& name,
                                           const std::string& value) {
  PropertyChanged(name, DBusAdaptor::StringToVariant(value));

}

map<string, ::DBus::Variant> ServiceDBusAdaptor::GetProperties(
    ::DBus::Error &error) {
  map<string, ::DBus::Variant> properties;
  DBusAdaptor::GetProperties(service_->store(), &properties, &error);
  return properties;
}

void ServiceDBusAdaptor::SetProperty(const string& name,
                                     const ::DBus::Variant& value,
                                     ::DBus::Error &error) {
  DBusAdaptor::DispatchOnType(service_->store(), name, value, &error);
}

void ServiceDBusAdaptor::ClearProperty(const string& , ::DBus::Error &error) {
}

void ServiceDBusAdaptor::Connect(::DBus::Error &error) {
  service_->Connect();
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

void ServiceDBusAdaptor::ActivateCellularModem(const string &carrier,
                                               ::DBus::Error &error) {
  service_->ActivateCellularModem(carrier);
}

}  // namespace shill
