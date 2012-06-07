// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/wimax.h"

#include <base/bind.h>
#include <base/string_util.h>
#include <base/stringprintf.h>

#include "shill/key_value_store.h"
#include "shill/manager.h"
#include "shill/proxy_factory.h"
#include "shill/scope_logger.h"
#include "shill/wimax_device_proxy_interface.h"
#include "shill/wimax_service.h"

using base::Bind;
using std::set;
using std::string;

namespace shill {

const int WiMax::kTimeoutDefault = 30000;  // milliseconds

WiMax::WiMax(ControlInterface *control,
             EventDispatcher *dispatcher,
             Metrics *metrics,
             Manager *manager,
             const string &link_name,
             const string &address,
             int interface_index,
             const RpcIdentifier &path)
    : Device(control, dispatcher, metrics, manager, link_name, address,
             interface_index, Technology::kWiMax),
      path_(path),
      scanning_(false),
      proxy_factory_(ProxyFactory::GetInstance()) {
  LOG(INFO) << "WiMAX device created: " << link_name << " @ " << path;
  PropertyStore *store = mutable_store();
  store->RegisterConstBool(flimflam::kScanningProperty, &scanning_);
}

WiMax::~WiMax() {
  LOG(INFO) << "WiMAX device destroyed: " << link_name();
}

void WiMax::Start(Error *error, const EnabledStateChangedCallback &callback) {
  SLOG(WiMax, 2) << __func__;
  scanning_ = false;
  proxy_.reset(proxy_factory_->CreateWiMaxDeviceProxy(path_));
  proxy_->set_networks_changed_callback(
      Bind(&WiMax::OnNetworksChanged, Unretained(this)));
  proxy_->set_status_changed_callback(
      Bind(&WiMax::OnStatusChanged, Unretained(this)));
  proxy_->Enable(
      error, Bind(&WiMax::OnEnableComplete, this, callback), kTimeoutDefault);
}

void WiMax::Stop(Error *error, const EnabledStateChangedCallback &callback) {
  SLOG(WiMax, 2) << __func__;
  if (selected_service()) {
    Error error;
    DisconnectFrom(selected_service(), &error);
  }
  scanning_ = false;
  networks_.clear();
  manager()->wimax_provider()->OnNetworksChanged();
  if (proxy_.get()) {
    proxy_->Disable(
        error, Bind(&WiMax::OnDisableComplete, this, callback),
        kTimeoutDefault);
  } else {
    OnDisableComplete(callback, Error());
  }
}

void WiMax::Scan(Error *error) {
  SLOG(WiMax, 2) << __func__;
  if (scanning_) {
    Error::PopulateAndLog(
        error, Error::kInProgress, "Scan already in progress.");
    return;
  }
  scanning_ = true;
  proxy_->ScanNetworks(
      error, Bind(&WiMax::OnScanNetworksComplete, this), kTimeoutDefault);
  if (error->IsFailure()) {
    OnScanNetworksComplete(*error);
  }
}

void WiMax::ConnectTo(const WiMaxServiceRefPtr &service, Error *error) {
  SLOG(WiMax, 2) << __func__ << "(" << service->GetStorageIdentifier() << ")";
  if (pending_service_) {
    Error::PopulateAndLog(
        error, Error::kInProgress,
        base::StringPrintf(
            "Pending connect to %s, ignoring connect request to %s.",
            pending_service_->friendly_name().c_str(),
            service->GetStorageIdentifier().c_str()));
    return;
  }
  service->SetState(Service::kStateAssociating);
  pending_service_ = service;

  KeyValueStore parameters;
  service->GetConnectParameters(&parameters);
  proxy_->Connect(
      service->GetNetworkObjectPath(), parameters,
      error, Bind(&WiMax::OnConnectComplete, this), kTimeoutDefault);
  if (error->IsFailure()) {
    OnConnectComplete(*error);
  }
}

void WiMax::DisconnectFrom(const ServiceRefPtr &service, Error *error) {
  SLOG(WiMax, 2) << __func__;
  if (pending_service_) {
    Error::PopulateAndLog(
        error, Error::kInProgress,
        base::StringPrintf(
            "Pending connect to %s, ignoring disconnect request from %s.",
            pending_service_->friendly_name().c_str(),
            service->GetStorageIdentifier().c_str()));
    return;
  }
  if (selected_service() && service != selected_service()) {
    Error::PopulateAndLog(
        error, Error::kNotConnected,
        base::StringPrintf(
            "Curent service is %s, ignoring disconnect request from %s.",
            selected_service()->friendly_name().c_str(),
            service->GetStorageIdentifier().c_str()));
    return;
  }
  DropConnection();
  proxy_->Disconnect(
      error, Bind(&WiMax::OnDisconnectComplete, this), kTimeoutDefault);
  if (error->IsFailure()) {
    OnDisconnectComplete(*error);
  }
}

void WiMax::OnServiceStopped(const WiMaxServiceRefPtr &service) {
  SLOG(WiMax, 2) << __func__;
  if (service == selected_service()) {
    DropConnection();
  }
  if (service == pending_service_) {
    pending_service_ = NULL;
  }
}

void WiMax::OnDeviceVanished() {
  LOG(INFO) << "WiMAX device vanished: " << link_name();
  proxy_.reset();
  DropService(Service::kStateIdle);
  // Disable the device. This will also clear any relevant properties such as
  // the live network set.
  SetEnabled(false);
}

void WiMax::OnScanNetworksComplete(const Error &/*error*/) {
  SLOG(WiMax, 2) << __func__;
  scanning_ = false;
  // The networks are updated when the NetworksChanged signal is received.
}

void WiMax::OnConnectComplete(const Error &error) {
  SLOG(WiMax, 2) << __func__;
  if (error.IsSuccess()) {
    // Nothing to do -- the connection process is resumed on the StatusChanged
    // signal.
    return;
  }
  DropService(Service::kStateFailure);
}

void WiMax::OnDisconnectComplete(const Error &/*error*/) {
  SLOG(WiMax, 2) << __func__;
}

void WiMax::OnEnableComplete(const EnabledStateChangedCallback &callback,
                             const Error &error) {
  SLOG(WiMax, 2) << __func__;
  if (error.IsFailure()) {
    proxy_.reset();
  } else {
    LOG(INFO) << "WiMAX device " << link_name() << " enabled.";
    // Updates the live networks based on the current WiMaxManager.Device
    // networks. The RPC device will signal when the network set changes.
    Error e;
    OnNetworksChanged(proxy_->Networks(&e));
  }
  callback.Run(error);
}

void WiMax::OnDisableComplete(const EnabledStateChangedCallback &callback,
                              const Error &error) {
  LOG(INFO) << "WiMAX device " << link_name() << " disabled.";
  proxy_.reset();
  callback.Run(error);
}

void WiMax::OnNetworksChanged(const RpcIdentifiers &networks) {
  SLOG(WiMax, 2) << __func__;
  networks_.clear();
  networks_.insert(networks.begin(), networks.end());
  manager()->wimax_provider()->OnNetworksChanged();
}

void WiMax::OnStatusChanged(wimax_manager::DeviceStatus status) {
  SLOG(WiMax, 2) << __func__ << "(" << status << ")";
  switch (status) {
    case wimax_manager::kDeviceStatusConnected:
      if (!pending_service_) {
        LOG(WARNING) << "Unexpected status change; ignored.";
        return;
      }
      if (AcquireIPConfig()) {
        LOG(INFO) << "WiMAX device " << link_name() << " connected to "
                  << pending_service_->GetStorageIdentifier();
        SelectService(pending_service_);
        pending_service_ = NULL;
        SetServiceState(Service::kStateConfiguring);
      } else {
        DropService(Service::kStateFailure);
      }
      break;
    case wimax_manager::kDeviceStatusConnecting:
      // Nothing to do.
      break;
    default:
      if (pending_service_) {
        // For now, assume that failing to connect to a live network indicates
        // bad user credentials. Reset the password to trigger the user/password
        // dialog in the UI.
        pending_service_->ClearPassphrase();
      }
      DropService(Service::kStateFailure);
      break;
  }
}

void WiMax::DropService(Service::ConnectState state) {
  SLOG(WiMax, 2) << __func__
                 << "(" << Service::ConnectStateToString(state) << ")";
  if (pending_service_) {
    LOG(WARNING) << "Unable to initiate connection to: "
                 << pending_service_->GetStorageIdentifier();
    pending_service_->SetState(state);
    pending_service_ = NULL;
  }
  if (selected_service()) {
    LOG(WARNING) << "Service disconnected: "
                 << selected_service()->GetStorageIdentifier();
    selected_service()->SetState(state);
    DropConnection();
  }
}

void WiMax::DropConnection() {
  SLOG(WiMax, 2) << __func__;
  DestroyIPConfig();
  SelectService(NULL);
}

}  // namespace shill
