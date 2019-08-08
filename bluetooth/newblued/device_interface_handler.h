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
#include <base/observer_list.h>
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

  // TODO(yudiliu): We need a proper "address" struct to represent the address.
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

  // Identity address (in format XX:XX:XX:XX:XX:XX).
  Property<std::string> identity_address;

  // Latest advertising address (in format XX:XX:XX:XX:XX:XX).
  Property<std::string> advertised_address;

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
    ConnectSession() : connect_by_us(false), disconnect_by_us(false) {}
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<>> connect_response;
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<>>
        disconnect_response;

    // *_by_us are used to distinguish whether the connection/disconnection is
    // initiated by either newblued (us) or our clients. With only the above
    // |*_response|, we cannot tell whether the connection/disconnection comes
    // from nowhere or from newblued.
    bool connect_by_us;
    bool disconnect_by_us;
  };

  // Structure representing a connection.
  struct Connection {
    gatt_client_conn_t conn_id;
    ble_hid_conn_t hid_id;  // This can be invalid if not a HID device.
  };

 public:
  // Interface for observing changes to devices
  class DeviceObserver {
   public:
    virtual ~DeviceObserver() {}
    virtual void OnGattConnected(const std::string& device_address,
                                 gatt_client_conn_t conn_id) {}
    virtual void OnGattDisconnected(const std::string& device_address,
                                    gatt_client_conn_t conn_id) {}
  };

  using ConnectCallback = base::Callback<void(const std::string& device_address,
                                              bool success,
                                              const std::string& dbus_error)>;

  using ScanManagementCallback = base::Callback<void(bool enabled)>;

  // |newblue| and |exported_object_manager_wrapper| not owned, caller must make
  // sure it outlives this object.
  DeviceInterfaceHandler(
      scoped_refptr<dbus::Bus> bus,
      Newblue* newblue,
      ExportedObjectManagerWrapper* exported_object_manager_wrapper);
  virtual ~DeviceInterfaceHandler() = default;

  // Starts listening pair state events from Newblue.
  bool Init();

  // Sets the callback to manage background scan.
  void SetScanManagementCallback(ScanManagementCallback callback);

  // Returns weak pointer of this object.
  base::WeakPtr<DeviceInterfaceHandler> GetWeakPtr();

  // Called when an update of a device info is received.
  // |scanned_by_client| being true means that the scan result is due to scan
  // requested by clients rather than a background scan.
  void OnDeviceDiscovered(bool scanned_by_client,
                          const std::string& adv_address,
                          uint8_t address_type,
                          const std::string& resolved_address,
                          int8_t rssi,
                          uint8_t reply_type,
                          const std::vector<uint8_t>& eir);

  // Removes a device D-Bus object and forgets its pairing information.
  bool RemoveDevice(const std::string& address, std::string* dbus_error);

  // Add/remove observer for device events.
  void AddDeviceObserver(DeviceObserver* observer);
  void RemoveDeviceObserver(DeviceObserver* observer);

  // Returns device address if the given connection ID exists, empty otherwise.
  std::string GetAddressByConnectionId(gatt_client_conn_t conn_id);

  // Returns device connection ID if connection exists,
  // |kInvalidGattConnectionId| otherwise.
  gatt_client_conn_t GetConnectionIdByAddress(const std::string& address);

  // Once GATT primary services traversal is done, GATT should call this with
  // |resolved| set to true. If GATT invalidates services, GATT should also call
  // this with |resolved| set to false.
  void SetGattServicesResolved(const std::string& device_address,
                               bool resolved);

 private:
  // Returns the in-memory discovered device based on its key address, adding
  // it if does not already exist.
  Device* AddOrGetDiscoveredDevice(const std::string& key_address,
                                   const std::string& adv_address,
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

  // Initiates LE connection/disconnection to a peer device. These are internal
  // functions called by the user facing D-Bus Connect() method and newblued for
  // internal stack logic. |*_by_us| indicates whether the
  // connection/disconnection is initiated by the internal logic but not D-Bus
  // clients.
  void ConnectInternal(const std::string& device_address,
                       std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<>>
                           connect_response,
                       bool connect_by_us);
  void DisconnectInternal(
      const std::string& device_address,
      std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<>>
          disconnect_response,
      bool disconnect_by_us);

  // Called when a connection/disconnection request is fulfilled and we are
  // ready to send a reply of the Connect()/Disconnect() method.
  void ConnectReply(const std::string& device_address,
                    bool success,
                    const std::string& dbus_error);
  // Called on update of GATT client connection.
  void OnGattClientConnectCallback(gatt_client_conn_t conn_id, uint8_t status);

  void SetDeviceConnected(Device* device, bool is_connected);
  void SetDevicePaired(Device* device, bool is_connected);
  void UpdateBackgroundScan();

  // Finds a device from |discovered_devices_| with the given |device_address|.
  // Returns nullptr if no such device is found.
  Device* FindDevice(const std::string& device_address);

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
                          PairError pair_error,
                          const std::string& identity_address);

  scoped_refptr<dbus::Bus> bus_;
  Newblue* newblue_;
  ExportedObjectManagerWrapper* exported_object_manager_wrapper_;

  ScanManagementCallback scan_management_callback_;

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

  base::ObserverList<DeviceObserver> observers_;

  // Must come last so that weak pointers will be invalidated before other
  // members are destroyed.
  base::WeakPtrFactory<DeviceInterfaceHandler> weak_ptr_factory_;

  FRIEND_TEST(DeviceInterfaceHandlerTest, UpdateEirNormal);
  FRIEND_TEST(DeviceInterfaceHandlerTest, UpdateEirAbnormal);

  DISALLOW_COPY_AND_ASSIGN(DeviceInterfaceHandler);
};

}  // namespace bluetooth

#endif  // BLUETOOTH_NEWBLUED_DEVICE_INTERFACE_HANDLER_H_
