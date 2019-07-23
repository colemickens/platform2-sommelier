// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bluetooth/dispatcher/dispatcher.h"

#include <utility>

#include <base/logging.h>
#include <base/stl_util.h>
#include <chromeos/dbus/service_constants.h>
#include <dbus/property.h>

#include "bluetooth/dispatcher/bluez_interface_handler.h"
#include "bluetooth/dispatcher/dbus_connection_factory.h"

namespace bluetooth {

Dispatcher::Dispatcher(scoped_refptr<dbus::Bus> bus)
    : bus_(bus),
      client_manager_(std::make_unique<ClientManager>(
          bus, std::make_unique<DBusConnectionFactory>())),
      weak_ptr_factory_(this) {}

Dispatcher::~Dispatcher() = default;

bool Dispatcher::Init(PassthroughMode mode) {
  service_names_.clear();
  // Add BlueZ first before NewBlue. The order matters as the default conflict
  // resolution is to fallback to the first service.
  if (mode != PassthroughMode::NEWBLUE_ONLY) {
    service_names_.push_back(
        bluez_object_manager::kBluezObjectManagerServiceName);
  }
  if (mode != PassthroughMode::BLUEZ_ONLY) {
    service_names_.push_back(
        newblue_object_manager::kNewblueObjectManagerServiceName);
  }

  if (!bus_->RequestOwnershipAndBlock(
          bluetooth_object_manager::kBluetoothObjectManagerServiceName,
          dbus::Bus::REQUIRE_PRIMARY)) {
    LOG(ERROR) << "Failed to acquire D-Bus name ownership";
    return false;
  }

  auto exported_object_manager =
      std::make_unique<brillo::dbus_utils::ExportedObjectManager>(
          bus_,
          dbus::ObjectPath(
              bluetooth_object_manager::kBluetoothObjectManagerServicePath));

  exported_object_manager_wrapper_ =
      std::make_unique<ExportedObjectManagerWrapper>(
          bus_, std::move(exported_object_manager));

  // Convenient temporary variable to hold InterfaceHandler's indexed by its
  // interface name to be registered.
  std::map<std::string, std::unique_ptr<InterfaceHandler>> interface_handlers;

  // Define which interface handler should impersonate which interface name.
  interface_handlers[bluetooth_adapter::kBluetoothAdapterInterface] =
      std::make_unique<BluezAdapterInterfaceHandler>();
  interface_handlers[bluetooth_device::kBluetoothDeviceInterface] =
      std::make_unique<BluezDeviceInterfaceHandler>();
  interface_handlers
      [bluetooth_gatt_characteristic::kBluetoothGattCharacteristicInterface] =
          std::make_unique<BluezGattCharacteristicInterfaceHandler>();
  interface_handlers[bluetooth_input::kBluetoothInputInterface] =
      std::make_unique<BluezInputInterfaceHandler>();
  interface_handlers[bluetooth_media::kBluetoothMediaInterface] =
      std::make_unique<BluezMediaInterfaceHandler>();
  interface_handlers[bluetooth_gatt_service::kBluetoothGattServiceInterface] =
      std::make_unique<BluezGattServiceInterfaceHandler>();
  interface_handlers
      [bluetooth_advertising_manager::kBluetoothAdvertisingManagerInterface] =
          std::make_unique<BluezLeAdvertisingManagerInterfaceHandler>();
  interface_handlers
      [bluetooth_gatt_descriptor::kBluetoothGattDescriptorInterface] =
          std::make_unique<BluezGattDescriptorInterfaceHandler>();
  interface_handlers
      [bluetooth_media_transport::kBluetoothMediaTransportInterface] =
          std::make_unique<BluezMediaTransportInterfaceHandler>();
  interface_handlers[bluetooth_agent_manager::kBluetoothAgentManagerInterface] =
      std::make_unique<BluezAgentManagerInterfaceHandler>();
  interface_handlers
      [bluetooth_profile_manager::kBluetoothProfileManagerInterface] =
          std::make_unique<BluezProfileManagerInterfaceHandler>();
  interface_handlers[bluetooth_plugin_device::kBluetoothPluginInterface] =
      std::make_unique<ChromiumBluetoothDeviceInterfaceHandler>();

  // Register the interfaces.
  for (auto& kv : interface_handlers) {
    std::string interface_name = kv.first;
    auto interface = std::make_unique<ImpersonationObjectManagerInterface>(
        bus_.get(), exported_object_manager_wrapper_.get(),
        std::move(kv.second), interface_name, client_manager_.get());

    for (const std::string& service_name : service_names_) {
      interface->RegisterToObjectManager(
          bus_->GetObjectManager(
              service_name,
              dbus::ObjectPath(bluetooth_object_manager::
                                   kBluetoothObjectManagerServicePath)),
          service_name);
    }

    impersonation_object_manager_interfaces_.emplace(interface_name,
                                                     std::move(interface));
  }

  return true;
}

void Dispatcher::Shutdown() {
  for (const auto& kv : impersonation_object_manager_interfaces_) {
    for (const std::string& service_name : service_names_) {
      bus_->GetObjectManager(
              service_name,
              dbus::ObjectPath(
                  bluetooth_object_manager::kBluetoothObjectManagerServicePath))
          ->UnregisterInterface(kv.first);
    }
  }
  impersonation_object_manager_interfaces_.clear();
  exported_object_manager_wrapper_.reset();
}

}  // namespace bluetooth
