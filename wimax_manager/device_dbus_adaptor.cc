// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wimax_manager/device_dbus_adaptor.h"

#include <vector>

#include <base/logging.h>
#include <base/stringprintf.h>
#include <chromeos/dbus/service_constants.h>

#include "wimax_manager/device.h"
#include "wimax_manager/network.h"
#include "wimax_manager/network_dbus_adaptor.h"

using org::chromium::WiMaxManager::Device_adaptor;
using std::string;
using std::vector;

namespace wimax_manager {

DeviceDBusAdaptor::DeviceDBusAdaptor(DBus::Connection *connection,
                                     Device *device)
    : DBusAdaptor(connection, GetDeviceObjectPath(*device)),
      device_(device) {
  Index = device->index();
  Name = device->name();
  Networks = vector<DBus::Path>();
}

DeviceDBusAdaptor::~DeviceDBusAdaptor() {
}

// static
string DeviceDBusAdaptor::GetDeviceObjectPath(const Device &device) {
  return base::StringPrintf("%s%s", kDeviceObjectPathPrefix,
                            device.name().c_str());
}

void DeviceDBusAdaptor::Enable(DBus::Error &error) {  // NOLINT
  if (!device_->Enable()) {
    SetError(&error, "Failed to enable device " + device_->name());
  }
}

void DeviceDBusAdaptor::Disable(DBus::Error &error) {  // NOLINT
  if (!device_->Disable()) {
    SetError(&error, "Failed to disable device " + device_->name());
  }
}

void DeviceDBusAdaptor::ScanNetworks(DBus::Error &error) {  // NOLINT
  if (!device_->ScanNetworks()) {
    SetError(&error, "Failed to scan networks from device " + device_->name());
  }
}

void DeviceDBusAdaptor::Connect(DBus::Error &error) {  // NOLINT
  if (!device_->Connect()) {
    SetError(&error,
             "Failed to connect device " + device_->name() + " to network");
  }
}

void DeviceDBusAdaptor::Disconnect(DBus::Error &error) {  // NOLINT
  if (!device_->Disconnect()) {
    SetError(&error,
             "Failed to disconnect device " + device_->name() +
             " from network");
  }
}

void DeviceDBusAdaptor::UpdateNetworks() {
  vector<DBus::Path> network_paths;
  const vector<Network *> &networks = device_->networks();
  for (vector<Network *>::const_iterator network_iterator = networks.begin();
       network_iterator != networks.end(); ++network_iterator) {
    network_paths.push_back((*network_iterator)->dbus_object_path());
  }
  Networks = network_paths;
  NetworksChanged(network_paths);
}

}  // namespace wimax_manager
