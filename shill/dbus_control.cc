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

const char ManagerDBusProxy::kInterfaceName[] = SHILL_INTERFACE "Manager";
const char ManagerDBusProxy::kPath[] = SHILL_PATH "Manager";
const char ServiceDBusProxy::kInterfaceName[] = SHILL_INTERFACE "Service";
const char ServiceDBusProxy::kPath[] = SHILL_PATH "Service";
const char DeviceDBusProxy::kInterfaceName[] = SHILL_INTERFACE "Device";
const char DeviceDBusProxy::kPath[] = SHILL_PATH "Device";

void DBusProxy::SetProperty(const string& /* key */,
                            const string& /* val */) {
  // Update property hash table, and output DBus signals, if requested
}

const string *DBusProxy::GetProperty(const string & /* key */) {
  // FIXME(pstew): Should be doing a hash table lookup
  return new string("value");
}

void DBusProxy::ClearProperty(const string & /* key */) {
  // Remove entry from hash table
}


ManagerDBusProxy::ManagerDBusProxy(Manager *manager)
  : interface_(kInterfaceName),
    path_(kPath),
    manager_(manager) {}

void ManagerDBusProxy::UpdateRunning() {}

ServiceDBusProxy::ServiceDBusProxy(Service *service)
  : interface_(kInterfaceName),
    path_(kPath),
    service_(service) {}

void ServiceDBusProxy::UpdateConnected() {}

DeviceDBusProxy::DeviceDBusProxy(Device *device)
  : interface_(kInterfaceName),
    path_(kPath),
    device_(device) {}

void DeviceDBusProxy::UpdateEnabled() {}

ManagerProxyInterface *DBusControl::CreateManagerProxy(Manager *manager) {
  return new ManagerDBusProxy(manager);
}

ServiceProxyInterface *DBusControl::CreateServiceProxy(Service *service) {
  return new ServiceDBusProxy(service);
}

DeviceProxyInterface *DBusControl::CreateDeviceProxy(Device *device) {
  return new DeviceDBusProxy(device);
}


}  // namespace shill
