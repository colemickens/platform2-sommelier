// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLUETOOTH_NEWBLUED_ADAPTER_INTERFACE_HANDLER_H_
#define BLUETOOTH_NEWBLUED_ADAPTER_INTERFACE_HANDLER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include <base/macros.h>
#include <base/memory/ref_counted.h>
#include <brillo/errors/error.h>
#include <dbus/bus.h>
#include <dbus/message.h>

#include "bluetooth/common/dbus_client.h"
#include "bluetooth/common/exported_object_manager_wrapper.h"
#include "bluetooth/newblued/device_interface_handler.h"
#include "bluetooth/newblued/newblue.h"

namespace bluetooth {

// Handles org.bluez.Adapter1 interface.
class AdapterInterfaceHandler {
 public:
  // |newblue| and |exported_object_manager_wrapper| not owned, caller must make
  // sure it outlives this object.
  AdapterInterfaceHandler(
      scoped_refptr<dbus::Bus> bus,
      Newblue* newblue,
      ExportedObjectManagerWrapper* exported_object_manager_wrapper);
  virtual ~AdapterInterfaceHandler() = default;

  // Starts exposing org.bluez.Adapter1 interface on object /org/bluez/hci0.
  // The properties of this object will be ignored by btdispatch, but the object
  // still has to be exposed to be able to receive org.bluez.Adapter1 method
  // calls, e.g. StartDiscovery(), StopDiscovery().
  void Init(DeviceInterfaceHandler* device_interface_handler);

 private:
  // D-Bus method handlers for adapter object.
  bool HandleStartDiscovery(brillo::ErrorPtr* error, dbus::Message* message);
  bool HandleStopDiscovery(brillo::ErrorPtr* error, dbus::Message* message);
  bool HandleRemoveDevice(brillo::ErrorPtr* error,
                          dbus::Message* message,
                          const dbus::ObjectPath& device_path);

  bool UpdateDiscovery(int n_discovery_clients);

  // Changes the state of background scan. If true, background scan will be
  // active even though there is no client requesting discovery.
  void SetBackgroundScanEnable(bool enabled);

  // Called when an update of a device info is received.
  void DeviceDiscoveryCallback(const std::string& address,
                               uint8_t address_type,
                               int8_t rssi,
                               uint8_t reply_type,
                               const std::vector<uint8_t>& eir);

  // Called when a client is disconnected from D-Bus.
  void OnClientUnavailable(const std::string& client_address);

  scoped_refptr<dbus::Bus> bus_;

  Newblue* newblue_;

  DeviceInterfaceHandler* device_interface_handler_;

  ExportedObjectManagerWrapper* exported_object_manager_wrapper_;

  Newblue::DeviceDiscoveredCallback device_discovered_callback_;

  // Clients which currently have active discovery mapped by its D-Bus address.
  // (D-Bus address -> DBusClient object).
  std::map<std::string, std::unique_ptr<DBusClient>> discovery_clients_;

  bool is_background_scan_enabled_ = false;

  bool is_discovering_ = false;

  // Must come last so that weak pointers will be invalidated before other
  // members are destroyed.
  base::WeakPtrFactory<AdapterInterfaceHandler> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(AdapterInterfaceHandler);
};

}  // namespace bluetooth

#endif  // BLUETOOTH_NEWBLUED_ADAPTER_INTERFACE_HANDLER_H_
