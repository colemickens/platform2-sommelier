// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/dbus/chromeos_wimax_device_proxy.h"

#include <base/bind.h>
#include <base/strings/stringprintf.h>
#include <chromeos/dbus/service_constants.h>

#include "shill/error.h"
#include "shill/logging.h"

using std::string;
using std::vector;
using wimax_manager::DeviceStatus;

namespace shill {

namespace Logging {
static auto kModuleLogScope = ScopeLogger::kDBus;
static string ObjectID(const dbus::ObjectPath* path) {
  return "wimax_device_proxy (" + path->value() + ")";
}
}

// static.
const char ChromeosWiMaxDeviceProxy::kPropertyIndex[] = "Index";
const char ChromeosWiMaxDeviceProxy::kPropertyName[] = "Name";
const char ChromeosWiMaxDeviceProxy::kPropertyNetworks[] = "Networks";

ChromeosWiMaxDeviceProxy::PropertySet::PropertySet(
    dbus::ObjectProxy* object_proxy,
    const std::string& interface_name,
    const PropertyChangedCallback& callback)
    : dbus::PropertySet(object_proxy, interface_name, callback) {
  RegisterProperty(kPropertyIndex, &index);
  RegisterProperty(kPropertyName, &name);
  RegisterProperty(kPropertyNetworks, &networks);
}

ChromeosWiMaxDeviceProxy::ChromeosWiMaxDeviceProxy(
    const scoped_refptr<dbus::Bus>& bus,
    const std::string& rpc_identifier)
    : proxy_(
        new org::chromium::WiMaxManager::DeviceProxy(
            bus,
            wimax_manager::kWiMaxManagerServiceName,
            dbus::ObjectPath(rpc_identifier))) {
  // Register signal handlers.
  proxy_->RegisterNetworksChangedSignalHandler(
      base::Bind(&ChromeosWiMaxDeviceProxy::NetworksChanged,
                 weak_factory_.GetWeakPtr()),
      base::Bind(&ChromeosWiMaxDeviceProxy::OnSignalConnected,
                 weak_factory_.GetWeakPtr()));
  proxy_->RegisterStatusChangedSignalHandler(
      base::Bind(&ChromeosWiMaxDeviceProxy::StatusChanged,
                 weak_factory_.GetWeakPtr()),
      base::Bind(&ChromeosWiMaxDeviceProxy::OnSignalConnected,
                 weak_factory_.GetWeakPtr()));

  // Register properties.
  properties_.reset(
      new PropertySet(
          proxy_->GetObjectProxy(),
          wimax_manager::kWiMaxManagerDeviceInterface,
          base::Bind(&ChromeosWiMaxDeviceProxy::OnPropertyChanged,
                     weak_factory_.GetWeakPtr())));
  properties_->ConnectSignals();
  properties_->GetAll();
}

ChromeosWiMaxDeviceProxy::~ChromeosWiMaxDeviceProxy() {
  proxy_->ReleaseObjectProxy(base::Bind(&base::DoNothing));
}

void ChromeosWiMaxDeviceProxy::Enable(Error* /*error*/,
                                      const ResultCallback& callback,
                                      int timeout) {
  proxy_->EnableAsync(
      base::Bind(&ChromeosWiMaxDeviceProxy::OnSuccess,
                 weak_factory_.GetWeakPtr(),
                 callback,
                 __func__),
      base::Bind(&ChromeosWiMaxDeviceProxy::OnFailure,
                 weak_factory_.GetWeakPtr(),
                 callback,
                 __func__),
      timeout);
}

void ChromeosWiMaxDeviceProxy::Disable(Error* /*error*/,
                                       const ResultCallback& callback,
                                       int timeout) {
  proxy_->DisableAsync(
      base::Bind(&ChromeosWiMaxDeviceProxy::OnSuccess,
                 weak_factory_.GetWeakPtr(),
                 callback,
                 __func__),
      base::Bind(&ChromeosWiMaxDeviceProxy::OnFailure,
                 weak_factory_.GetWeakPtr(),
                 callback,
                 __func__),
      timeout);
}

void ChromeosWiMaxDeviceProxy::ScanNetworks(Error* /*error*/,
                                            const ResultCallback& callback,
                                            int timeout) {
  proxy_->ScanNetworksAsync(
      base::Bind(&ChromeosWiMaxDeviceProxy::OnSuccess,
                 weak_factory_.GetWeakPtr(),
                 callback,
                 __func__),
      base::Bind(&ChromeosWiMaxDeviceProxy::OnFailure,
                 weak_factory_.GetWeakPtr(),
                 callback,
                 __func__),
      timeout);
}

void ChromeosWiMaxDeviceProxy::Connect(const RpcIdentifier& network,
                                       const KeyValueStore& parameters,
                                       Error* /*error*/,
                                       const ResultCallback& callback,
                                       int timeout) {
  proxy_->ConnectAsync(
      dbus::ObjectPath(network),
      parameters.properties(),
      base::Bind(&ChromeosWiMaxDeviceProxy::OnSuccess,
                 weak_factory_.GetWeakPtr(),
                 callback,
                 __func__),
      base::Bind(&ChromeosWiMaxDeviceProxy::OnFailure,
                 weak_factory_.GetWeakPtr(),
                 callback,
                 __func__),
      timeout);
}

void ChromeosWiMaxDeviceProxy::Disconnect(Error* /*error*/,
                                          const ResultCallback& callback,
                                          int timeout) {
  proxy_->DisconnectAsync(
      base::Bind(&ChromeosWiMaxDeviceProxy::OnSuccess,
                 weak_factory_.GetWeakPtr(),
                 callback,
                 __func__),
      base::Bind(&ChromeosWiMaxDeviceProxy::OnFailure,
                 weak_factory_.GetWeakPtr(),
                 callback,
                 __func__),
      timeout);
}

void ChromeosWiMaxDeviceProxy::set_networks_changed_callback(
    const NetworksChangedCallback& callback) {
  networks_changed_callback_ = callback;
}

void ChromeosWiMaxDeviceProxy::set_status_changed_callback(
    const StatusChangedCallback& callback) {
  status_changed_callback_ = callback;
}

uint8_t ChromeosWiMaxDeviceProxy::Index(Error* /*error*/) {
  SLOG(&proxy_->GetObjectPath(), 2) << __func__;
  if (!properties_->index.GetAndBlock()) {
    LOG(ERROR) << "Failed to get Index";
    return 0;
  }
  return properties_->index.value();
}

string ChromeosWiMaxDeviceProxy::Name(Error* /*error*/) {
  SLOG(&proxy_->GetObjectPath(), 2) << __func__;
  if (!properties_->name.GetAndBlock()) {
    LOG(ERROR) << "Failed to get Name";
    return string();
  }
  return properties_->name.value();
}

RpcIdentifiers ChromeosWiMaxDeviceProxy::Networks(Error* /*error*/) {
  SLOG(&proxy_->GetObjectPath(), 2) << __func__;
  RpcIdentifiers rpc_networks = KeyValueStore::ConvertPathsToRpcIdentifiers(
      properties_->networks.value());
  return rpc_networks;
}

void ChromeosWiMaxDeviceProxy::OnSignalConnected(
    const string& interface_name, const string& signal_name, bool success) {
  SLOG(&proxy_->GetObjectPath(), 2) << __func__
      << "interface: " << interface_name << " signal: " << signal_name
      << "success: " << success;
  if (!success) {
    LOG(ERROR) << "Failed to connect signal " << signal_name
        << " to interface " << interface_name;
  }
}

void ChromeosWiMaxDeviceProxy::OnPropertyChanged(
    const std::string& property_name) {
  SLOG(&proxy_->GetObjectPath(), 2) << __func__ << ": " << property_name;
}

void ChromeosWiMaxDeviceProxy::NetworksChanged(
    const vector<dbus::ObjectPath>& networks) {
  SLOG(&proxy_->GetObjectPath(), 2) << __func__
      << "(" << networks.size() << ")";
  if (networks_changed_callback_.is_null()) {
    return;
  }
  RpcIdentifiers rpc_networks =
      KeyValueStore::ConvertPathsToRpcIdentifiers(networks);
  networks_changed_callback_.Run(rpc_networks);
}

void ChromeosWiMaxDeviceProxy::StatusChanged(int32_t status) {
  SLOG(&proxy_->GetObjectPath(), 2) << __func__ << "(" << status << ")";
  if (status_changed_callback_.is_null()) {
    return;
  }
  status_changed_callback_.Run(static_cast<DeviceStatus>(status));
}

void ChromeosWiMaxDeviceProxy::OnSuccess(const ResultCallback& callback,
                                         const string& method) {
  SLOG(&proxy_->GetObjectPath(), 2) << __func__ << ": " << method;
  Error error;
  callback.Run(error);
}

void ChromeosWiMaxDeviceProxy::OnFailure(const ResultCallback& callback,
                                         const string& method,
                                         brillo::Error* dbus_error) {
  Error error;
  Error::PopulateAndLog(
      FROM_HERE, &error, Error::kOperationFailed,
      base::StringPrintf("%s failed: %s %s",
                         method.c_str(),
                         dbus_error->GetCode().c_str(),
                         dbus_error->GetMessage().c_str()));
  callback.Run(error);
}

}  // namespace shill
