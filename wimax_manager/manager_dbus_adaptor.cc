// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wimax_manager/manager_dbus_adaptor.h"

#include <base/logging.h>

#include "wimax_manager/device_dbus_adaptor.h"
#include "wimax_manager/manager.h"

using std::vector;

namespace wimax_manager {

namespace {

const char kManagerDBusObjectPath[] = "/org/chromium/WiMaxManager";

}  // namespace

ManagerDBusAdaptor::ManagerDBusAdaptor(DBus::Connection *connection,
                                       Manager *manager)
    : DBusAdaptor(connection, kManagerDBusObjectPath),
      manager_(manager) {
  Devices = vector<DBus::Path>();
}

ManagerDBusAdaptor::~ManagerDBusAdaptor() {
}

void ManagerDBusAdaptor::UpdateDevices() {
  vector<DBus::Path> device_paths;
  const vector<Device *> &devices = manager_->devices();
  for (vector<Device *>::const_iterator device_iterator = devices.begin();
       device_iterator != devices.end(); ++ device_iterator) {
    device_paths.push_back(
        DeviceDBusAdaptor::GetDeviceObjectPath(*(*device_iterator)));
  }
  Devices = device_paths;
}

}  // namespace wimax_manager
