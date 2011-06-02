// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include <dbus-c++/dbus.h>

#include "shill/dbus_adaptor.h"

namespace shill {

DBusAdaptor::DBusAdaptor(DBus::Connection* conn, const std::string& object_path)
    : DBus::ObjectAdaptor(*conn, object_path) {
}

DBusAdaptor::~DBusAdaptor() {}

// static
::DBus::Variant DBusAdaptor::BoolToVariant(bool value) {
  ::DBus::Variant v;
  v.writer().append_bool(value);
  return v;
}

// static
::DBus::Variant DBusAdaptor::UInt32ToVariant(uint32 value) {
  ::DBus::Variant v;
  v.writer().append_uint32(value);
  return v;
}

// static
::DBus::Variant DBusAdaptor::IntToVariant(int value) {
  ::DBus::Variant v;
  v.writer().append_int32(value);
  return v;
}

// static
::DBus::Variant DBusAdaptor::StringToVariant(const std::string& value) {
  ::DBus::Variant v;
  v.writer().append_string(value.c_str());
  return v;
}

}  // namespace shill
