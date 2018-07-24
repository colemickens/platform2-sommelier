// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/wimax/wimax_network_proxy.h"

#include <chromeos/dbus/service_constants.h>

#include "shill/error.h"
#include "shill/logging.h"

using std::string;

namespace shill {

namespace Logging {
static auto kModuleLogScope = ScopeLogger::kDBus;
static string ObjectID(const DBus::Path* p) { return *p; }
}

WiMaxNetworkProxy::WiMaxNetworkProxy(DBus::Connection* connection,
                                     const DBus::Path& path)
    : proxy_(connection, path) {}

WiMaxNetworkProxy::~WiMaxNetworkProxy() {}

RpcIdentifier WiMaxNetworkProxy::path() const {
  return proxy_.path();
}

void WiMaxNetworkProxy::set_signal_strength_changed_callback(
    const SignalStrengthChangedCallback& callback) {
  proxy_.set_signal_strength_changed_callback(callback);
}

uint32_t WiMaxNetworkProxy::Identifier(Error* error) {
  SLOG(&proxy_.path(), 2) << __func__;
  try {
    return proxy_.Identifier();
  } catch (const DBus::Error& e) {
    FromDBusError(e, error);
  }
  return 0;
}

string WiMaxNetworkProxy::Name(Error* error) {
  SLOG(&proxy_.path(), 2) << __func__;
  try {
    return proxy_.Name();
  } catch (const DBus::Error& e) {
    FromDBusError(e, error);
  }
  return string();
}

int WiMaxNetworkProxy::Type(Error* error) {
  SLOG(&proxy_.path(), 2) << __func__;
  try {
    return proxy_.Type();
  } catch (const DBus::Error& e) {
    FromDBusError(e, error);
  }
  return 0;
}

int WiMaxNetworkProxy::CINR(Error* error) {
  SLOG(&proxy_.path(), 2) << __func__;
  try {
    return proxy_.CINR();
  } catch (const DBus::Error& e) {
    FromDBusError(e, error);
  }
  return 0;
}

int WiMaxNetworkProxy::RSSI(Error* error) {
  SLOG(&proxy_.path(), 2) << __func__;
  try {
    return proxy_.RSSI();
  } catch (const DBus::Error& e) {
    FromDBusError(e, error);
  }
  return 0;
}

int WiMaxNetworkProxy::SignalStrength(Error* error) {
  SLOG(&proxy_.path(), 2) << __func__;
  try {
    return proxy_.SignalStrength();
  } catch (const DBus::Error& e) {
    FromDBusError(e, error);
  }
  return 0;
}

// static
void WiMaxNetworkProxy::FromDBusError(const DBus::Error& dbus_error,
                                      Error* error) {
  if (!error) {
    return;
  }
  if (!dbus_error.is_set()) {
    error->Reset();
    return;
  }
  Error::PopulateAndLog(
      FROM_HERE, error, Error::kOperationFailed, dbus_error.what());
}

WiMaxNetworkProxy::Proxy::Proxy(DBus::Connection* connection,
                                const DBus::Path& path)
    : DBus::ObjectProxy(*connection, path,
                        wimax_manager::kWiMaxManagerServiceName) {}

WiMaxNetworkProxy::Proxy::~Proxy() {}

void WiMaxNetworkProxy::Proxy::set_signal_strength_changed_callback(
    const SignalStrengthChangedCallback& callback) {
  signal_strength_changed_callback_ = callback;
}

void WiMaxNetworkProxy::Proxy::SignalStrengthChanged(
    const int32_t& signal_strength) {
  SLOG(&path(), 2) << __func__ << "(" << signal_strength << ")";
  if (!signal_strength_changed_callback_.is_null()) {
    signal_strength_changed_callback_.Run(signal_strength);
  }
}

}  // namespace shill
