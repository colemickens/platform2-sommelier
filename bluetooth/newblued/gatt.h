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

#include "bluetooth/newblued/device_interface_handler.h"
#include "bluetooth/newblued/gatt_attributes.h"
#include "bluetooth/newblued/newblue.h"
#include "bluetooth/newblued/uuid.h"

namespace bluetooth {

// The core implementation of GATT profile.
class Gatt : DeviceInterfaceHandler::DeviceObserver {
 public:
  Gatt(Newblue* newblue, DeviceInterfaceHandler* device_interface_handler);
  ~Gatt();

  // Overrides of DeviceInterfaceHandler::DeviceObserver.
  void OnGattConnected(const std::string& device_address,
                       gatt_client_conn_t conn_id) override;
  void OnGattDisconnected(const std::string& device_address,
                          gatt_client_conn_t conn_id) override;

 private:
  void OnGattClientEnumServices(bool finished,
                                gatt_client_conn_t conn_id,
                                UniqueId transaction_id,
                                Uuid uuid,
                                bool primary,
                                uint16_t first_handle,
                                uint16_t num_handles,
                                GattClientOperationStatus status);

  Newblue* newblue_;

  // TODO(mcchou): Once the refactoring of internal API layer is done, the
  // constructor should take the pointer to the object holding the device
  // connection instead of DeviceInterfaceHandler.
  DeviceInterfaceHandler* device_interface_handler_;

  // Contains pairs of <device address, map of GATT services> to store the GATT
  // services associated with remote devices.
  std::map<std::string, std::map<uint16_t, std::unique_ptr<GattService>>>
      remote_services_;

  std::map<UniqueId, GattClientOperationType> transactions_;

  // Must come last so that weak pointers will be invalidated before other
  // members are destroyed.
  base::WeakPtrFactory<Gatt> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(Gatt);
};

}  // namespace bluetooth

#endif  // BLUETOOTH_NEWBLUED_GATT_H_
