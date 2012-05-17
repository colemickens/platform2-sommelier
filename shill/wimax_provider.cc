// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/wimax_provider.h"

#include <algorithm>

#include <base/bind.h>
#include <base/string_util.h>
#include <chromeos/dbus/service_constants.h>

#include "shill/dbus_properties_proxy_interface.h"
#include "shill/error.h"
#include "shill/manager.h"
#include "shill/proxy_factory.h"
#include "shill/scope_logger.h"
#include "shill/wimax.h"
#include "shill/wimax_manager_proxy_interface.h"

using base::Bind;
using base::Unretained;
using std::find;
using std::map;
using std::string;
using std::vector;

namespace shill {

namespace {
// TODO(petkov): Add these to chromeos/dbus/service_constants.h.
const char kWiMaxDevicePathPrefix[] = "/org/chromium/WiMaxManager/Device/";
const char kWiMaxManagerDevicesProperty[] = "Devices";
}  // namespace

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
  properties_proxy_.reset(
      proxy_factory_->CreateDBusPropertiesProxy(
          wimax_manager::kWiMaxManagerServicePath,
          wimax_manager::kWiMaxManagerServiceName));
  properties_proxy_->set_properties_changed_callback(
      Bind(&WiMaxProvider::OnPropertiesChanged, Unretained(this)));
  manager_proxy_.reset(proxy_factory_->CreateWiMaxManagerProxy());
  Error error;
  UpdateDevices(manager_proxy_->Devices(&error));
}

void WiMaxProvider::Stop() {
  SLOG(WiMax, 2) << __func__;
  properties_proxy_.reset();
  manager_proxy_.reset();
  DestroyDeadDevices(RpcIdentifiers());
}

void WiMaxProvider::UpdateDevices(const RpcIdentifiers &devices) {
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

void WiMaxProvider::OnPropertiesChanged(
    const string &/*interface*/,
    const DBusPropertiesMap &changed_properties,
    const vector<string> &/*invalidated_properties*/) {
  SLOG(WiMax, 2) << __func__;
  RpcIdentifiers devices;
  if (DBusProperties::GetRpcIdentifiers(
          changed_properties, kWiMaxManagerDevicesProperty, &devices)) {
    UpdateDevices(devices);
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
  if (StartsWithASCII(path, kWiMaxDevicePathPrefix, true)) {
    return path.substr(strlen(kWiMaxDevicePathPrefix));
  }
  LOG(ERROR) << "Unable to determine link name from RPC path: " << path;
  return string();
}

}  // namespace shill
