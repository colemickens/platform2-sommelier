// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/device.h"
#include "shill/device_dbus_adaptor.h"

#include <map>
#include <string>

using std::map;
using std::string;

namespace shill {

// TODO(cmasone): Figure out if we should be trying to own sub-interfaces.
// static
const char DeviceDBusAdaptor::kInterfaceName[] = SHILL_INTERFACE;  // ".Device";
// static
const char DeviceDBusAdaptor::kPath[] = SHILL_PATH "/Device";

DeviceDBusAdaptor::DeviceDBusAdaptor(DBus::Connection& conn, Device *device)
    : DBusAdaptor(conn, string(kPath) + string("/") + device->Name()),
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
