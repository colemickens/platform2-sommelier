// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLUETOOTH_NEWBLUED_GATT_ATTRIBUTES_H_
#define BLUETOOTH_NEWBLUED_GATT_ATTRIBUTES_H_

#include <list>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <base/macros.h>

#include "bluetooth/common/uuid.h"

namespace bluetooth {

class GattIncludedService;
class GattCharacteristic;
class GattDescriptor;

// Represents a GATT primary/secondary service.
class GattService {
 public:
  GattService(const std::string& device_address,
              uint16_t first_handle,
              uint16_t last_handle,
              bool primary,
              const Uuid& uuid);
  virtual ~GattService() {}

  const std::string& device_address() { return device_address_; }
  uint16_t first_handle() { return first_handle_; }
  uint16_t last_handle() { return last_handle_; }
  bool primary() { return primary_; }
  const Uuid& uuid() { return uuid_; }

 private:
  std::string device_address_;
  uint16_t first_handle_;
  uint16_t last_handle_;
  bool primary_;
  Uuid uuid_;
  std::map<uint16_t, std::unique_ptr<GattCharacteristic>> characteristics_;
  std::list<std::unique_ptr<GattIncludedService>> included_services_;

  DISALLOW_COPY_AND_ASSIGN(GattService);
};

// Represents a GATT included service.
class GattIncludedService {
 public:
  GattIncludedService(GattService* service,
                      uint16_t included_handle,
                      uint16_t first_handle,
                      uint16_t last_handle,
                      const Uuid& uuid);
  virtual ~GattIncludedService() {}

  const GattService* service() { return service_; }
  uint16_t included_handle() { return included_handle_; }
  uint16_t first_handle() { return first_handle_; }
  uint16_t last_handle() { return last_handle_; }
  const Uuid& uuid() { return uuid_; }

 private:
  GattService* service_;

  uint16_t included_handle_;
  uint16_t first_handle_;
  uint16_t last_handle_;
  Uuid uuid_;

  DISALLOW_COPY_AND_ASSIGN(GattIncludedService);
};

// Represents a GATT characteristic.
class GattCharacteristic {
 public:
  enum class NotifySetting : uint8_t {
    NONE,
    NOTIFICATION,
    INDICATION,
  };

  GattCharacteristic(GattService* service,
                     uint16_t value_handle,
                     uint16_t first_handle,
                     uint16_t last_handle,
                     uint8_t properties,
                     const Uuid& uuid);
  virtual ~GattCharacteristic() {}

  const GattService* service() { return service_; }
  uint16_t value_handle() { return value_handle_; }
  uint16_t first_handle() { return first_handle_; }
  uint16_t last_handle() { return last_handle_; }
  uint8_t properties() { return properties_; }
  const Uuid& uuid() { return uuid_; }
  const std::vector<uint8_t>& value() { return value_; }
  NotifySetting notify_setting() { return notify_setting_; }

 private:
  GattService* service_;

  uint16_t value_handle_;
  uint16_t first_handle_;
  uint16_t last_handle_;
  uint8_t properties_;
  Uuid uuid_;
  std::vector<uint8_t> value_;
  std::map<uint16_t, GattDescriptor> descriptors_;

  NotifySetting notify_setting_;

  DISALLOW_COPY_AND_ASSIGN(GattCharacteristic);
};

// Represents a GATT descriptor.
class GattDescriptor {
 public:
  GattDescriptor(GattCharacteristic* characteristic,
                 uint16_t handle,
                 const Uuid& uuid);
  virtual ~GattDescriptor() {}

  const GattCharacteristic* characteristic() { return characteristic_; }
  uint16_t handle() { return handle_; }
  const Uuid& uuid() { return uuid_; }
  const std::vector<uint8_t>& value() { return value_; }

 private:
  GattCharacteristic* characteristic_;

  uint16_t handle_;
  Uuid uuid_;
  std::vector<uint8_t> value_;

  DISALLOW_COPY_AND_ASSIGN(GattDescriptor);
};

}  // namespace bluetooth

#endif  // BLUETOOTH_NEWBLUED_GATT_ATTRIBUTES_H_
