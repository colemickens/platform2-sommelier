// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wimax_manager/device_dbus_adaptor.h"

#include <base/logging.h>
#include <base/stringprintf.h>

#include "wimax_manager/device.h"

using org::chromium::WiMaxManager::Device_adaptor;
using std::string;

namespace wimax_manager {

namespace {

const char kDeviceDBusObjectPathPrefix[] = "/org/chromium/WiMaxManager/Device";

}  // namespace

// static
string DeviceDBusAdaptor::GetDeviceObjectPath(const Device &device) {
  return base::StringPrintf("%s/%d",
                            kDeviceDBusObjectPathPrefix, device.index());
}

DeviceDBusAdaptor::DeviceDBusAdaptor(DBus::Connection *connection,
                                     Device *device)
    : DBusAdaptor(connection, GetDeviceObjectPath(*device)),
      device_(device) {
  Index = device->index();
  Name = device->name();
}

DeviceDBusAdaptor::~DeviceDBusAdaptor() {
}

void DeviceDBusAdaptor::Enable(DBus::Error &error) {  // NOLINT
  if (!device_->Enable()) {
  }
}

void DeviceDBusAdaptor::Disable(DBus::Error &error) {  // NOLINT
  if (!device_->Disable()) {
  }
}

void DeviceDBusAdaptor::Connect(DBus::Error &error) {  // NOLINT
  if (!device_->Connect()) {
  }
}

void DeviceDBusAdaptor::Disconnect(DBus::Error &error) {  // NOLINT
  if (!device_->Disconnect()) {
  }
}

}  // namespace wimax_manager
