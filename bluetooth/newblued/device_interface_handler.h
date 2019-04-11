// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLUETOOTH_NEWBLUED_DEVICE_INTERFACE_HANDLER_H_
#define BLUETOOTH_NEWBLUED_DEVICE_INTERFACE_HANDLER_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include <base/memory/ref_counted.h>
#include <base/memory/weak_ptr.h>
#include <dbus/bus.h>

#include "bluetooth/common/exported_object_manager_wrapper.h"
#include "bluetooth/newblued/newblue.h"
#include "bluetooth/newblued/property.h"
#include "bluetooth/newblued/uuid.h"

namespace bluetooth {

// Structure representing a discovered device.
struct Device {
  Device();
  explicit Device(const std::string& address);

  // MAC address (in format XX:XX:XX:XX:XX:XX).
  std::string address;
  // Whether the MAC address is a random address.
  bool is_random_address;

  // [mandatory] Whether the device is paired.
  Property<bool> paired;
  // [mandatory] Whether the device is connected.
  Property<bool> connected;
  // [mandatory] Whether the device is in the white list.
  Property<bool> trusted;
  // [mandatory] Whether the device is in the black list.
  Property<bool> blocked;
  // [mandatory] Whether the services provided by the device has been resolved.
  Property<bool> services_resolved;

  // [mandatory] A readable and writable alias given to the device.
  Property<std::string> alias;
  // Actual alias provided by the user.
  std::string internal_alias;
  // [optional] A readable name of the device.
  Property<std::string> name;

  // [optional] Transmission power level of the advertisement packet
  Property<int16_t> tx_power;
  // [optional] RSSI of last received inquiry response.
  Property<int16_t> rssi;

  // [optional] Class of the device.
  Property<uint32_t> eir_class;
  // [optional] External appearance of the device.
  Property<uint16_t> appearance;
  // [optional] Icon type of the device based on the value of |appearance|.
  Property<std::string> icon;

  // [optional] Advertising flags.
  Property<std::vector<uint8_t>> flags;
  // [optional] Service UUIDs of 16-bit 32-bit and 128-bit.
  Property<std::set<Uuid>> service_uuids;
  // [optional] Service data associated with UUIDs.
  Property<std::map<Uuid, std::vector<uint8_t>>> service_data;

  // [optional] Manufacturer identifier with the extra manufacturer data
  Property<std::map<uint16_t, std::vector<uint8_t>>> manufacturer;

  DISALLOW_COPY_AND_ASSIGN(Device);
};

// These are based on the connection state defined in newblue/gatt.h.
enum class ConnectState : uint8_t {
  CONNECTED,
  DISCONNECTED,
  ERROR,
  DISCONNECTED_BY_US,
};

// Handles org.bluez.Device1 interface.
class DeviceInterfaceHandler {
  // Represents a pairing session.
  struct PairSession {
    std::string address;
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<>> pair_response;
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<>>
        cancel_pair_response;
  };

  // Represents a connection session.
  struct ConnectSession {
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<>> connect_response;
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<>>
        disconnect_response;
  };

  // Structure representing a connection.
  struct Connection {
    gatt_client_conn_t conn_id;
    ble_hid_conn_t hid_id;  // This can be invalid if not a HID device.
  };

 public:
  using ConnectCallback = base::Callback<void(const std::string& device_address,
                                              bool success,
                                              const std::string& dbus_error)>;

  // |newblue| and |exported_object_manager_wrapper| not owned, caller must make
  // sure it outlives this object.
  DeviceInterfaceHandler(
      scoped_refptr<dbus::Bus> bus,
      Newblue* newblue,
      ExportedObjectManagerWrapper* exported_object_manager_wrapper);
  virtual ~DeviceInterfaceHandler() = default;

  // Starts listening pair state events from Newblue.
  bool Init();

  // Returns weak pointer of this object.
  base::WeakPtr<DeviceInterfaceHandler> GetWeakPtr();

  // Called when an update of a device info is received.
  // |scanned_by_client| being true means that the scan result is due to scan
  // requested by clients rather than a background scan.
  void OnDeviceDiscovered(bool scanned_by_client,
                          const std::string& address,
                          uint8_t address_type,
                          int8_t rssi,
                          uint8_t reply_type,
                          const std::vector<uint8_t>& eir);

  // Removes a device D-Bus object and forgets its pairing information.
  bool RemoveDevice(const std::string& address);

 private:
  // Returns the in-memory discovered device based on its address, adding it if
  // does not already exist.
  Device* AddOrGetDiscoveredDevice(const std::string& address,
                                   uint8_t address_type);

  // Exports the device to D-Bus (if not already exported) and updates its
  // current properties.
  void ExportOrUpdateDevice(Device* device);

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
  void HandleDisconnect(
      std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<>> response,
      dbus::Message* message);
  // TODO(mcchou): Handle the rest of the D-Bus methods of the device interface.
  // ConnectProfile() - No op, but we may need dummy implementation later.
  // DisconnectPorfile() - No op, but we may need dummy implementation later.
  // GetServiceRecords() - No op, but we may need dummy implementation later.
  // ExecuteWrite()

  // Called when a connection/disconnection request is fulfilled and we are
  // ready to send a reply of the Connect()/Disconnect() method.
  void ConnectReply(const std::string& device_address,
                    bool success,
                    const std::string& dbus_error);
  // Called on update of GATT client connection.
  void OnGattClientConnectCallback(gatt_client_conn_t conn_id, uint8_t status);

  // Finds a device from |discovered_devices_| with the given |device_address|.
  // Returns nullptr if no such device is found.
  Device* FindDevice(const std::string& device_address);

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

  // Updates EIR data of |device|.
  static void UpdateEir(Device* device, const std::vector<uint8_t>& eir);

  // Updates the service UUIDs based on data of EirTypes including
  // UUID16_INCOMPLETE, UUID16_COMPLETE, UUID32_INCOMPLETE, UUID32_COMPLETE,
  // UUID128_INCOMPLETE and UUID128_COMPLETE.
  static void UpdateServiceUuids(std::set<Uuid>* service_uuids,
                                 uint8_t uuid_size,
                                 const uint8_t* data,
                                 uint8_t data_len);

  // Updates the service data based on data of EirTypes including SVC_DATA16,
  // SVC_DATA32 and SVC_DATA128.
  static void UpdateServiceData(
      std::map<Uuid, std::vector<uint8_t>>* service_data,
      uint8_t uuid_size,
      const uint8_t* data,
      uint8_t data_len);

  // Resets the update status of device properties.
  void ClearPropertiesUpdated(Device* device);

  // Determines the security requirements based on the appearance of a device.
  // Returns true if determined. The default security requirements
  // (bond:true MITM:false) are used.
  struct smPairSecurityRequirements DetermineSecurityRequirements(
      const Device& device);

  // Called when a pairing state changed event is received.
  void OnPairStateChanged(const std::string& address,
                          PairState pair_state,
                          PairError pair_error);

  // Called when a connection state changed.
  void OnConnectStateChanged(const std::string& address,
                             ConnectState connect_state);

  scoped_refptr<dbus::Bus> bus_;
  Newblue* newblue_;
  ExportedObjectManagerWrapper* exported_object_manager_wrapper_;

  // Keeps the discovered devices.
  // TODO(sonnysasaka): Clear old devices according to BlueZ mechanism.
  std::map<std::string, std::unique_ptr<Device>> discovered_devices_;

  UniqueId pair_observer_id_;

  // Device object path and its response to the ongoing pairing/cancelpairing
  // request. <device address, D-Bus method response to pairing, D-Bus
  // method response to cancel pairing>
  struct PairSession ongoing_pairing_;

  // Contains pairs of <device address, connection session> to store the
  // D-Bus method response(s) to the ongoing connection/disconnection requests.
  std::map<std::string, struct ConnectSession> connection_sessions_;
  // Contains pairs of <device address, connection info>.
  std::map<std::string, struct Connection> connections_;
  // Contains pairs of <device address, connection attempt>.
  std::map<std::string, gatt_client_conn_t> connection_attempts_;

  // Must come last so that weak pointers will be invalidated before other
  // members are destroyed.
  base::WeakPtrFactory<DeviceInterfaceHandler> weak_ptr_factory_;

  FRIEND_TEST(DeviceInterfaceHandlerTest, UpdateEirNormal);
  FRIEND_TEST(DeviceInterfaceHandlerTest, UpdateEirAbnormal);

  DISALLOW_COPY_AND_ASSIGN(DeviceInterfaceHandler);
};

}  // namespace bluetooth

#endif  // BLUETOOTH_NEWBLUED_DEVICE_INTERFACE_HANDLER_H_
