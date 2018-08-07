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
#include "bluetooth/newblued/newblue.h"
#include "bluetooth/newblued/stack_sync_monitor.h"

namespace bluetooth {

class NewblueDaemon : public BluetoothDaemon {
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

  // Exports org.bluez.Adapter1 interface on object /org/bluez/hci0.
  // The properties of this object will be ignored by btdispatch, but the object
  // still has to be exposed to be able to receive org.bluez.Adapter1 method
  // calls, e.g. StartDiscovery(), StopDiscovery().
  void ExportAdapterInterface();

  // Installs org.bluez.Adapter1 method handlers.
  void AddAdapterMethodHandlers(ExportedInterface* adapter_interface);

  // D-Bus method handlers for adapter object.
  bool HandleStartDiscovery(brillo::ErrorPtr* error, dbus::Message* message);
  bool HandleStopDiscovery(brillo::ErrorPtr* error, dbus::Message* message);

  // D-Bus method handlers for device objects.
  bool HandlePair(brillo::ErrorPtr* error, dbus::Message* message);
  bool HandleConnect(brillo::ErrorPtr* error, dbus::Message* message);

  // Called when an update of a device info is received.
  void OnDeviceDiscovered(const Device& device);

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

  // Keeps the discovered devices.
  // TODO(sonnysasaka): Clear old devices according to BlueZ mechanism.
  std::map<std::string, Device> discovered_devices_;

  // Must come last so that weak pointers will be invalidated before other
  // members are destroyed.
  base::WeakPtrFactory<NewblueDaemon> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(NewblueDaemon);
};

}  // namespace bluetooth

#endif  // BLUETOOTH_NEWBLUED_NEWBLUE_DAEMON_H_
