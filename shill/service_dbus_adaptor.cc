// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
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
const char ServiceDBusAdaptor::kPath[] = "/service/";

ServiceDBusAdaptor::ServiceDBusAdaptor(DBus::Connection* conn, Service *service)
    : DBusAdaptor(conn, kPath + service->UniqueName()),
      service_(service) {}

ServiceDBusAdaptor::~ServiceDBusAdaptor() {
  service_ = NULL;
}

void ServiceDBusAdaptor::UpdateConnected() {}

void ServiceDBusAdaptor::EmitBoolChanged(const string &name, bool value) {
  PropertyChanged(name, DBusAdaptor::BoolToVariant(value));
}

void ServiceDBusAdaptor::EmitUint8Changed(const string &name, uint8 value) {
  PropertyChanged(name, DBusAdaptor::ByteToVariant(value));
}

void ServiceDBusAdaptor::EmitUint16Changed(const string &name, uint16 value) {
  PropertyChanged(name, DBusAdaptor::Uint16ToVariant(value));
}

void ServiceDBusAdaptor::EmitUintChanged(const string &name, uint32 value) {
  PropertyChanged(name, DBusAdaptor::Uint32ToVariant(value));
}

void ServiceDBusAdaptor::EmitIntChanged(const string &name, int value) {
  PropertyChanged(name, DBusAdaptor::Int32ToVariant(value));
}

void ServiceDBusAdaptor::EmitStringChanged(const string &name,
                                           const string &value) {
  PropertyChanged(name, DBusAdaptor::StringToVariant(value));
}

void ServiceDBusAdaptor::EmitStringmapChanged(const string &name,
                                              const Stringmap &value) {
  PropertyChanged(name, DBusAdaptor::StringmapToVariant(value));
}

map<string, ::DBus::Variant> ServiceDBusAdaptor::GetProperties(
    ::DBus::Error &error) {
  map<string, ::DBus::Variant> properties;
  DBusAdaptor::GetProperties(service_->store(), &properties, &error);
  return properties;
}

void ServiceDBusAdaptor::SetProperty(const string &name,
                                     const ::DBus::Variant &value,
                                     ::DBus::Error &error) {
  DBusAdaptor::SetProperty(service_->mutable_store(), name, value, &error);
}

void ServiceDBusAdaptor::ClearProperty(const string &name,
                                       ::DBus::Error &error) {
  DBusAdaptor::ClearProperty(service_->mutable_store(), name, &error);
}

void ServiceDBusAdaptor::Connect(::DBus::Error &error) {
  Error e;
  service_->Connect(&e);
  e.ToDBusError(&error);
}

void ServiceDBusAdaptor::Disconnect(::DBus::Error &error) {
  Error e;
  service_->Disconnect(&e);
  e.ToDBusError(&error);
}

void ServiceDBusAdaptor::Remove(::DBus::Error &/*error*/) {
}

void ServiceDBusAdaptor::MoveBefore(const ::DBus::Path& ,
                                    ::DBus::Error &/*error*/) {
}

void ServiceDBusAdaptor::MoveAfter(const ::DBus::Path& ,
                                   ::DBus::Error &/*error*/) {
}

void ServiceDBusAdaptor::ActivateCellularModem(const string &carrier,
                                               ::DBus::Error &error) {
  VLOG(2) << __func__;
  Error e(Error::kOperationInitiated);
  DBus::Tag *tag = new DBus::Tag();
  service_->ActivateCellularModem(carrier, &e, GetMethodReplyCallback(tag));
  ReturnResultOrDefer(tag, e, &error);
}

}  // namespace shill
