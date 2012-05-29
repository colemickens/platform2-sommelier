// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/wimax_device_proxy.h"

#include <base/bind.h>
#include <base/logging.h>
#include <chromeos/dbus/service_constants.h>

#include "shill/error.h"
#include "shill/scope_logger.h"

using base::Bind;
using base::Callback;
using base::Unretained;
using std::string;
using std::vector;
using wimax_manager::DeviceStatus;

namespace shill {

WiMaxDeviceProxy::WiMaxDeviceProxy(DBus::Connection *connection,
                                   const DBus::Path &path)
    : proxy_(connection, path) {}

WiMaxDeviceProxy::~WiMaxDeviceProxy() {}

void WiMaxDeviceProxy::Enable(Error *error,
                              const ResultCallback &callback,
                              int timeout) {
  SLOG(DBus, 2) << __func__ << "(" << timeout << ")";
  Invoke(Bind((void (Proxy::*)(void *, int))&Proxy::Enable,
              Unretained(&proxy_)),
         error, callback, timeout);
}

void WiMaxDeviceProxy::Disable(Error *error,
                               const ResultCallback &callback,
                               int timeout) {
  SLOG(DBus, 2) << __func__ << "(" << timeout << ")";
  Invoke(Bind((void (Proxy::*)(void *, int))&Proxy::Disable,
              Unretained(&proxy_)),
         error, callback, timeout);
}

void WiMaxDeviceProxy::ScanNetworks(Error *error,
                                    const ResultCallback &callback,
                                    int timeout) {
  SLOG(DBus, 2) << __func__ << "(" << timeout << ")";
  Invoke(Bind((void (Proxy::*)(void *, int))&Proxy::ScanNetworks,
              Unretained(&proxy_)),
         error, callback, timeout);
}

void WiMaxDeviceProxy::Connect(const RpcIdentifier &network,
                               const KeyValueStore &parameters,
                               Error *error,
                               const ResultCallback &callback,
                               int timeout) {
  SLOG(DBus, 2) << __func__ << "(" << timeout << ")";
  DBus::Path path = network;
  DBusPropertiesMap args;
  DBusProperties::ConvertKeyValueStoreToMap(parameters, &args);
  Invoke(Bind((void (Proxy::*)(const DBus::Path &, const DBusPropertiesMap &,
                               void *, int))&Proxy::Connect,
              Unretained(&proxy_), path, args),
         error, callback, timeout);
}

void WiMaxDeviceProxy::Disconnect(Error *error,
                                  const ResultCallback &callback,
                                  int timeout) {
  SLOG(DBus, 2) << __func__ << "(" << timeout << ")";
  Invoke(Bind((void (Proxy::*)(void *, int))&Proxy::Disconnect,
              Unretained(&proxy_)),
         error, callback, timeout);
}

void WiMaxDeviceProxy::set_networks_changed_callback(
    const NetworksChangedCallback &callback) {
  proxy_.set_networks_changed_callback(callback);
}

void WiMaxDeviceProxy::set_status_changed_callback(
    const StatusChangedCallback &callback) {
  proxy_.set_status_changed_callback(callback);
}

uint8 WiMaxDeviceProxy::Index(Error *error) {
  SLOG(DBus, 2) << __func__;
  try {
    return proxy_.Index();
  } catch (const DBus::Error &e) {
    FromDBusError(e, error);
  }
  return 0;
}

string WiMaxDeviceProxy::Name(Error *error) {
  SLOG(DBus, 2) << __func__;
  try {
    return proxy_.Name();
  } catch (const DBus::Error &e) {
    FromDBusError(e, error);
  }
  return string();
}

void WiMaxDeviceProxy::Invoke(const Callback<void(void *, int)> &method,
                              Error *error,
                              const ResultCallback &callback,
                              int timeout) {
  scoped_ptr<ResultCallback> cb(new ResultCallback(callback));
  try {
    method.Run(cb.get(), timeout);
    cb.release();
  } catch (const DBus::Error &e) {
    FromDBusError(e, error);
  }
}

// static
void WiMaxDeviceProxy::FromDBusError(const DBus::Error &dbus_error,
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

WiMaxDeviceProxy::Proxy::Proxy(DBus::Connection *connection,
                               const DBus::Path &path)
    : DBus::ObjectProxy(*connection, path,
                        wimax_manager::kWiMaxManagerServiceName) {}

WiMaxDeviceProxy::Proxy::~Proxy() {}

void WiMaxDeviceProxy::Proxy::set_networks_changed_callback(
    const NetworksChangedCallback &callback) {
  networks_changed_callback_ = callback;
}

void WiMaxDeviceProxy::Proxy::set_status_changed_callback(
    const StatusChangedCallback &callback) {
  status_changed_callback_ = callback;
}

void WiMaxDeviceProxy::Proxy::NetworksChanged(
    const vector<DBus::Path> &networks) {
  SLOG(DBus, 2) << __func__ << "(" << networks.size() << ")";
  if (networks_changed_callback_.is_null()) {
    return;
  }
  RpcIdentifiers rpc_networks;
  DBusProperties::ConvertPathsToRpcIdentifiers(networks, &rpc_networks);
  networks_changed_callback_.Run(rpc_networks);
}

void WiMaxDeviceProxy::Proxy::StatusChanged(const int32 &status) {
  SLOG(DBus, 2) << __func__ << "(" << status << ")";
  if (status_changed_callback_.is_null()) {
    return;
  }
  status_changed_callback_.Run(static_cast<DeviceStatus>(status));
}

void WiMaxDeviceProxy::Proxy::EnableCallback(const DBus::Error &error,
                                             void *data) {
  SLOG(DBus, 2) << __func__;
  HandleCallback(error, data);
}

void WiMaxDeviceProxy::Proxy::DisableCallback(const DBus::Error &error,
                                              void *data) {
  SLOG(DBus, 2) << __func__;
  HandleCallback(error, data);
}

void WiMaxDeviceProxy::Proxy::ScanNetworksCallback(const DBus::Error &error,
                                                   void *data) {
  SLOG(DBus, 2) << __func__;
  HandleCallback(error, data);
}

void WiMaxDeviceProxy::Proxy::ConnectCallback(const DBus::Error &error,
                                              void *data) {
  SLOG(DBus, 2) << __func__;
  HandleCallback(error, data);
}

void WiMaxDeviceProxy::Proxy::DisconnectCallback(const DBus::Error &error,
                                                 void *data) {
  SLOG(DBus, 2) << __func__;
  HandleCallback(error, data);
}

// static
void WiMaxDeviceProxy::Proxy::HandleCallback(const DBus::Error &error,
                                             void *data) {
  scoped_ptr<ResultCallback> callback(reinterpret_cast<ResultCallback *>(data));
  Error e;
  FromDBusError(error, &e);
  callback->Run(e);
}

}  // namespace shill
