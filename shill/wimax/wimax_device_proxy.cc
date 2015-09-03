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

#include "shill/wimax/wimax_device_proxy.h"

#include <memory>

#include <base/bind.h>
#include <chromeos/dbus/service_constants.h>

#include "shill/dbus_async_call_helper.h"
#include "shill/error.h"
#include "shill/logging.h"

using base::Bind;
using base::Callback;
using base::Unretained;
using std::string;
using std::vector;
using wimax_manager::DeviceStatus;

namespace shill {

WiMaxDeviceProxy::WiMaxDeviceProxy(DBus::Connection* connection,
                                   const DBus::Path& path)
    : proxy_(connection, path) {}

WiMaxDeviceProxy::~WiMaxDeviceProxy() {}

void WiMaxDeviceProxy::Enable(Error* error,
                              const ResultCallback& callback,
                              int timeout) {
  BeginAsyncDBusCall(__func__, proxy_, &Proxy::EnableAsync, callback, error,
                     &FromDBusError, timeout);
}

void WiMaxDeviceProxy::Disable(Error* error,
                               const ResultCallback& callback,
                               int timeout) {
  BeginAsyncDBusCall(__func__, proxy_, &Proxy::DisableAsync, callback, error,
                     &FromDBusError, timeout);
}

void WiMaxDeviceProxy::ScanNetworks(Error* error,
                                    const ResultCallback& callback,
                                    int timeout) {
  BeginAsyncDBusCall(__func__, proxy_, &Proxy::ScanNetworksAsync, callback,
                     error, &FromDBusError, timeout);
}

void WiMaxDeviceProxy::Connect(const RpcIdentifier& network,
                               const KeyValueStore& parameters,
                               Error* error,
                               const ResultCallback& callback,
                               int timeout) {
  DBus::Path path = network;
  DBusPropertiesMap args;
  DBusProperties::ConvertKeyValueStoreToMap(parameters, &args);
  BeginAsyncDBusCall(__func__, proxy_, &Proxy::ConnectAsync, callback, error,
                     &FromDBusError, timeout, path, args);
}

void WiMaxDeviceProxy::Disconnect(Error* error,
                                  const ResultCallback& callback,
                                  int timeout) {
  BeginAsyncDBusCall(__func__, proxy_, &Proxy::DisconnectAsync, callback, error,
                     &FromDBusError, timeout);
}

void WiMaxDeviceProxy::set_networks_changed_callback(
    const NetworksChangedCallback& callback) {
  proxy_.set_networks_changed_callback(callback);
}

void WiMaxDeviceProxy::set_status_changed_callback(
    const StatusChangedCallback& callback) {
  proxy_.set_status_changed_callback(callback);
}

uint8_t WiMaxDeviceProxy::Index(Error* error) {
  SLOG(&proxy_.path(), 2) << __func__;
  try {
    return proxy_.Index();
  } catch (const DBus::Error& e) {
    FromDBusError(e, error);
  }
  return 0;
}

string WiMaxDeviceProxy::Name(Error* error) {
  SLOG(&proxy_.path(), 2) << __func__;
  try {
    return proxy_.Name();
  } catch (const DBus::Error& e) {
    FromDBusError(e, error);
  }
  return string();
}

RpcIdentifiers WiMaxDeviceProxy::Networks(Error* error) {
  SLOG(&proxy_.path(), 2) << __func__;
  vector<DBus::Path> dbus_paths;
  try {
    dbus_paths = proxy_.Networks();
  } catch (const DBus::Error& e) {
    FromDBusError(e, error);
    return RpcIdentifiers();
  }
  RpcIdentifiers rpc_networks;
  DBusProperties::ConvertPathsToRpcIdentifiers(dbus_paths, &rpc_networks);
  return rpc_networks;
}

// static
void WiMaxDeviceProxy::FromDBusError(const DBus::Error& dbus_error,
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

WiMaxDeviceProxy::Proxy::Proxy(DBus::Connection* connection,
                               const DBus::Path& path)
    : DBus::ObjectProxy(*connection, path,
                        wimax_manager::kWiMaxManagerServiceName) {}

WiMaxDeviceProxy::Proxy::~Proxy() {}

void WiMaxDeviceProxy::Proxy::set_networks_changed_callback(
    const NetworksChangedCallback& callback) {
  networks_changed_callback_ = callback;
}

void WiMaxDeviceProxy::Proxy::set_status_changed_callback(
    const StatusChangedCallback& callback) {
  status_changed_callback_ = callback;
}

void WiMaxDeviceProxy::Proxy::NetworksChanged(
    const vector<DBus::Path>& networks) {
  SLOG(&path(), 2) << __func__ << "(" << networks.size() << ")";
  if (networks_changed_callback_.is_null()) {
    return;
  }
  RpcIdentifiers rpc_networks;
  DBusProperties::ConvertPathsToRpcIdentifiers(networks, &rpc_networks);
  networks_changed_callback_.Run(rpc_networks);
}

void WiMaxDeviceProxy::Proxy::StatusChanged(const int32_t& status) {
  SLOG(&path(), 2) << __func__ << "(" << status << ")";
  if (status_changed_callback_.is_null()) {
    return;
  }
  status_changed_callback_.Run(static_cast<DeviceStatus>(status));
}

void WiMaxDeviceProxy::Proxy::EnableCallback(const DBus::Error& error,
                                             void* data) {
  SLOG(&path(), 2) << __func__;
  HandleCallback(error, data);
}

void WiMaxDeviceProxy::Proxy::DisableCallback(const DBus::Error& error,
                                              void* data) {
  SLOG(&path(), 2) << __func__;
  HandleCallback(error, data);
}

void WiMaxDeviceProxy::Proxy::ScanNetworksCallback(const DBus::Error& error,
                                                   void* data) {
  SLOG(&path(), 2) << __func__;
  HandleCallback(error, data);
}

void WiMaxDeviceProxy::Proxy::ConnectCallback(const DBus::Error& error,
                                              void* data) {
  SLOG(&path(), 2) << __func__;
  HandleCallback(error, data);
}

void WiMaxDeviceProxy::Proxy::DisconnectCallback(const DBus::Error& error,
                                                 void* data) {
  SLOG(&path(), 2) << __func__;
  HandleCallback(error, data);
}

// static
void WiMaxDeviceProxy::Proxy::HandleCallback(const DBus::Error& error,
                                             void* data) {
  std::unique_ptr<ResultCallback> callback(
      reinterpret_cast<ResultCallback*>(data));
  Error e;
  FromDBusError(error, &e);
  callback->Run(e);
}

}  // namespace shill
