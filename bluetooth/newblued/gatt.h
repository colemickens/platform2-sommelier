// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLUETOOTH_NEWBLUED_GATT_H_
#define BLUETOOTH_NEWBLUED_GATT_H_

#include <cstdint>
#include <map>
#include <memory>
#include <string>

#include <base/macros.h>
#include <base/memory/weak_ptr.h>
#include <base/observer_list.h>

#include "bluetooth/newblued/device_interface_handler.h"
#include "bluetooth/newblued/gatt_attributes.h"
#include "bluetooth/newblued/newblue.h"
#include "bluetooth/newblued/uuid.h"

namespace bluetooth {

// The core implementation of GATT profile.
class Gatt final : public DeviceInterfaceHandler::DeviceObserver {
 public:
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
    virtual void OnGattDescriptorAdded(const GattDescriptor& descriptor) {}
    virtual void OnGattDescriptorRemoved(const GattDescriptor& descriptor) {}
  };

  Gatt(Newblue* newblue, DeviceInterfaceHandler* device_interface_handler);
  ~Gatt();

  // Add/remove observer for GATT events.
  void AddGattObserver(GattObserver* observer);
  void RemoveGattObserver(GattObserver* observer);

  // Overrides of DeviceInterfaceHandler::DeviceObserver.
  void OnGattConnected(const std::string& device_address,
                       gatt_client_conn_t conn_id) override;
  void OnGattDisconnected(const std::string& device_address,
                          gatt_client_conn_t conn_id) override;

 private:
  struct ClientOperation {
    GattClientOperationType type;
    gatt_client_conn_t conn_id;
  };

  // Traverses all primary services to retrieve complete structure of remote
  // GATT attributes.
  void TravPrimaryServices(const std::string& device_address,
                           gatt_client_conn_t conn_id);

  bool IsTravPrimaryServicesCompleted(gatt_client_conn_t conn_id);

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

  Newblue* newblue_;

  // TODO(mcchou): Once the refactoring of internal API layer is done, the
  // constructor should take the pointer to the object holding the device
  // connection instead of DeviceInterfaceHandler.
  DeviceInterfaceHandler* device_interface_handler_;

  // Contains pairs of <device address, map of GATT services> to store the GATT
  // services associated with remote devices.
  std::map<std::string, std::map<uint16_t, std::unique_ptr<GattService>>>
      remote_services_;

  std::map<UniqueId, ClientOperation> transactions_;

  base::ObserverList<GattObserver> observers_;

  // Must come last so that weak pointers will be invalidated before other
  // members are destroyed.
  base::WeakPtrFactory<Gatt> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(Gatt);
};

}  // namespace bluetooth

#endif  // BLUETOOTH_NEWBLUED_GATT_H_
