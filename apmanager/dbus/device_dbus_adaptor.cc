// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apmanager/dbus/device_dbus_adaptor.h"

#include <base/strings/stringprintf.h>
#include <dbus_bindings/org.chromium.apmanager.Manager.h>

#include "apmanager/device.h"

using brillo::dbus_utils::ExportedObjectManager;
using brillo::dbus_utils::DBusObject;
using org::chromium::apmanager::ManagerAdaptor;
using std::string;

namespace apmanager {

DeviceDBusAdaptor::DeviceDBusAdaptor(
    const scoped_refptr<dbus::Bus>& bus,
    ExportedObjectManager* object_manager,
    Device* device)
    : adaptor_(this),
      object_path_(
          base::StringPrintf("%s/devices/%d",
                             ManagerAdaptor::GetObjectPath().value().c_str(),
                             device->identifier())),
      dbus_object_(object_manager, bus, object_path_) {
  // Register D-Bus object.
  adaptor_.RegisterWithDBusObject(&dbus_object_);
  dbus_object_.RegisterAndBlock();
}

DeviceDBusAdaptor::~DeviceDBusAdaptor() {}

void DeviceDBusAdaptor::SetDeviceName(const string& device_name) {
  adaptor_.SetDeviceName(device_name);
}

string DeviceDBusAdaptor::GetDeviceName() {
  return adaptor_.GetDeviceName();
}

void DeviceDBusAdaptor::SetPreferredApInterface(const string& interface_name) {
  adaptor_.SetPreferredApInterface(interface_name);
}

string DeviceDBusAdaptor::GetPreferredApInterface() {
  return adaptor_.GetPreferredApInterface();
}

void DeviceDBusAdaptor::SetInUse(bool in_use) {
  adaptor_.SetInUse(in_use);
}

bool DeviceDBusAdaptor::GetInUse() {
  return adaptor_.GetInUse();
}

}  // namespace apmanager
