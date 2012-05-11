// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wimax_manager/manager_dbus_adaptor.h"

#include "wimax_manager/manager.h"

namespace wimax_manager {

namespace {

const char kManagerDBusObjectPath[] = "/org/chromium/WiMaxManager";

}  // namespace

ManagerDBusAdaptor::ManagerDBusAdaptor(DBus::Connection *connection,
                                       Manager *manager)
    : DBusAdaptor(connection, kManagerDBusObjectPath),
      manager_(manager) {
}

ManagerDBusAdaptor::~ManagerDBusAdaptor() {
}

}  // namespace wimax_manager
