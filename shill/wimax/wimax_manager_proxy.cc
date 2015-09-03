//
// Copyright (C) 2012 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include "shill/wimax/wimax_manager_proxy.h"

#include <string>

#include <chromeos/dbus/service_constants.h>

#include "shill/dbus_properties.h"
#include "shill/error.h"
#include "shill/logging.h"

using std::string;
using std::vector;

namespace shill {

namespace Logging {
static auto kModuleLogScope = ScopeLogger::kDBus;
static string ObjectID(WiMaxManagerProxy* w) { return "(wimax_manager_proxy)"; }
}

WiMaxManagerProxy::WiMaxManagerProxy(DBus::Connection* connection)
    : proxy_(connection) {}

WiMaxManagerProxy::~WiMaxManagerProxy() {}

void WiMaxManagerProxy::set_devices_changed_callback(
    const DevicesChangedCallback& callback) {
  proxy_.set_devices_changed_callback(callback);
}

RpcIdentifiers WiMaxManagerProxy::Devices(Error* error) {
  SLOG(this, 2) << __func__;
  vector<DBus::Path> dbus_devices;
  try {
    dbus_devices = proxy_.Devices();
  } catch (const DBus::Error& e) {
    Error::PopulateAndLog(FROM_HERE, error, Error::kOperationFailed, e.what());
  }
  RpcIdentifiers devices;
  DBusProperties::ConvertPathsToRpcIdentifiers(dbus_devices, &devices);
  return devices;
}

WiMaxManagerProxy::Proxy::Proxy(DBus::Connection* connection)
    : DBus::ObjectProxy(*connection,
                        wimax_manager::kWiMaxManagerServicePath,
                        wimax_manager::kWiMaxManagerServiceName) {}

WiMaxManagerProxy::Proxy::~Proxy() {}

void WiMaxManagerProxy::Proxy::set_devices_changed_callback(
    const DevicesChangedCallback& callback) {
  devices_changed_callback_ = callback;
}

void WiMaxManagerProxy::Proxy::DevicesChanged(
    const vector<DBus::Path>& devices) {
  SLOG(DBus, nullptr, 2) << __func__ << "(" << devices.size() << ")";
  if (devices_changed_callback_.is_null()) {
    return;
  }
  RpcIdentifiers rpc_devices;
  DBusProperties::ConvertPathsToRpcIdentifiers(devices, &rpc_devices);
  devices_changed_callback_.Run(rpc_devices);
}

}  // namespace shill
