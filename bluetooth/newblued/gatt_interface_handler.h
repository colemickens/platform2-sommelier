// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLUETOOTH_NEWBLUED_GATT_INTERFACE_HANDLER_H_
#define BLUETOOTH_NEWBLUED_GATT_INTERFACE_HANDLER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include <base/memory/ref_counted.h>
#include <dbus/bus.h>

#include "bluetooth/common/exported_object_manager_wrapper.h"
#include "bluetooth/newblued/gatt.h"

namespace bluetooth {

class GattInterfaceHandler final : public Gatt::GattObserver {
 public:
  GattInterfaceHandler(
      scoped_refptr<dbus::Bus> bus,
      Newblue* newblue,
      ExportedObjectManagerWrapper* exported_object_manager_wrapper,
      Gatt* gatt);
  ~GattInterfaceHandler() = default;

  void Init();

  // Overrides of Gatt::GattObserver.
  void OnGattServiceAdded(const GattService& service) override;
  void OnGattServiceRemoved(const GattService& service) override;
  void OnGattServiceChanged(const GattService& service) override;
  void OnGattCharacteristicAdded(
      const GattCharacteristic& characteristic) override;
  void OnGattCharacteristicRemoved(
      const GattCharacteristic& characteristic) override;
  void OnGattCharacteristicChanged(
      const GattCharacteristic& characteristic) override;
  void OnGattDescriptorAdded(const GattDescriptor& descriptor) override;
  void OnGattDescriptorRemoved(const GattDescriptor& descriptor) override;

 private:
  // Represents a GATT client request from D-Bus client and respond to the
  // request based on |type|.
  struct GattClientRequest {
    std::string object_path;
    GattClientRequestType type;
    std::unique_ptr<
        brillo::dbus_utils::DBusMethodResponse<std::vector<uint8_t>>>
        read_char_value_response;
  };

  // Helpers to export GATT service characteristic and descriptor interfaces.
  void AddGattCharacteristicMethodHandlers(ExportedInterface* char_interface);
  void AddGattDescriptorMethodHandlers(ExportedInterface* desc_interface);
  void ExportGattServiceProperties(bool is_new,
                                   ExportedInterface* service_interface,
                                   const GattService& service);
  void ExportGattCharacteristicProperties(
      bool is_new,
      ExportedInterface* char_interface,
      const GattCharacteristic& characteristic);
  void ExportGattDescriptorProperties(bool is_new,
                                      ExportedInterface* desc_interface,
                                      const GattDescriptor& descriptor);
  void ExportGattServiceInterface(const GattService& service);
  void ExportGattCharacteristicInterface(
      const GattCharacteristic& characteristic);
  void ExportGattDescriptorInterface(const GattDescriptor& descriptor);

  // D-Bus method handlers for GATT characteristic interface.
  void HandleCharacteristicReadValue(
      std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<
          std::vector<uint8_t>>> response,
      dbus::Message* message,
      const brillo::VariantDictionary& options);
  void HandleCharacteristicWriteValue(
      std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<>> response,
      dbus::Message* message);
  void HandleCharacteristicStartNotify(
      std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<>> response,
      dbus::Message* message);
  void HandleCharacteristicStopNotify(
      std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<>> response,
      dbus::Message* message);
  void HandleCharacteristicPrepareWriteValue(
      std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<>> response,
      dbus::Message* message);

  // D-Bus method handlers for GATT descriptor interface.
  void HandleDescriptorReadValue(
      std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<>> response,
      dbus::Message* message);
  void HandleDescriptorWriteValue(
      std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<>> response,
      dbus::Message* message);

  // Called when GATT client read characteristic value request is done.
  void OnReadCharacteristicValue(UniqueId transaction_id,
                                 const std::string& device_address,
                                 uint16_t service_handle,
                                 uint16_t char_handle,
                                 GattClientOperationError error,
                                 const std::vector<uint8_t>& value);

  scoped_refptr<dbus::Bus> bus_;
  Newblue* newblue_;
  ExportedObjectManagerWrapper* exported_object_manager_wrapper_;
  Gatt* gatt_;

  // Contains pairs of <transaction ID, GATT client request info>.
  std::map<UniqueId, GattClientRequest> gatt_client_requests_;

  // Must come last so that weak pointers will be invalidated before other
  // members are destroyed.
  base::WeakPtrFactory<GattInterfaceHandler> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(GattInterfaceHandler);
};

}  // namespace bluetooth

#endif  // BLUETOOTH_NEWBLUED_GATT_INTERFACE_HANDLER_H_
