// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/wimax_manager_proxy.h"

#include <chromeos/dbus/service_constants.h>

#include "shill/dbus_properties.h"
#include "shill/error.h"
#include "shill/logging.h"

using std::vector;

namespace shill {

WiMaxManagerProxy::WiMaxManagerProxy(DBus::Connection *connection)
    : proxy_(connection) {}

WiMaxManagerProxy::~WiMaxManagerProxy() {}

void WiMaxManagerProxy::set_devices_changed_callback(
    const DevicesChangedCallback &callback) {
  proxy_.set_devices_changed_callback(callback);
}

RpcIdentifiers WiMaxManagerProxy::Devices(Error *error) {
  SLOG(DBus, 2) << __func__;
  vector<DBus::Path> dbus_devices;
  try {
    dbus_devices = proxy_.Devices();
  } catch (const DBus::Error &e) {
    Error::PopulateAndLog(error, Error::kOperationFailed, e.what());
  }
  RpcIdentifiers devices;
  DBusProperties::ConvertPathsToRpcIdentifiers(dbus_devices, &devices);
  return devices;
}

WiMaxManagerProxy::Proxy::Proxy(DBus::Connection *connection)
    : DBus::ObjectProxy(*connection,
                        wimax_manager::kWiMaxManagerServicePath,
                        wimax_manager::kWiMaxManagerServiceName) {}

WiMaxManagerProxy::Proxy::~Proxy() {}

void WiMaxManagerProxy::Proxy::set_devices_changed_callback(
    const DevicesChangedCallback &callback) {
  devices_changed_callback_ = callback;
}

void WiMaxManagerProxy::Proxy::DevicesChanged(
    const vector<DBus::Path> &devices) {
  SLOG(DBus, 2) << __func__ << "(" << devices.size() << ")";
  if (devices_changed_callback_.is_null()) {
    return;
  }
  RpcIdentifiers rpc_devices;
  DBusProperties::ConvertPathsToRpcIdentifiers(devices, &rpc_devices);
  devices_changed_callback_.Run(rpc_devices);
}

}  // namespace shill
