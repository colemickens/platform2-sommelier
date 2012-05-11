// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wimax_manager/device_dbus_adaptor.h"

#include "wimax_manager/device.h"

using std::string;

namespace wimax_manager {

DeviceDBusAdaptor::DeviceDBusAdaptor(DBus::Connection *connection,
                                     const string &object_path,
                                     Device *device)
    : DBusAdaptor(connection, object_path),
      device_(device) {
}

DeviceDBusAdaptor::~DeviceDBusAdaptor() {
}

}  // namespace wimax_manager
