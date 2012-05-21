// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/wimax.h"

#include <base/bind.h>
#include <base/stringprintf.h>

#include "shill/key_value_store.h"
#include "shill/manager.h"
#include "shill/proxy_factory.h"
#include "shill/scope_logger.h"
#include "shill/wimax_device_proxy_interface.h"
#include "shill/wimax_service.h"

using base::Bind;
using std::map;
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
  DestroyDeadServices(RpcIdentifiers());
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
  SLOG(WiMax, 2) << __func__ << "(" << service->friendly_name() << ")";
  if (pending_service_) {
    Error::PopulateAndLog(
        error, Error::kInProgress,
        base::StringPrintf(
            "Pending connect to %s, ignoring connect request to %s.",
            pending_service_->friendly_name().c_str(),
            service->friendly_name().c_str()));
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

void WiMax::DisconnectFrom(const WiMaxServiceRefPtr &service, Error *error) {
  SLOG(WiMax, 2) << __func__;
  if (pending_service_) {
    Error::PopulateAndLog(
        error, Error::kInProgress,
        base::StringPrintf(
            "Pending connect to %s, ignoring disconnect request from %s.",
            pending_service_->friendly_name().c_str(),
            service->friendly_name().c_str()));
    return;
  }
  if (selected_service() && service != selected_service()) {
    Error::PopulateAndLog(
        error, Error::kNotConnected,
        base::StringPrintf(
            "Curent service is %s, ignoring disconnect request from %s.",
            selected_service()->friendly_name().c_str(),
            service->friendly_name().c_str()));
    return;
  }
  proxy_->Disconnect(
      error, Bind(&WiMax::OnDisconnectComplete, this), kTimeoutDefault);
  if (error->IsFailure()) {
    OnDisconnectComplete(Error());
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
  DestroyIPConfig();
  SelectService(NULL);
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
  DestroyDeadServices(networks);
  for (RpcIdentifiers::const_iterator it = networks.begin();
       it != networks.end(); ++it) {
    CreateService(*it);
  }
}

void WiMax::CreateService(const RpcIdentifier &network) {
  SLOG(WiMax, 2) << __func__ << "(" << network << ")";
  if (ContainsKey(services_, network)) {
    SLOG(WiMax, 2) << "Service already exists.";
    return;
  }
  WiMaxServiceRefPtr service = new WiMaxService(control_interface(),
                                                dispatcher(),
                                                metrics(),
                                                manager(),
                                                this);
  // Creates and passes ownership of the network proxy.
  if (service->Start(proxy_factory_->CreateWiMaxNetworkProxy(network))) {
    manager()->RegisterService(service);
    services_[network] = service;
  } else {
    LOG(ERROR) << "Unable to start service: " << network;
  }
}

void WiMax::DestroyDeadServices(const RpcIdentifiers &live_networks) {
  SLOG(WiMax, 2) << __func__ << "(" << live_networks.size() << ")";
  for (map<string, WiMaxServiceRefPtr>::iterator it = services_.begin();
       it != services_.end(); ) {
    if (find(live_networks.begin(), live_networks.end(), it->first) ==
        live_networks.end()) {
      WiMaxServiceRefPtr service = it->second;
      LOG(INFO) << "Destroying service: " << service->friendly_name();
      if (service == selected_service()) {
        DestroyIPConfig();
        SelectService(NULL);
      }
      if (pending_service_ == service) {
        pending_service_ = NULL;
      }
      manager()->DeregisterService(service);
      services_.erase(it++);
    } else {
      ++it;
    }
  }
}

}  // namespace shill
