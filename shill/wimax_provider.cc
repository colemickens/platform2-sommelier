// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/wimax_provider.h"

#include <algorithm>
#include <set>

#include <base/bind.h>
#include <base/string_util.h>
#include <chromeos/dbus/service_constants.h>

#include "shill/error.h"
#include "shill/key_value_store.h"
#include "shill/manager.h"
#include "shill/profile.h"
#include "shill/proxy_factory.h"
#include "shill/scope_logger.h"
#include "shill/store_interface.h"
#include "shill/wimax.h"
#include "shill/wimax_manager_proxy_interface.h"
#include "shill/wimax_service.h"

using base::Bind;
using base::Unretained;
using std::find;
using std::map;
using std::set;
using std::string;

namespace shill {

WiMaxProvider::WiMaxProvider(ControlInterface *control,
                             EventDispatcher *dispatcher,
                             Metrics *metrics,
                             Manager *manager)
    : control_(control),
      dispatcher_(dispatcher),
      metrics_(metrics),
      manager_(manager),
      proxy_factory_(ProxyFactory::GetInstance()) {}

WiMaxProvider::~WiMaxProvider() {}

void WiMaxProvider::Start() {
  SLOG(WiMax, 2) << __func__;
  if (!on_wimax_manager_appear_.IsCancelled()) {
    return;
  }
  // Registers a watcher for the WiMaxManager service. This provider will
  // connect to it if/when the OnWiMaxManagerAppear callback is invoked.
  on_wimax_manager_appear_.Reset(
      Bind(&WiMaxProvider::OnWiMaxManagerAppear, Unretained(this)));
  on_wimax_manager_vanish_.Reset(
      Bind(&WiMaxProvider::DisconnectFromWiMaxManager, Unretained(this)));
  manager_->dbus_manager()->WatchName(
      wimax_manager::kWiMaxManagerServiceName,
      on_wimax_manager_appear_.callback(),
      on_wimax_manager_vanish_.callback());
}

void WiMaxProvider::Stop() {
  SLOG(WiMax, 2) << __func__;
  // TODO(petkov): Deregister the watcher from DBusManager to avoid potential
  // memory leaks (crosbug.com/32226).
  on_wimax_manager_appear_.Cancel();
  on_wimax_manager_vanish_.Cancel();
  DisconnectFromWiMaxManager();
  DestroyAllServices();
}

void WiMaxProvider::ConnectToWiMaxManager() {
  DCHECK(!wimax_manager_proxy_.get());
  LOG(INFO) << "Connecting to WiMaxManager.";
  wimax_manager_proxy_.reset(proxy_factory_->CreateWiMaxManagerProxy());
  wimax_manager_proxy_->set_devices_changed_callback(
      Bind(&WiMaxProvider::OnDevicesChanged, Unretained(this)));
  Error error;
  OnDevicesChanged(wimax_manager_proxy_->Devices(&error));
}

void WiMaxProvider::DisconnectFromWiMaxManager() {
  SLOG(WiMax, 2) << __func__;
  if (!wimax_manager_proxy_.get()) {
    return;
  }
  LOG(INFO) << "Disconnecting from WiMaxManager.";
  wimax_manager_proxy_.reset();
  OnDevicesChanged(RpcIdentifiers());
}

void WiMaxProvider::OnWiMaxManagerAppear(const string &owner) {
  SLOG(WiMax, 2) << __func__ << "(" << owner << ")";
  DisconnectFromWiMaxManager();
  ConnectToWiMaxManager();
}

void WiMaxProvider::OnDeviceInfoAvailable(const string &link_name) {
  SLOG(WiMax, 2) << __func__ << "(" << link_name << ")";
  map<string, RpcIdentifier>::const_iterator find_it =
      pending_devices_.find(link_name);
  if (find_it != pending_devices_.end()) {
    RpcIdentifier path = find_it->second;
    CreateDevice(link_name, path);
  }
}

void WiMaxProvider::OnNetworksChanged() {
  SLOG(WiMax, 2) << __func__;
  // Collects a set of live networks from all devices.
  set<RpcIdentifier> live_networks;
  for (map<string, WiMaxRefPtr>::const_iterator it = devices_.begin();
       it != devices_.end(); ++it) {
    const set<RpcIdentifier> &networks = it->second->networks();
    live_networks.insert(networks.begin(), networks.end());
  }
  // Removes dead networks from |networks_|.
  for (map<RpcIdentifier, NetworkInfo>::iterator it = networks_.begin();
       it != networks_.end(); ) {
    const RpcIdentifier &path = it->first;
    if (ContainsKey(live_networks, path)) {
      ++it;
    } else {
      LOG(INFO) << "WiMAX network disappeared: " << path;
      networks_.erase(it++);
    }
  }
  // Retrieves network info into |networks_| for the live networks.
  for (set<RpcIdentifier>::const_iterator it = live_networks.begin();
       it != live_networks.end(); ++it) {
    RetrieveNetworkInfo(*it);
  }
  // Stops dead and starts live services based on the current |networks_|.
  StopDeadServices();
  StartLiveServices();
}

bool WiMaxProvider::OnServiceUnloaded(const WiMaxServiceRefPtr &service) {
  SLOG(WiMax, 2) << __func__ << "(" << service->GetStorageIdentifier() << ")";
  if (service->is_default()) {
    return false;
  }
  // Removes the service from the managed service set. The service will be
  // deregistered from Manager when we release ownership by returning true.
  services_.erase(service->GetStorageIdentifier());
  return true;
}

WiMaxServiceRefPtr WiMaxProvider::GetService(const KeyValueStore &args,
                                             Error *error) {
  SLOG(WiMax, 2) << __func__;
  CHECK_EQ(args.GetString(flimflam::kTypeProperty), flimflam::kTypeWimax);
  WiMaxNetworkId id = args.LookupString(WiMaxService::kNetworkIdProperty, "");
  if (id.empty()) {
    Error::PopulateAndLog(
        error, Error::kInvalidArguments, "Missing WiMAX network id.");
    return NULL;
  }
  string name = args.LookupString(flimflam::kNameProperty, "");
  if (name.empty()) {
    Error::PopulateAndLog(
        error, Error::kInvalidArguments, "Missing WiMAX service name.");
    return NULL;
  }
  WiMaxServiceRefPtr service = GetUniqueService(id, name);
  CHECK(service);
  // Configures the service using the rest of the passed-in arguments.
  service->Configure(args, error);
  // Starts the service if there's a matching live network.
  StartLiveServices();
  return service;
}

void WiMaxProvider::CreateServicesFromProfile(const ProfileRefPtr &profile) {
  SLOG(WiMax, 2) << __func__;
  bool created = false;
  const StoreInterface *storage = profile->GetConstStorage();
  set<string> groups = storage->GetGroupsWithKey(Service::kStorageType);
  for (set<string>::const_iterator it = groups.begin();
       it != groups.end(); ++it) {
    const string &storage_id = *it;
    string type;
    if (!storage->GetString(storage_id, Service::kStorageType, &type) ||
        type != Technology::NameFromIdentifier(Technology::kWiMax)) {
      continue;
    }
    if (FindService(storage_id)) {
      continue;
    }
    WiMaxNetworkId id;
    if (!storage->GetString(storage_id, WiMaxService::kStorageNetworkId, &id) ||
        id.empty()) {
      LOG(ERROR) << "Unable to load network id: " << storage_id;
      continue;
    }
    string name;
    if (!storage->GetString(storage_id, Service::kStorageName, &name) ||
        name.empty()) {
      LOG(ERROR) << "Unable to load service name: " << storage_id;
      continue;
    }
    WiMaxServiceRefPtr service = GetUniqueService(id, name);
    CHECK(service);
    if (!profile->ConfigureService(service)) {
      LOG(ERROR) << "Could not configure service: " << storage_id;
    }
    created = true;
  }
  if (created) {
    StartLiveServices();
  }
}

WiMaxRefPtr WiMaxProvider::SelectCarrier(const WiMaxServiceRefPtr &service) {
  SLOG(WiMax, 2) << __func__ << "(" << service->GetStorageIdentifier() << ")";
  if (devices_.empty()) {
    LOG(ERROR) << "No WiMAX devices available.";
    return NULL;
  }
  // TODO(petkov): For now, just return the first available device. We need to
  // be smarter here and select a device that sees |service|'s network.
  return devices_.begin()->second;
}

void WiMaxProvider::OnDevicesChanged(const RpcIdentifiers &devices) {
  SLOG(WiMax, 2) << __func__;
  DestroyDeadDevices(devices);
  for (RpcIdentifiers::const_iterator it = devices.begin();
       it != devices.end(); ++it) {
    const RpcIdentifier &path = *it;
    string link_name = GetLinkName(path);
    if (!link_name.empty()) {
      CreateDevice(link_name, path);
    }
  }
}

void WiMaxProvider::CreateDevice(const string &link_name,
                                 const RpcIdentifier &path) {
  SLOG(WiMax, 2) << __func__ << "(" << link_name << ", " << path << ")";
  if (ContainsKey(devices_, link_name)) {
    SLOG(WiMax, 2) << "Device already exists.";
    CHECK_EQ(path, devices_[link_name]->path());
    return;
  }
  pending_devices_.erase(link_name);
  DeviceInfo *device_info = manager_->device_info();
  if (device_info->IsDeviceBlackListed(link_name)) {
    LOG(INFO) << "WiMAX device not created, interface blacklisted: "
              << link_name;
    return;
  }
  int index = device_info->GetIndex(link_name);
  if (index < 0) {
    SLOG(WiMax, 2) << link_name << " pending device info.";
    // Adds the link to the pending device map, waiting for a notification from
    // DeviceInfo that it's received information about the device from RTNL.
    pending_devices_[link_name] = path;
    return;
  }
  ByteString address_bytes;
  if (!device_info->GetMACAddress(index, &address_bytes)) {
    LOG(ERROR) << "Unable to create a WiMAX device with no MAC address: "
               << link_name;
    return;
  }
  string address = address_bytes.HexEncode();
  WiMaxRefPtr device(new WiMax(control_, dispatcher_, metrics_, manager_,
                               link_name, address, index, path));
  devices_[link_name] = device;
  device_info->RegisterDevice(device);
  LOG(INFO) << "Created WiMAX device: " << link_name << " @ " << path;
}

void WiMaxProvider::DestroyDeadDevices(const RpcIdentifiers &live_devices) {
  SLOG(WiMax, 2) << __func__ << "(" << live_devices.size() << ")";
  for (map<string, RpcIdentifier>::iterator it = pending_devices_.begin();
       it != pending_devices_.end(); ) {
    if (find(live_devices.begin(), live_devices.end(), it->second) ==
        live_devices.end()) {
      LOG(INFO) << "Forgetting pending device: " << it->second;
      pending_devices_.erase(it++);
    } else {
      ++it;
    }
  }
  for (map<string, WiMaxRefPtr>::iterator it = devices_.begin();
       it != devices_.end(); ) {
    if (find(live_devices.begin(), live_devices.end(), it->second->path()) ==
        live_devices.end()) {
      LOG(INFO) << "Destroying device: " << it->first;
      const WiMaxRefPtr &device = it->second;
      device->OnDeviceVanished();
      manager_->device_info()->DeregisterDevice(device);
      devices_.erase(it++);
    } else {
      ++it;
    }
  }
}

string WiMaxProvider::GetLinkName(const RpcIdentifier &path) {
  if (StartsWithASCII(path, wimax_manager::kDeviceObjectPathPrefix, true)) {
    return path.substr(strlen(wimax_manager::kDeviceObjectPathPrefix));
  }
  LOG(ERROR) << "Unable to determine link name from RPC path: " << path;
  return string();
}

void WiMaxProvider::RetrieveNetworkInfo(const RpcIdentifier &path) {
  if (ContainsKey(networks_, path)) {
    // Nothing to do, the network info is already available.
    return;
  }
  LOG(INFO) << "WiMAX network appeared: " << path;
  scoped_ptr<WiMaxNetworkProxyInterface> proxy(
      proxy_factory_->CreateWiMaxNetworkProxy(path));
  Error error;
  NetworkInfo info;
  info.name = proxy->Name(&error);
  if (error.IsFailure()) {
    return;
  }
  uint32 identifier = proxy->Identifier(&error);
  if (error.IsFailure()) {
    return;
  }
  info.id = WiMaxService::ConvertIdentifierToNetworkId(identifier);
  networks_[path] = info;
}

WiMaxServiceRefPtr WiMaxProvider::FindService(const string &storage_id) {
  SLOG(WiMax, 2) << __func__ << "(" << storage_id << ")";
  map<string, WiMaxServiceRefPtr>::const_iterator find_it =
      services_.find(storage_id);
  if (find_it == services_.end()) {
    return NULL;
  }
  const WiMaxServiceRefPtr &service = find_it->second;
  LOG_IF(ERROR, storage_id != service->GetStorageIdentifier());
  return service;
}

WiMaxServiceRefPtr WiMaxProvider::GetUniqueService(const WiMaxNetworkId &id,
                                                   const string &name) {
  SLOG(WiMax, 2) << __func__ << "(" << id << ", " << name << ")";
  string storage_id = WiMaxService::CreateStorageIdentifier(id, name);
  WiMaxServiceRefPtr service = FindService(storage_id);
  if (service) {
    SLOG(WiMax, 2) << "Service already exists.";
    return service;
  }
  service = new WiMaxService(control_, dispatcher_, metrics_, manager_);
  service->set_network_id(id);
  service->set_friendly_name(name);
  service->InitStorageIdentifier();
  services_[service->GetStorageIdentifier()] = service;
  manager_->RegisterService(service);
  LOG(INFO) << "Registered WiMAX service: " << service->GetStorageIdentifier();
  return service;
}

void WiMaxProvider::StartLiveServices() {
  SLOG(WiMax, 2) << __func__ << "(" << networks_.size() << ")";
  for (map<RpcIdentifier, NetworkInfo>::const_iterator nit = networks_.begin();
       nit != networks_.end(); ++nit) {
    const RpcIdentifier &path = nit->first;
    const NetworkInfo &info = nit->second;

    // Creates the default service for the network, if not already created.
    GetUniqueService(info.id, info.name)->set_is_default(true);

    // Starts services for this live network.
    for (map<string, WiMaxServiceRefPtr>::const_iterator it = services_.begin();
         it != services_.end(); ++it) {
      const WiMaxServiceRefPtr &service = it->second;
      if (service->network_id() != info.id || service->IsStarted()) {
        continue;
      }
      if (!service->Start(proxy_factory_->CreateWiMaxNetworkProxy(path))) {
        LOG(ERROR) << "Unable to start service: "
                   << service->GetStorageIdentifier();
      }
    }
  }
}

void WiMaxProvider::StopDeadServices() {
  SLOG(WiMax, 2) << __func__ << "(" << networks_.size() << ")";
  for (map<string, WiMaxServiceRefPtr>::iterator it = services_.begin();
       it != services_.end(); ) {
    // Keeps a local reference until we're done with this service.
    WiMaxServiceRefPtr service = it->second;
    if (service->IsStarted() &&
        !ContainsKey(networks_, service->GetNetworkObjectPath())) {
      service->Stop();
      // Default services are created and registered when a network becomes
      // live. They need to be deregistered and destroyed when the network
      // disappears.
      if (service->is_default()) {
        // Removes |service| from the managed service set before deregistering
        // it from Manager to ensure correct iterator increment (consider
        // Manager::DeregisterService -> WiMaxService::Unload ->
        // WiMaxProvider::OnServiceUnloaded -> services_.erase).
        services_.erase(it++);
        manager_->DeregisterService(service);
        continue;
      }
    }
    ++it;
  }
}

void WiMaxProvider::DestroyAllServices() {
  SLOG(WiMax, 2) << __func__;
  while (!services_.empty()) {
    // Keeps a local reference until we're done with this service.
    WiMaxServiceRefPtr service = services_.begin()->second;
    services_.erase(services_.begin());
    // Stops the service so that it can notify its carrier device, if any.
    service->Stop();
    manager_->DeregisterService(service);
    LOG(INFO) << "Deregistered WiMAX service: "
              << service->GetStorageIdentifier();
  }
}

}  // namespace shill
