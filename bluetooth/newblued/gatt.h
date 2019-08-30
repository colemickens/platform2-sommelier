// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLUETOOTH_NEWBLUED_GATT_H_
#define BLUETOOTH_NEWBLUED_GATT_H_

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <base/macros.h>
#include <base/memory/weak_ptr.h>
#include <base/observer_list.h>

#include "bluetooth/newblued/device_interface_handler.h"
#include "bluetooth/newblued/gatt_attributes.h"
#include "bluetooth/newblued/newblue.h"
#include "bluetooth/newblued/uuid.h"

namespace bluetooth {

// Error codes for GATT client operations based on AttError code.
enum class GattClientOperationError : uint8_t {
  NONE,
  READ_NOT_ALLOWED,
  WRITE_NOT_ALLOWED,
  INSUFF_AUTHN,
  NOT_SUPPORTED,
  INSUFF_AUTHZ,
  INVALID_OFFSET,
  INSUFF_ENCR_KEY_SIZE,
  INVALUD_ATTR_VALUE_LENGTH,
  INSUFF_ENC,
  OTHER,
};

enum class GattClientRequestType : uint8_t {
  NONE,
  READ_CHARACTERISTIC_VALUE,
  READ_DESCRIPTOR_VALUE,
};

// The core implementation of GATT profile.
class Gatt final : public DeviceInterfaceHandler::DeviceObserver {
 public:
  // Called when GATT client read characteristic value is done. Empty |value|
  // and |error| other than GattClientOperationError::NONE both indicate
  // failure.
  using ReadCharacteristicValueCallback =
      base::Callback<void(UniqueId transaction_id,
                          const std::string& device_address,
                          uint16_t service_handle,
                          uint16_t char_handle,
                          GattClientOperationError error,
                          const std::vector<uint8_t>& value)>;

  using ReadDescriptorValueCallback =
      base::Callback<void(UniqueId transaction_id,
                          const std::string& device_address,
                          uint16_t service_handle,
                          uint16_t char_handle,
                          uint16_t desc_handle,
                          GattClientOperationError error,
                          const std::vector<uint8_t>& value)>;

  // Interface for observing GATT events.
  class GattObserver {
   public:
    virtual void OnGattServiceAdded(const GattService& service) {}
    virtual void OnGattServiceRemoved(const GattService& service) {}
    virtual void OnGattServiceChanged(const GattService& service) {}
    virtual void OnGattCharacteristicAdded(
        const GattCharacteristic& characteristic) {}
    virtual void OnGattCharacteristicRemoved(
        const GattCharacteristic& characteristic) {}
    virtual void OnGattCharacteristicChanged(
        const GattCharacteristic& characteristic) {}
    virtual void OnGattDescriptorAdded(const GattDescriptor& descriptor) {}
    virtual void OnGattDescriptorRemoved(const GattDescriptor& descriptor) {}
    virtual void OnGattDescriptorChanged(const GattDescriptor& descriptor) {}
  };

  Gatt(Newblue* newblue, DeviceInterfaceHandler* device_interface_handler);
  ~Gatt();

  // Add/remove observer for GATT events.
  void AddGattObserver(GattObserver* observer);
  void RemoveGattObserver(GattObserver* observer);

  // Reads the complete value associated with a GATT client characteristic
  // starting from |offset|, e.g. given |offset| 2, the return of reading from
  // value {0x00, 0x01, 0x02, 0x03} will be {0x02, 0x03}. This returns a valid
  // UniqueId to indicate that the read is ongoing.
  UniqueId ReadCharacteristicValue(const std::string& device_address,
                                   uint16_t service_handle,
                                   uint16_t char_handle,
                                   uint16_t offset,
                                   ReadCharacteristicValueCallback callback);

  // Reads the complete value associated with a GATT client descriptor start
  // from |offset| , e.g. given |offset| 2, the return of reading from value
  // {0x00, 0x01, 0x02, 0x03} will be {0x02, 0x03}. This returns a valid
  // UniqueId to indicate that the read is ongoing.
  UniqueId ReadDescriptorValue(const std::string& device_address,
                               uint16_t service_handle,
                               uint16_t char_handle,
                               uint16_t desc_handle,
                               uint16_t offset,
                               ReadDescriptorValueCallback callback);

  // Overrides of DeviceInterfaceHandler::DeviceObserver.
  void OnGattConnected(const std::string& device_address,
                       gatt_client_conn_t conn_id) override;
  void OnGattDisconnected(const std::string& device_address,
                          gatt_client_conn_t conn_id) override;

 private:
  // Represents the intermediate state which is used to perform sub-procedure.
  struct ClientOperationState {
    ClientOperationState(uint16_t s_handle,
                         uint16_t c_handle,
                         uint16_t d_handle,
                         uint16_t os)
        : service_handle(s_handle),
          char_handle(c_handle),
          desc_handle(d_handle),
          offset(os) {}
    uint16_t service_handle;
    uint16_t char_handle;
    uint16_t desc_handle;
    uint16_t offset;
    std::vector<uint8_t> value;
  };

  // Represents the mapping between a request and an GATT operation. Extra
  // information are kept here to perform async callback.
  struct ClientOperation {
    GattClientOperationType operation_type;
    GattClientRequestType request_type;
    gatt_client_conn_t conn_id;
    ReadCharacteristicValueCallback read_char_value_callback;
    ReadDescriptorValueCallback read_desc_value_callback;
  };

  // Traverses all primary services to retrieve complete structure of remote
  // GATT attributes.
  void TravPrimaryServices(const std::string& device_address,
                           gatt_client_conn_t conn_id);

  bool IsTravPrimaryServicesCompleted(gatt_client_conn_t conn_id);

  // Returns the target characteristic if it exists, nullptr otherwise.
  GattCharacteristic* FindGattCharacteristic(const std::string& device_address,
                                             uint16_t service_handle,
                                             uint16_t char_handle) const;

  // Returns the target descriptor if it exists, nullptr otherwise.
  GattDescriptor* FindGattDescriptor(const std::string& device_address,
                                     uint16_t service_handle,
                                     uint16_t char_handle,
                                     uint16_t desc_handle) const;

  void OnGattClientEnumServices(bool finished,
                                gatt_client_conn_t conn_id,
                                UniqueId transaction_id,
                                Uuid uuid,
                                bool primary,
                                uint16_t first_handle,
                                uint16_t num_handles,
                                GattClientOperationStatus status);

  void OnGattClientTravPrimaryService(gatt_client_conn_t conn_id,
                                      UniqueId transaction_id,
                                      std::unique_ptr<GattService> service);

  // Called when GATT client read value operation is done. Note that
  // |state| is attached to the callback in advance as extra information
  // such as attribute handles.
  void OnGattClientReadLongValue(std::unique_ptr<ClientOperationState> state,
                                 gatt_client_conn_t conn_id,
                                 UniqueId transaction_id,
                                 uint16_t value_handle,
                                 AttError error,
                                 const std::vector<uint8_t>& value);

  // Called by OnGattClientReadLongValue() to report the result of reading
  // characteristic value.
  void OnGattClientReadLongCharacteristicValue(
      std::unique_ptr<ClientOperationState> state,
      const std::string& device_address,
      UniqueId transaction_id,
      uint16_t value_handle,
      AttError error,
      const std::vector<uint8_t>& value);

  // Called by OnGattClientReadLongValue() to report the result of reading
  // descriptor value.
  void OnGattClientReadLongDescriptorValue(
      std::unique_ptr<ClientOperationState> state,
      const std::string& device_address,
      UniqueId transaction_id,
      uint16_t value_handle,
      AttError error,
      const std::vector<uint8_t>& value);

  Newblue* newblue_;

  // TODO(mcchou): Once the refactoring of internal API layer is done, the
  // constructor should take the pointer to the object holding the device
  // connection instead of DeviceInterfaceHandler.
  DeviceInterfaceHandler* device_interface_handler_;

  // Contains pairs of <device address, map of GATT services> to store the GATT
  // services associated with remote devices.
  std::map<std::string, std::map<uint16_t, std::unique_ptr<GattService>>>
      remote_services_;

  // Contains pairs of <unique ID, GATT operation> to store the ongoing GATT
  // request and its corresponding GATT client operation.
  std::map<UniqueId, ClientOperation> transactions_;

  base::ObserverList<GattObserver> observers_;

  // Must come last so that weak pointers will be invalidated before other
  // members are destroyed.
  base::WeakPtrFactory<Gatt> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(Gatt);
};

}  // namespace bluetooth

#endif  // BLUETOOTH_NEWBLUED_GATT_H_
