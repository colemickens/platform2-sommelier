// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/wimax_provider.h"

#include <algorithm>

#include <base/bind.h>
#include <base/string_util.h>
#include <chromeos/dbus/service_constants.h>

#include "shill/error.h"
#include "shill/key_value_store.h"
#include "shill/manager.h"
#include "shill/proxy_factory.h"
#include "shill/scope_logger.h"
#include "shill/wimax.h"
#include "shill/wimax_manager_proxy_interface.h"
#include "shill/wimax_service.h"

using base::Bind;
using base::Unretained;
using std::find;
using std::map;
using std::string;
using std::vector;

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

WiMaxProvider::~WiMaxProvider() {
  Stop();
}

void WiMaxProvider::Start() {
  SLOG(WiMax, 2) << __func__;
  if (manager_proxy_.get()) {
    return;
  }
  manager_proxy_.reset(proxy_factory_->CreateWiMaxManagerProxy());
  manager_proxy_->set_devices_changed_callback(
      Bind(&WiMaxProvider::OnDevicesChanged, Unretained(this)));
  Error error;
  OnDevicesChanged(manager_proxy_->Devices(&error));
}

void WiMaxProvider::Stop() {
  SLOG(WiMax, 2) << __func__;
  manager_proxy_.reset();
  DestroyDeadDevices(RpcIdentifiers());
}

void WiMaxProvider::OnDevicesChanged(const RpcIdentifiers &devices) {
  SLOG(WiMax, 2) << __func__;
  DestroyDeadDevices(devices);
  for (RpcIdentifiers::const_iterator it = devices.begin();
       it != devices.end(); ++it) {
    string link_name = GetLinkName(*it);
    if (!link_name.empty()) {
      CreateDevice(link_name, *it);
    }
  }
}

void WiMaxProvider::OnDeviceInfoAvailable(const string &link_name) {
  SLOG(WiMax, 2) << __func__ << "(" << link_name << ")";
  map<string, string>::iterator find_it = pending_devices_.find(link_name);
  if (find_it != pending_devices_.end()) {
    RpcIdentifier path = find_it->second;
    CreateDevice(link_name, path);
  }
}

WiMaxServiceRefPtr WiMaxProvider::GetService(const KeyValueStore &args,
                                             Error *error) {
  SLOG(WiMax, 2) << __func__;
  CHECK_EQ(args.GetString(flimflam::kTypeProperty), flimflam::kTypeWimax);
  if (devices_.empty()) {
    Error::PopulateAndLog(
        error, Error::kNotSupported, "No WiMAX device available.");
    return NULL;
  }
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
  // Use the first available WiMAX device to construct and register the
  // service. We may need to consider alternatives if this becomes an issue. For
  // example, we could refactor service creation and management out of WiMax
  // into WiMaxProvider.
  WiMaxRefPtr device = devices_.begin()->second;
  WiMaxServiceRefPtr service = device->GetService(id, name);
  CHECK(service);
  // Configure the service using the the rest of the passed-in arguments.
  service->Configure(args, error);
  // Start the service if there's a matching live network.
  device->StartLiveServices();
  return service;
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
    LOG(INFO) << "Do not create WiMax device for blacklisted interface "
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
    LOG(ERROR) << "Unable to create a WiMax device with no MAC address: "
               << link_name;
    return;
  }
  string address = address_bytes.HexEncode();
  WiMaxRefPtr device(new WiMax(control_, dispatcher_, metrics_, manager_,
                               link_name, address, index, path));
  devices_[link_name] = device;
  device_info->RegisterDevice(device);
}

void WiMaxProvider::DestroyDeadDevices(const RpcIdentifiers &live_devices) {
  SLOG(WiMax, 2) << __func__ << "(" << live_devices.size() << ")";
  for (map<string, string>::iterator it = pending_devices_.begin();
       it != pending_devices_.end(); ) {
    if (find(live_devices.begin(), live_devices.end(), it->second) ==
        live_devices.end()) {
      SLOG(WiMax, 2) << "Forgetting pending device: " << it->second;
      pending_devices_.erase(it++);
    } else {
      ++it;
    }
  }
  for (map<string, WiMaxRefPtr>::iterator it = devices_.begin();
       it != devices_.end(); ) {
    if (find(live_devices.begin(), live_devices.end(), it->second->path()) ==
        live_devices.end()) {
      SLOG(WiMax, 2) << "Destroying device: " << it->first;
      manager_->device_info()->DeregisterDevice(it->second);
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

}  // namespace shill
