// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wimax_manager/dbus_proxy.h"

using std::string;

namespace wimax_manager {

DBusProxy::DBusProxy(DBus::Connection *connection,
                     const string &service_name, const string &object_path)
    : DBus::ObjectProxy(*connection, object_path, service_name.c_str()) {
}

DBusProxy::~DBusProxy() {
}

}  // namespace wimax_manager
