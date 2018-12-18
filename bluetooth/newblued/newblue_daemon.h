// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLUETOOTH_NEWBLUED_NEWBLUE_DAEMON_H_
#define BLUETOOTH_NEWBLUED_NEWBLUE_DAEMON_H_

#include <map>
#include <memory>
#include <string>

#include <base/memory/ref_counted.h>
#include <base/memory/weak_ptr.h>
#include <brillo/dbus/exported_object_manager.h>
#include <dbus/bus.h>

#include "bluetooth/common/bluetooth_daemon.h"
#include "bluetooth/common/exported_object_manager_wrapper.h"
#include "bluetooth/common/util.h"
#include "bluetooth/newblued/adapter_interface_handler.h"
#include "bluetooth/newblued/agent_manager_interface_handler.h"
#include "bluetooth/newblued/newblue.h"
#include "bluetooth/newblued/stack_sync_monitor.h"

namespace bluetooth {

class NewblueDaemon : public BluetoothDaemon {
  // Represents a pairing session.
  struct PairSession {
    std::string address;
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<>> pair_response;
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<>>
        cancel_pair_response;
  };

 public:
  explicit NewblueDaemon(std::unique_ptr<Newblue> newblue);
  ~NewblueDaemon() override = default;

  // BluetoothDaemon override:
  bool Init(scoped_refptr<dbus::Bus> bus, DBusDaemon* dbus_daemon) override;

  // Frees up all resources. Currently only needed in test.
  void Shutdown();

  // Called when NewBlue is ready to be brought up.
  void OnHciReadyForUp();

 private:
  // Registers GetAll/Get/Set method handlers.
  void SetupPropertyMethodHandlers(
      brillo::dbus_utils::DBusInterface* prop_interface,
      brillo::dbus_utils::ExportedPropertySet* property_set);

  // Installs org.bluez.Device1 method handlers.
  void AddDeviceMethodHandlers(ExportedInterface* device_interface);

  // D-Bus method handlers for device objects.
  void HandlePair(
      std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<>> response,
      dbus::Message* message);
  void HandleCancelPairing(
      std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<>> response,
      dbus::Message* message);
  void HandleConnect(
      std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<>> response,
      dbus::Message* message);
  // TODO(mcchou): Handle the rest of the D-Bus methods of the device interface.
  // Connect()
  // Disconnect()
  // ConnectProfile() - No op, but we may need dummy implementation later.
  // DisconnectPorfile() - No op, but we may need dummy implementation later.
  // GetServiceRecords() - No op, but we may need dummy implementation later.
  // ExecuteWrite()

  // Called when an update of a device info is received.
  void OnDeviceDiscovered(const Device& device);

  // Called when a pairing state changed event is received.
  void OnPairStateChanged(const Device& device,
                          PairState pair_state,
                          const std::string& dbus_error);

  // Exposes or updates the device object's property depends on the whether it
  // was exposed before or should be forced updated.
  template <typename T>
  void UpdateDeviceProperty(ExportedInterface* interface,
                            const std::string& property_name,
                            const Property<T>& property,
                            bool force_export) {
    if (force_export || property.updated()) {
      interface->EnsureExportedPropertyRegistered<T>(property_name)
          ->SetValue(property.value());
    }
  }

  // Exposes or updates the device object's property depends on the whether it
  // was exposed before or should be forced updated. Takes a converter function
  // which converts the value of a property into the value for exposing.
  template <typename T, typename U>
  void UpdateDeviceProperty(ExportedInterface* interface,
                            const std::string& property_name,
                            const Property<U>& property,
                            T (*converter)(const U&),
                            bool force_export) {
    if (force_export || property.updated()) {
      interface->EnsureExportedPropertyRegistered<T>(property_name)
          ->SetValue(converter(property.value()));
    }
  }

  // Exposes all mandatory device object's properties and update the properties
  // for the existing devices by either exposing them if not exposed before or
  // emitting the value changes if any.
  void UpdateDeviceProperties(ExportedInterface* interface,
                              const Device& device,
                              bool is_new_device);

  void OnBluezDown();

  scoped_refptr<dbus::Bus> bus_;

  std::unique_ptr<ExportedObjectManagerWrapper>
      exported_object_manager_wrapper_;

  std::unique_ptr<Newblue> newblue_;

  DBusDaemon* dbus_daemon_;

  StackSyncMonitor stack_sync_monitor_;

  std::unique_ptr<AdapterInterfaceHandler> adapter_interface_handler_;

  std::unique_ptr<AgentManagerInterfaceHandler>
      agent_manager_interface_handler_;

  // Keeps the discovered devices.
  // TODO(sonnysasaka): Clear old devices according to BlueZ mechanism.
  std::map<std::string, Device> discovered_devices_;

  UniqueId pair_observer_id_;

  // Device object path and its response to the ongoing pairing/cancelpairing
  // request. <device address, D-Bus method response to pairing, D-Bus
  // method response to cancel pairing>
  struct PairSession ongoing_pairing_;

  // Must come last so that weak pointers will be invalidated before other
  // members are destroyed.
  base::WeakPtrFactory<NewblueDaemon> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(NewblueDaemon);
};

}  // namespace bluetooth

#endif  // BLUETOOTH_NEWBLUED_NEWBLUE_DAEMON_H_
