// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arc/network/shill_client.h"

#include <vector>

#include <base/bind.h>
#include <base/logging.h>
#include <chromeos/dbus/service_constants.h>

namespace {

// Returns true if the connection state corresponds to a network with full or
// partial connectivity where layer 3 has been provisioned. This includes all
// portal states (Portal, RedirectFound, PortalSuspected), the validated state
// (Online), and intermediary states where portal detection has not started or
// not been conclusive yet (Ready, NoConnectivity).
bool IsConnectedState(const std::string& connection_state) {
  return connection_state == shill::kStateOnline ||
      connection_state == shill::kStateReady ||
      connection_state == shill::kStatePortal ||
      connection_state == shill::kStateNoConnectivity ||
      connection_state == shill::kStateRedirectFound ||
      connection_state == shill::kStatePortalSuspected;
}

std::set<std::string> GetDevices(const brillo::Any& property_value) {
  std::set<std::string> devices;
  for (const auto& path :
       property_value.TryGet<std::vector<dbus::ObjectPath>>()) {
    std::string device = path.value();
    // Strip "/device/" prefix.
    devices.emplace(device.substr(device.find_last_of('/') + 1));
  }
  return devices;
}

}  // namespace

namespace arc_networkd {

ShillClient::ShillClient(scoped_refptr<dbus::Bus> bus) : bus_(bus) {
  manager_proxy_.reset(new org::chromium::flimflam::ManagerProxy(bus_));
  manager_proxy_->RegisterPropertyChangedSignalHandler(
      base::Bind(&ShillClient::OnManagerPropertyChange,
                 weak_factory_.GetWeakPtr()),
      base::Bind(&ShillClient::OnManagerPropertyChangeRegistration,
                 weak_factory_.GetWeakPtr()));
}

void ShillClient::ScanDevices(
    const base::Callback<void(const std::set<std::string>&)>& callback) {
  brillo::VariantDictionary props;
  if (!manager_proxy_->GetProperties(&props, nullptr)) {
    LOG(ERROR) << "Unable to get manager properties";
    return;
  }
  const auto it = props.find(shill::kDevicesProperty);
  if (it == props.end()) {
    LOG(WARNING) << "Manager properties is missing devices";
    return;
  }
  callback.Run(GetDevices(it->second));
}

bool ShillClient::GetDefaultInterface(std::string* name) {
  brillo::VariantDictionary manager_props;

  CHECK(name);
  if (!manager_proxy_->GetProperties(&manager_props, nullptr)) {
    LOG(ERROR) << "Unable to get manager properties";
    return false;
  }

  auto it = manager_props.find(shill::kDefaultServiceProperty);
  if (it == manager_props.end()) {
    LOG(WARNING) << "Manager properties is missing default service";
    return false;
  }

  dbus::ObjectPath service_path = it->second.TryGet<dbus::ObjectPath>();
  if (!service_path.IsValid() || service_path.value() == "/") {
    name->clear();
    return true;
  }

  org::chromium::flimflam::ServiceProxy service_proxy(bus_, service_path);
  brillo::VariantDictionary service_props;
  if (!service_proxy.GetProperties(&service_props, nullptr)) {
    LOG(ERROR) << "Can't retrieve properties for service";
    return false;
  }

  it = service_props.find(shill::kStateProperty);
  if (it == service_props.end()) {
    LOG(WARNING) << "Service properties is missing state";
    return false;
  }
  std::string state = it->second.TryGet<std::string>();
  if (!IsConnectedState(state)) {
    LOG(INFO) << "Ignoring non-connected service in state " << state;
    name->clear();
    return true;
  }

  it = service_props.find(shill::kDeviceProperty);
  if (it == service_props.end()) {
    LOG(WARNING) << "Service properties is missing device path";
    return false;
  }

  dbus::ObjectPath device_path = it->second.TryGet<dbus::ObjectPath>();
  if (!device_path.IsValid()) {
    LOG(WARNING) << "Invalid device path";
    return false;
  }

  org::chromium::flimflam::DeviceProxy device_proxy(bus_, device_path);
  brillo::VariantDictionary device_props;
  if (!device_proxy.GetProperties(&device_props, nullptr)) {
    LOG(ERROR) << "Can't retrieve properties for device";
    return false;
  }

  it = device_props.find(shill::kInterfaceProperty);
  if (it == device_props.end()) {
    LOG(WARNING) << "Device properties is missing interface name";
    return false;
  }

  std::string interface = it->second.TryGet<std::string>();
  if (interface.empty()) {
    LOG(WARNING) << "Device interface name is empty";
    return false;
  }

  *name = interface;
  return true;
}

void ShillClient::OnManagerPropertyChangeRegistration(
    const std::string& interface,
    const std::string& signal_name,
    bool success) {
  if (!success)
    LOG(FATAL) << "Unable to register for interface change events";
}

void ShillClient::OnManagerPropertyChange(const std::string& property_name,
                                          const brillo::Any& property_value) {
  if (property_name == shill::kDevicesProperty &&
      !devices_callback_.is_null()) {
    devices_callback_.Run(GetDevices(property_value));
    return;
  }

  if (property_name != shill::kDefaultServiceProperty &&
      property_name != shill::kConnectionStateProperty)
    return;

  std::string new_default;
  if (!GetDefaultInterface(&new_default))
    new_default.clear();
  if (default_interface_ == new_default)
    return;

  default_interface_ = new_default;
  if (!default_interface_callback_.is_null())
    default_interface_callback_.Run(default_interface_);
}

void ShillClient::RegisterDefaultInterfaceChangedHandler(
    const base::Callback<void(const std::string&)>& callback) {
  default_interface_callback_ = callback;
  if (!GetDefaultInterface(&default_interface_))
    default_interface_.clear();
  default_interface_callback_.Run(default_interface_);
}

void ShillClient::UnregisterDefaultInterfaceChangedHandler() {
  default_interface_callback_.Reset();
}

void ShillClient::RegisterDevicesChangedHandler(
    const base::Callback<void(const std::set<std::string>&)>& callback) {
  devices_callback_ = callback;
}

void ShillClient::UnregisterDevicesChangedHandler() {
  devices_callback_.Reset();
}

}  // namespace arc_networkd
