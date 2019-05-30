// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bluetooth/newblued/gatt_attributes.h"

#include <base/logging.h>
#include <base/macros.h>

namespace bluetooth {

GattService::GattService(const std::string& device_address,
                         uint16_t first_handle,
                         uint16_t last_handle,
                         bool primary,
                         const Uuid& uuid)
    : device_address_(device_address),
      first_handle_(first_handle),
      last_handle_(last_handle),
      primary_(primary),
      uuid_(uuid) {
  CHECK(!device_address.empty());
  CHECK(first_handle <= last_handle);
}

GattIncludedService::GattIncludedService(GattService* service,
                                         uint16_t included_handle,
                                         uint16_t first_handle,
                                         uint16_t last_handle,
                                         const Uuid& uuid)
    : included_handle_(included_handle),
      first_handle_(first_handle),
      last_handle_(last_handle),
      uuid_(uuid) {
  CHECK(service != nullptr);
  CHECK(first_handle <= last_handle);
  service_ = service;
}

GattCharacteristic::GattCharacteristic(GattService* service,
                                       uint16_t value_handle,
                                       uint16_t first_handle,
                                       uint16_t last_handle,
                                       uint8_t properties,
                                       const Uuid& uuid)
    : value_handle_(value_handle),
      first_handle_(first_handle),
      last_handle_(last_handle),
      properties_(properties),
      uuid_(uuid),
      notify_setting_(GattCharacteristic::NotifySetting::NONE) {
  CHECK(service != nullptr);
  CHECK(first_handle <= last_handle);
  service_ = service;
}

GattDescriptor::GattDescriptor(GattCharacteristic* characteristic,
                               uint16_t handle,
                               const Uuid& uuid)
    : handle_(handle), uuid_(uuid) {
  CHECK(characteristic != nullptr);
  characteristic_ = characteristic;
}

}  // namespace bluetooth
