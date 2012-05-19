// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/wimax_network_proxy.h"

#include <base/logging.h>
#include <chromeos/dbus/service_constants.h>

#include "shill/error.h"
#include "shill/scope_logger.h"

using std::string;

namespace shill {

WiMaxNetworkProxy::WiMaxNetworkProxy(DBus::Connection *connection,
                                     const DBus::Path &path)
    : proxy_(connection, path) {}

WiMaxNetworkProxy::~WiMaxNetworkProxy() {}

DBus::Path WiMaxNetworkProxy::proxy_object_path() const {
  return proxy_.path();
}

uint32 WiMaxNetworkProxy::Identifier(Error *error) {
  SLOG(DBus, 2) << __func__;
  try {
    return proxy_.Identifier();
  } catch (const DBus::Error &e) {
    FromDBusError(e, error);
  }
  return 0;
}

string WiMaxNetworkProxy::Name(Error *error) {
  SLOG(DBus, 2) << __func__;
  try {
    return proxy_.Name();
  } catch (const DBus::Error &e) {
    FromDBusError(e, error);
  }
  return string();
}

int WiMaxNetworkProxy::Type(Error *error) {
  SLOG(DBus, 2) << __func__;
  try {
    return proxy_.Type();
  } catch (const DBus::Error &e) {
    FromDBusError(e, error);
  }
  return 0;
}

int WiMaxNetworkProxy::CINR(Error *error) {
  SLOG(DBus, 2) << __func__;
  try {
    return proxy_.CINR();
  } catch (const DBus::Error &e) {
    FromDBusError(e, error);
  }
  return 0;
}

int WiMaxNetworkProxy::RSSI(Error *error) {
  SLOG(DBus, 2) << __func__;
  try {
    return proxy_.RSSI();
  } catch (const DBus::Error &e) {
    FromDBusError(e, error);
  }
  return 0;
}

// static
void WiMaxNetworkProxy::FromDBusError(const DBus::Error &dbus_error,
                                      Error *error) {
  if (!error) {
    return;
  }
  if (!dbus_error.is_set()) {
    error->Reset();
    return;
  }
  Error::PopulateAndLog(error, Error::kOperationFailed, dbus_error.what());
}

WiMaxNetworkProxy::Proxy::Proxy(DBus::Connection *connection,
                                const DBus::Path &path)
    : DBus::ObjectProxy(*connection, path,
                        wimax_manager::kWiMaxManagerServiceName) {}

WiMaxNetworkProxy::Proxy::~Proxy() {}

}  // namespace shill
