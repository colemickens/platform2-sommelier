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

const int WiMax::kTimeoutDefault = 30000;

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
  SLOG(WiMax, 2) << __func__ << "(" << link_name << ", " << path << ")";
  PropertyStore *store = mutable_store();
  store->RegisterConstBool(flimflam::kScanningProperty, &scanning_);
}

WiMax::~WiMax() {
  SLOG(WiMax, 2) << __func__ << "(" << link_name() << ", " << path_ << ")";
}

void WiMax::Start(Error *error, const EnabledStateChangedCallback &callback) {
  SLOG(WiMax, 2) << __func__;
  scanning_ = false;
  proxy_.reset(proxy_factory_->CreateWiMaxDeviceProxy(path_));
  proxy_->set_networks_changed_callback(
      Bind(&WiMax::OnNetworksChanged, Unretained(this)));
  proxy_->Enable(
      error, Bind(&WiMax::OnEnableComplete, this, callback), kTimeoutDefault);
}

void WiMax::Stop(Error *error, const EnabledStateChangedCallback &callback) {
  SLOG(WiMax, 2) << __func__;
  if (selected_service()) {
    Error error;
    DisconnectFrom(selected_service(), &error);
  }
  networks_.clear();
  manager()->wimax_provider()->OnNetworksChanged();
  proxy_->Disable(
      error, Bind(&WiMax::OnDisableComplete, this, callback), kTimeoutDefault);
}

bool WiMax::TechnologyIs(const Technology::Identifier type) const {
  return type == Technology::kWiMax;
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
  if (service == selected_service()) {
    DropConnection();
  }
  if (service == pending_service_) {
    pending_service_ = NULL;
  }
}

void WiMax::OnScanNetworksComplete(const Error &/*error*/) {
  SLOG(WiMax, 2) << __func__;
  scanning_ = false;
  // The networks are updated when the NetworksChanged signal is received.
}

void WiMax::OnConnectComplete(const Error &error) {
  SLOG(WiMax, 2) << __func__;
  if (!pending_service_) {
    LOG(ERROR) << "Unexpected OnConnectComplete callback.";
    return;
  }
  if (error.IsSuccess() && AcquireIPConfig()) {
    LOG(INFO) << "Connected to " << pending_service_->friendly_name();
    SelectService(pending_service_);
    SetServiceState(Service::kStateConfiguring);
  } else {
    LOG(ERROR) << "Unable to connect to " << pending_service_->friendly_name();
    pending_service_->SetState(Service::kStateFailure);
  }
  pending_service_ = NULL;
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
    // Scan for networks to allow service creation when the network list becomes
    // available.
    Error e;
    Scan(&e);
  }
  callback.Run(error);
}

void WiMax::OnDisableComplete(const EnabledStateChangedCallback &callback,
                              const Error &error) {
  SLOG(WiMax, 2) << __func__;
  proxy_.reset();
  callback.Run(error);
}

void WiMax::OnNetworksChanged(const RpcIdentifiers &networks) {
  SLOG(WiMax, 2) << __func__;
  networks_.clear();
  networks_.insert(networks.begin(), networks.end());
  manager()->wimax_provider()->OnNetworksChanged();
}

void WiMax::DropConnection() {
  DestroyIPConfig();
  SelectService(NULL);
}

}  // namespace shill
