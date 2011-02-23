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

const char DBusManagerProxy::kInterfaceName[] = "org.chromium.shill.Manager";
const char DBusManagerProxy::kPath[] = "/org/chromium/shill/Manager";
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


DBusManagerProxy::DBusManagerProxy(Manager *manager)
  : interface_(kInterfaceName),
    path_(kPath),
    manager_(manager) {}

void DBusManagerProxy::UpdateRunning() {
}


ManagerProxyInterface *DBusControl::CreateManagerProxy(Manager *manager) {
  return new DBusManagerProxy(manager);
}

}  // namespace shill
