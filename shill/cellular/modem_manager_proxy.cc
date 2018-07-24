// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/cellular/modem_manager_proxy.h"

#include "shill/cellular/modem_manager.h"
#include "shill/logging.h"

using std::string;
using std::vector;

namespace shill {

namespace Logging {
static auto kModuleLogScope = ScopeLogger::kDBus;
static string ObjectID(const DBus::Path* p) { return *p; }
}

ModemManagerProxy::ModemManagerProxy(DBus::Connection* connection,
                                     ModemManagerClassic* manager,
                                     const string& path,
                                     const string& service)
    : proxy_(connection, manager, path, service) {}

ModemManagerProxy::~ModemManagerProxy() {}

vector<DBus::Path> ModemManagerProxy::EnumerateDevices() {
  SLOG(&proxy_.path(), 2) << __func__;
  try {
    return proxy_.EnumerateDevices();
  } catch (const DBus::Error& e) {
    LOG(ERROR) << "DBus exception: " << e.name() << ": " << e.what();
  }
  return vector<DBus::Path>();
}

ModemManagerProxy::Proxy::Proxy(DBus::Connection* connection,
                                ModemManagerClassic* manager,
                                const string& path,
                                const string& service)
    : DBus::ObjectProxy(*connection, path, service.c_str()),
      manager_(manager) {}

ModemManagerProxy::Proxy::~Proxy() {}

void ModemManagerProxy::Proxy::DeviceAdded(const DBus::Path& device) {
  SLOG(&path(), 2) << __func__;
  manager_->OnDeviceAdded(device);
}

void ModemManagerProxy::Proxy::DeviceRemoved(const DBus::Path& device) {
  SLOG(&path(), 2) << __func__;
  manager_->OnDeviceRemoved(device);
}

}  // namespace shill
