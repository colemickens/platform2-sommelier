// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bluetooth/dispatcher/dispatcher.h"

#include <utility>

#include <base/logging.h>
#include <chromeos/dbus/service_constants.h>

#include "bluetooth/dispatcher/bluez_interface_handler.h"
#include "bluetooth/dispatcher/dbus_connection_factory.h"

namespace bluetooth {

Dispatcher::Dispatcher(scoped_refptr<dbus::Bus> bus)
    : bus_(bus),
      client_manager_(std::make_unique<ClientManager>(
          bus, std::make_unique<DBusConnectionFactory>())) {}

Dispatcher::~Dispatcher() = default;

bool Dispatcher::Init(PassthroughMode mode) {
  // PassthroughMode::MULTIPLEX fallbacks to PassthroughMode::BLUEZ_ONLY
  // as it's not yet supported.
  // TODO(sonnysasaka): Add support for BlueZ-NewBlue multiplex.
  std::string object_manager_service_name =
      (mode == PassthroughMode::NEWBLUE_ONLY)
          ? newblue_object_manager::kNewblueObjectManagerServiceName
          : bluez_object_manager::kBluezObjectManagerServiceName;
  std::string object_manager_service_path =
      bluetooth_object_manager::kBluetoothObjectManagerServicePath;

  if (!bus_->RequestOwnershipAndBlock(
          bluetooth_object_manager::kBluetoothObjectManagerServiceName,
          dbus::Bus::REQUIRE_PRIMARY)) {
    LOG(ERROR) << "Failed to acquire D-Bus name ownership";
    return false;
  }

  auto exported_object_manager =
      std::make_unique<brillo::dbus_utils::ExportedObjectManager>(
          bus_, dbus::ObjectPath(object_manager_service_path));

  exported_object_manager_wrapper_ =
      std::make_unique<ExportedObjectManagerWrapper>(
          bus_, std::move(exported_object_manager));

  source_object_manager_ =
      bus_->GetObjectManager(object_manager_service_name,
                             dbus::ObjectPath(object_manager_service_path));

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

  // Register the interfaces.
  for (auto& kv : interface_handlers) {
    std::string interface_name = kv.first;
    auto interface = std::make_unique<ImpersonationObjectManagerInterface>(
        bus_.get(), exported_object_manager_wrapper_.get(),
        std::move(kv.second), interface_name, client_manager_.get());
    interface->RegisterToObjectManager(source_object_manager_,
                                       object_manager_service_name);
    impersonation_object_manager_interfaces_.emplace(interface_name,
                                                     std::move(interface));
  }

  return true;
}

void Dispatcher::Shutdown() {
  for (const auto& kv : impersonation_object_manager_interfaces_) {
    source_object_manager_->UnregisterInterface(kv.first);
  }
  impersonation_object_manager_interfaces_.clear();
  exported_object_manager_wrapper_.reset();
}

}  // namespace bluetooth
