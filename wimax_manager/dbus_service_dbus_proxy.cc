// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wimax_manager/dbus_service_dbus_proxy.h"

#include <base/logging.h>
#include <chromeos/dbus/service_constants.h>

#include "wimax_manager/dbus_service.h"

using std::string;

namespace wimax_manager {

DBusServiceDBusProxy::DBusServiceDBusProxy(DBus::Connection *connection,
                                           DBusService *dbus_service)
    : DBusProxy(connection, dbus::kDBusServiceName, dbus::kDBusServicePath),
      dbus_service_(dbus_service) {
  CHECK(dbus_service_);
}

DBusServiceDBusProxy::~DBusServiceDBusProxy() {
}

void DBusServiceDBusProxy::NameOwnerChanged(const string &name,
                                            const string &old_owner,
                                            const string &new_owner) {
  dbus_service_->OnNameOwnerChanged(name, old_owner, new_owner);
}

}  // namespace wimax_manager
