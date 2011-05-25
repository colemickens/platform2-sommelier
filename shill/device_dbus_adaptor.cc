// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/device.h"
#include "shill/device_dbus_adaptor.h"

#include <map>
#include <string>

#include "shill/device.h"

using std::map;
using std::string;

namespace shill {

// static
const char DeviceDBusAdaptor::kInterfaceName[] = SHILL_INTERFACE;
// static
const char DeviceDBusAdaptor::kPath[] = "/device/";

DeviceDBusAdaptor::DeviceDBusAdaptor(DBus::Connection& conn, Device *device)
    : DBusAdaptor(conn, kPath + device->UniqueName()),
      device_(device) {
}
DeviceDBusAdaptor::~DeviceDBusAdaptor() {}

void DeviceDBusAdaptor::UpdateEnabled() {}

map<string, ::DBus::Variant> DeviceDBusAdaptor::GetProperties(
    ::DBus::Error &error) {
  return map<string, ::DBus::Variant>();
}

void DeviceDBusAdaptor::SetProperty(const string& ,
                                    const ::DBus::Variant& ,
                                    ::DBus::Error &error) {
}

void DeviceDBusAdaptor::ProposeScan(::DBus::Error &error) {
}

::DBus::Path DeviceDBusAdaptor::AddIPConfig(const string& ,
                                            ::DBus::Error &error) {
  return ::DBus::Path();
}

}  // namespace shill
