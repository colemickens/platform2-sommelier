// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bluetooth/newblued/gatt_attributes.h"

#include <utility>

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

GattService::GattService(uint16_t first_handle,
                         uint16_t last_handle,
                         bool primary,
                         const Uuid& uuid)
    : first_handle_(first_handle),
      last_handle_(last_handle),
      primary_(primary),
      uuid_(uuid) {
  CHECK(first_handle <= last_handle);
}

void GattService::SetDeviceAddress(const std::string& device_address) {
  CHECK(!device_address.empty());
  device_address_.SetValue(device_address);
}

void GattService::AddIncludedService(
    std::unique_ptr<GattIncludedService> included_service) {
  CHECK(included_service != nullptr);
  CHECK(included_service->service() == this);

  included_services_.emplace(included_service->first_handle(),
                             std::move(included_service));
}

void GattService::AddCharacteristic(
    std::unique_ptr<GattCharacteristic> characteristic) {
  CHECK(characteristic != nullptr);
  CHECK(characteristic->service().value() == this);

  characteristics_.emplace(characteristic->first_handle(),
                           std::move(characteristic));
}

bool GattService::HasOwner() const {
  return !device_address_.value().empty();
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

GattCharacteristic::GattCharacteristic(const GattService* service,
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
  service_.SetValue(service);
}

void GattCharacteristic::AddDescriptor(
    std::unique_ptr<GattDescriptor> descriptor) {
  CHECK(descriptor != nullptr);
  CHECK(descriptor->characteristic().value() == this);
  descriptors_.emplace(descriptor->handle(), std::move(descriptor));
}

GattDescriptor::GattDescriptor(const GattCharacteristic* characteristic,
                               uint16_t handle,
                               const Uuid& uuid)
    : handle_(handle), uuid_(uuid) {
  CHECK(characteristic != nullptr);
  characteristic_.SetValue(characteristic);
}

}  // namespace bluetooth
