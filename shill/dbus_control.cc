// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <glib.h>

#include <string>

#include "shill/shill_event.h"
#include "shill/dbus_control.h"
#include "shill/dbus_control_int.h"

namespace shill {

#define SHILL_INTERFACE "org.chromium.shill."
#define SHILL_PATH "/org/chromium/shill/"

const char ManagerDBusAdaptor::kInterfaceName[] = SHILL_INTERFACE "Manager";
const char ManagerDBusAdaptor::kPath[] = SHILL_PATH "Manager";
const char ServiceDBusAdaptor::kInterfaceName[] = SHILL_INTERFACE "Service";
const char ServiceDBusAdaptor::kPath[] = SHILL_PATH "Service";
const char DeviceDBusAdaptor::kInterfaceName[] = SHILL_INTERFACE "Device";
const char DeviceDBusAdaptor::kPath[] = SHILL_PATH "Device";

void DBusAdaptor::SetProperty(const string& /* key */,
                            const string& /* val */) {
  // Update property hash table, and output DBus signals, if requested
}

const string *DBusAdaptor::GetProperty(const string & /* key */) {
  // FIXME(pstew): Should be doing a hash table lookup
  return new string("value");
}

void DBusAdaptor::ClearProperty(const string & /* key */) {
  // Remove entry from hash table
}


ManagerDBusAdaptor::ManagerDBusAdaptor(Manager *manager)
  : interface_(kInterfaceName),
    path_(kPath),
    manager_(manager) {}

void ManagerDBusAdaptor::UpdateRunning() {}

ServiceDBusAdaptor::ServiceDBusAdaptor(Service *service)
  : interface_(kInterfaceName),
    path_(kPath),
    service_(service) {}

void ServiceDBusAdaptor::UpdateConnected() {}

DeviceDBusAdaptor::DeviceDBusAdaptor(Device *device)
  : interface_(kInterfaceName),
    path_(kPath),
    device_(device) {}

void DeviceDBusAdaptor::UpdateEnabled() {}

ManagerAdaptorInterface *DBusControl::CreateManagerAdaptor(Manager *manager) {
  return new ManagerDBusAdaptor(manager);
}

ServiceAdaptorInterface *DBusControl::CreateServiceAdaptor(Service *service) {
  return new ServiceDBusAdaptor(service);
}

DeviceAdaptorInterface *DBusControl::CreateDeviceAdaptor(Device *device) {
  return new DeviceDBusAdaptor(device);
}


}  // namespace shill
