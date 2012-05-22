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
#include "shill/store_interface.h"
#include "shill/wimax_device_proxy_interface.h"
#include "shill/wimax_service.h"

using base::Bind;
using std::set;
using std::string;
using std::vector;

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
  networks_.clear();
  StopDeadServices();
  DestroyAllServices();
  services_.clear();
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
  networks_ = networks;
  StopDeadServices();
  StartLiveServices();
}

void WiMax::StartLiveServices() {
  for (RpcIdentifiers::const_iterator it = networks_.begin();
       it != networks_.end(); ++it) {
    StartLiveServicesForNetwork(*it);
  }
}

void WiMax::StartLiveServicesForNetwork(const RpcIdentifier &network) {
  SLOG(WiMax, 2) << __func__ << "(" << network << ")";
  WiMaxServiceRefPtr default_service = GetDefaultService(network);
  if (!default_service) {
    return;
  }
  // Start services for this live network identifier.
  for (vector<WiMaxServiceRefPtr>::iterator it = services_.begin();
       it != services_.end(); ++it) {
    WiMaxServiceRefPtr service = *it;
    if (service->network_id() != default_service->network_id()) {
      continue;
    }
    if (service->IsStarted()) {
      continue;
    }
    if (service->Start(proxy_factory_->CreateWiMaxNetworkProxy(network))) {
      LOG(INFO) << "WiMAX service started: "
                << service->GetStorageIdentifier();
    } else {
      LOG(ERROR) << "Unable to start service: "
                 << service->GetStorageIdentifier();
    }
  }
}

void WiMax::StopDeadServices() {
  SLOG(WiMax, 2) << __func__ << "(" << networks_.size() << ")";
  for (vector<WiMaxServiceRefPtr>::iterator it = services_.begin();
       it != services_.end(); ++it) {
    WiMaxServiceRefPtr service = *it;
    if (!service->IsStarted()) {
      continue;
    }
    if (find(networks_.begin(), networks_.end(),
             service->GetNetworkObjectPath()) == networks_.end()) {
      LOG(INFO) << "Stopping WiMAX service: "
                << service->GetStorageIdentifier();
      if (service == selected_service()) {
        DestroyIPConfig();
        SelectService(NULL);
      }
      if (pending_service_ == service) {
        pending_service_ = NULL;
      }
      service->Stop();
    }
  }
}

void WiMax::DestroyAllServices() {
  SLOG(WiMax, 2) << __func__;
  while (!services_.empty()) {
    const WiMaxServiceRefPtr &service = services_.back();
    manager()->DeregisterService(service);
    LOG(INFO) << "Deregistered WiMAX service: "
              << service->GetStorageIdentifier();
    services_.pop_back();
  }
}

WiMaxServiceRefPtr WiMax::GetService(const WiMaxNetworkId &id,
                                     const string &name) {
  SLOG(WiMax, 2) << __func__ << "(" << id << ", " << name << ")";
  string storage_id = WiMaxService::CreateStorageIdentifier(id, name);
  WiMaxServiceRefPtr service = FindService(storage_id);
  if (service) {
    SLOG(WiMax, 2) << "Service already exists.";
    return service;
  }
  service = new WiMaxService(control_interface(),
                             dispatcher(),
                             metrics(),
                             manager(),
                             this);
  service->set_network_id(id);
  service->set_friendly_name(name);
  service->InitStorageIdentifier();
  services_.push_back(service);
  manager()->RegisterService(service);
  LOG(INFO) << "Registered WiMAX service: " << service->GetStorageIdentifier();
  return service;
}

WiMaxServiceRefPtr WiMax::GetDefaultService(const RpcIdentifier &network) {
  SLOG(WiMax, 2) << __func__ << "(" << network << ")";
  scoped_ptr<WiMaxNetworkProxyInterface> proxy(
      proxy_factory_->CreateWiMaxNetworkProxy(network));
  Error error;
  uint32 identifier = proxy->Identifier(&error);
  if (error.IsFailure()) {
    return NULL;
  }
  string name = proxy->Name(&error);
  if (error.IsFailure()) {
    return NULL;
  }
  return GetService(WiMaxService::ConvertIdentifierToNetworkId(identifier),
                    name);
}

WiMaxServiceRefPtr WiMax::FindService(const string &storage_id) {
  SLOG(WiMax, 2) << __func__ << "(" << storage_id << ")";
  for (vector<WiMaxServiceRefPtr>::const_iterator it = services_.begin();
       it != services_.end(); ++it) {
    if ((*it)->GetStorageIdentifier() == storage_id) {
      return *it;
    }
  }
  return NULL;
}

bool WiMax::Load(StoreInterface *storage) {
  bool loaded = Device::Load(storage);
  if (LoadServices(storage)) {
    StartLiveServices();
  }
  return loaded;
}

bool WiMax::LoadServices(StoreInterface *storage) {
  SLOG(WiMax, 2) << __func__;
  bool loaded = false;
  set<string> groups = storage->GetGroupsWithKey(Service::kStorageType);
  for (set<string>::iterator it = groups.begin(); it != groups.end(); ++it) {
    string type;
    if (!storage->GetString(*it, Service::kStorageType, &type) ||
        type != GetTechnologyString(NULL)) {
      continue;
    }
    if (FindService(*it)) {
      continue;
    }
    WiMaxNetworkId id;
    if (!storage->GetString(*it, WiMaxService::kStorageNetworkId, &id) ||
        id.empty()) {
      LOG(ERROR) << "Unable to load network id.";
      continue;
    }
    string name;
    if (!storage->GetString(*it, Service::kStorageName, &name) ||
        name.empty()) {
      LOG(ERROR) << "Unable to load service name.";
      continue;
    }
    GetService(id, name);
    loaded = true;
  }
  return loaded;
}

}  // namespace shill
