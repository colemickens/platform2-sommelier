// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/wimax_manager_proxy.h"

#include <base/logging.h>
#include <chromeos/dbus/service_constants.h>

#include "shill/error.h"
#include "shill/scope_logger.h"

using std::vector;

namespace shill {

WiMaxManagerProxy::WiMaxManagerProxy(DBus::Connection *connection)
    : proxy_(connection) {}

WiMaxManagerProxy::~WiMaxManagerProxy() {}

vector<RpcIdentifier> WiMaxManagerProxy::Devices(Error *error) {
  SLOG(DBus, 2) << __func__;
  vector<DBus::Path> dbus_devices;
  try {
    dbus_devices = proxy_.Devices();
  } catch (const DBus::Error &e) {
    Error::PopulateAndLog(error, Error::kOperationFailed, e.what());
  }
  vector<RpcIdentifier> devices;
  for (vector<DBus::Path>::const_iterator it = dbus_devices.begin();
       it != dbus_devices.end(); ++it) {
    devices.push_back(*it);
  }
  return devices;
}

WiMaxManagerProxy::Proxy::Proxy(DBus::Connection *connection)
    : DBus::ObjectProxy(*connection,
                        wimax_manager::kWiMaxManagerServicePath,
                        wimax_manager::kWiMaxManagerServiceName) {}

WiMaxManagerProxy::Proxy::~Proxy() {}

}  // namespace shill
