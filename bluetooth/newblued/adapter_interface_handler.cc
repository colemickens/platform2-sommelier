// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bluetooth/newblued/adapter_interface_handler.h"

#include <memory>
#include <string>

#include <base/stl_util.h>
#include <base/strings/stringprintf.h>
#include <brillo/errors/error.h>
#include <chromeos/dbus/service_constants.h>
#include <dbus/object_path.h>

#include "bluetooth/common/exported_object_manager_wrapper.h"
#include "bluetooth/common/util.h"
#include "bluetooth/newblued/newblue.h"

namespace bluetooth {

AdapterInterfaceHandler::AdapterInterfaceHandler(
    scoped_refptr<dbus::Bus> bus,
    Newblue* newblue,
    ExportedObjectManagerWrapper* exported_object_manager_wrapper)
    : bus_(bus),
      newblue_(newblue),
      exported_object_manager_wrapper_(exported_object_manager_wrapper),
      weak_ptr_factory_(this) {}

void AdapterInterfaceHandler::Init(
    Newblue::DeviceDiscoveredCallback device_discovered_callback) {
  device_discovered_callback_ = device_discovered_callback;
  dbus::ObjectPath adapter_object_path(kAdapterObjectPath);
  exported_object_manager_wrapper_->AddExportedInterface(
      adapter_object_path, bluetooth_adapter::kBluetoothAdapterInterface);
  ExportedInterface* adapter_interface =
      exported_object_manager_wrapper_->GetExportedInterface(
          adapter_object_path, bluetooth_adapter::kBluetoothAdapterInterface);

  // Expose the "Powered" property of the adapter. This property is only
  // controlled by BlueZ, so newblued's "Powered" property is ignored by
  // btdispatch. However, it is useful to have the dummy "Powered" property
  // for testing when Chrome (or any client) connects directly to newblued
  // instead of via btdispatch.
  adapter_interface
      ->EnsureExportedPropertyRegistered<bool>(
          bluetooth_adapter::kPoweredProperty)
      ->SetValue(true);
  adapter_interface
      ->EnsureExportedPropertyRegistered<bool>(
          bluetooth_adapter::kStackSyncQuittingProperty)
      ->SetValue(false);

  adapter_interface->AddSimpleMethodHandlerWithErrorAndMessage(
      bluetooth_adapter::kStartDiscovery, base::Unretained(this),
      &AdapterInterfaceHandler::HandleStartDiscovery);
  adapter_interface->AddSimpleMethodHandlerWithErrorAndMessage(
      bluetooth_adapter::kStopDiscovery, base::Unretained(this),
      &AdapterInterfaceHandler::HandleStopDiscovery);

  adapter_interface->ExportAndBlock();
}

bool AdapterInterfaceHandler::HandleStartDiscovery(brillo::ErrorPtr* error,
                                                   dbus::Message* message) {
  VLOG(1) << __func__;

  const std::string& client_address = message->GetSender();

  if (base::ContainsKey(discovery_clients_, client_address)) {
    brillo::Error::AddTo(
        error, FROM_HERE, brillo::errors::dbus::kDomain,
        bluetooth_adapter::kErrorInProgress,
        base::StringPrintf("Client already has a discovery session: %s",
                           client_address.c_str()));
    return false;
  }

  if (!UpdateDiscovery(discovery_clients_.size() + 1)) {
    brillo::Error::AddTo(error, FROM_HERE, brillo::errors::dbus::kDomain,
                         bluetooth_adapter::kErrorFailed,
                         "Failed to start discovery");
    return false;
  }

  discovery_clients_[client_address] =
      std::make_unique<DBusClient>(bus_, client_address);
  discovery_clients_[client_address]->WatchClientUnavailable(
      base::Bind(&AdapterInterfaceHandler::OnClientUnavailable,
                 weak_ptr_factory_.GetWeakPtr(), client_address));

  return true;
}

bool AdapterInterfaceHandler::HandleStopDiscovery(brillo::ErrorPtr* error,
                                                  dbus::Message* message) {
  VLOG(1) << __func__;

  const std::string& client_address = message->GetSender();

  if (!base::ContainsKey(discovery_clients_, client_address)) {
    brillo::Error::AddTo(
        error, FROM_HERE, brillo::errors::dbus::kDomain,
        bluetooth_adapter::kErrorFailed,
        base::StringPrintf("Client doesn't have a discovery session: %s",
                           client_address.c_str()));
    return false;
  }

  if (!UpdateDiscovery(discovery_clients_.size() - 1)) {
    brillo::Error::AddTo(error, FROM_HERE, brillo::errors::dbus::kDomain,
                         bluetooth_adapter::kErrorFailed,
                         "Failed to stop discovery");
    return false;
  }

  discovery_clients_.erase(client_address);

  return true;
}

bool AdapterInterfaceHandler::UpdateDiscovery(int n_discovery_clients) {
  VLOG(1) << "Updating discovery for would be " << n_discovery_clients
          << " clients.";
  if (n_discovery_clients > 0 && !is_discovering_) {
    // There is at least one client requesting for discovery, and it's not
    // currently discovering.
    VLOG(1) << "Trying to start discovery";
    if (!newblue_->StartDiscovery(device_discovered_callback_)) {
      LOG(ERROR) << "Failed to start discovery";
      return false;
    }
    is_discovering_ = true;
  } else if (n_discovery_clients == 0 && is_discovering_) {
    // There is no client requesting for discovery, and it's currently
    // discovering.
    VLOG(1) << "Trying to stop discovery";
    if (!newblue_->StopDiscovery()) {
      LOG(ERROR) << "Failed to stop discovery";
      return false;
    }
    is_discovering_ = false;
  } else {
    VLOG(1) << "No need to change discovery state";
  }

  return true;
}

void AdapterInterfaceHandler::OnClientUnavailable(
    const std::string& client_address) {
  VLOG(1) << "Discovery client becomes unavailable, address " << client_address;
  discovery_clients_.erase(client_address);
  UpdateDiscovery(discovery_clients_.size());
}

}  // namespace bluetooth
