// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bluetooth/dispatcher/bluez_interface_handler.h"

#include <stdint.h>
#include <map>
#include <string>
#include <vector>

#include <dbus/object_path.h>

namespace bluetooth {

BluezAdapterInterfaceHandler::BluezAdapterInterfaceHandler() {
  AddPropertyFactory<std::string>(bluetooth_adapter::kAddressProperty);
  AddPropertyFactory<std::string>(bluetooth_adapter::kNameProperty);
  AddPropertyFactory<std::string>(bluetooth_adapter::kAliasProperty);
  AddPropertyFactory<uint32_t>(bluetooth_adapter::kClassProperty);
  AddPropertyFactory<bool>(bluetooth_adapter::kPoweredProperty);
  AddPropertyFactory<bool>(bluetooth_adapter::kDiscoverableProperty);
  AddPropertyFactory<bool>(bluetooth_adapter::kPairableProperty);
  AddPropertyFactory<uint32_t>(bluetooth_adapter::kPairableTimeoutProperty);
  AddPropertyFactory<uint32_t>(bluetooth_adapter::kDiscoverableTimeoutProperty);
  AddPropertyFactory<bool>(bluetooth_adapter::kDiscoveringProperty);
  AddPropertyFactory<std::vector<std::string>>(
      bluetooth_adapter::kUUIDsProperty);
  AddPropertyFactory<std::string>(bluetooth_adapter::kModaliasProperty);

  AddMethodName(bluetooth_adapter::kStartDiscovery);
  AddMethodName(bluetooth_adapter::kSetDiscoveryFilter);
  AddMethodName(bluetooth_adapter::kStopDiscovery);
  AddMethodName(bluetooth_adapter::kPauseDiscovery);
  AddMethodName(bluetooth_adapter::kUnpauseDiscovery);
  AddMethodName(bluetooth_adapter::kRemoveDevice);
  AddMethodName(bluetooth_adapter::kCreateServiceRecord);
  AddMethodName(bluetooth_adapter::kRemoveServiceRecord);
}

BluezDeviceInterfaceHandler::BluezDeviceInterfaceHandler() {
  AddPropertyFactory<std::string>(bluetooth_device::kAddressProperty);
  AddPropertyFactory<std::string>(bluetooth_device::kNameProperty);
  AddPropertyFactory<std::string>(bluetooth_device::kIconProperty);
  AddPropertyFactory<uint32_t>(bluetooth_device::kClassProperty);
  AddPropertyFactory<std::string>(bluetooth_device::kTypeProperty);
  AddPropertyFactory<uint16_t>(bluetooth_device::kAppearanceProperty);
  AddPropertyFactory<std::vector<std::string>>(
      bluetooth_device::kUUIDsProperty);
  AddPropertyFactory<int16_t>(bluetooth_device::kTxPowerProperty);
  AddPropertyFactory<bool>(bluetooth_device::kPairedProperty);
  AddPropertyFactory<bool>(bluetooth_device::kConnectedProperty);
  AddPropertyFactory<bool>(bluetooth_device::kTrustedProperty);
  AddPropertyFactory<bool>(bluetooth_device::kBlockedProperty);
  AddPropertyFactory<std::string>(bluetooth_device::kAliasProperty);
  AddPropertyFactory<dbus::ObjectPath>(bluetooth_device::kAdapterProperty);
  AddPropertyFactory<bool>(bluetooth_device::kLegacyPairingProperty);
  AddPropertyFactory<std::string>(bluetooth_device::kModaliasProperty);
  AddPropertyFactory<int16_t>(bluetooth_device::kRSSIProperty);
  AddPropertyFactory<std::map<uint16_t, std::vector<uint8_t>>>(
      bluetooth_device::kManufacturerDataProperty);
  AddPropertyFactory<std::map<std::string, std::vector<uint8_t>>>(
      bluetooth_device::kServiceDataProperty);
  AddPropertyFactory<bool>(bluetooth_device::kServicesResolvedProperty);
  AddPropertyFactory<std::vector<uint8_t>>(
      bluetooth_device::kAdvertisingDataFlagsProperty);

  AddMethodName(bluetooth_device::kConnect);
  AddMethodName(bluetooth_device::kDisconnect);
  AddMethodName(bluetooth_device::kConnectProfile);
  AddMethodName(bluetooth_device::kDisconnectProfile);
  AddMethodName(bluetooth_device::kPair);
  AddMethodName(bluetooth_device::kCancelPairing);
  AddMethodName(bluetooth_device::kGetServiceRecords);
}

BluezGattCharacteristicInterfaceHandler::
    BluezGattCharacteristicInterfaceHandler() {
  AddPropertyFactory<std::string>(bluetooth_gatt_characteristic::kUUIDProperty);
  AddPropertyFactory<dbus::ObjectPath>(
      bluetooth_gatt_characteristic::kServiceProperty);
  AddPropertyFactory<std::vector<uint8_t>>(
      bluetooth_gatt_characteristic::kValueProperty);
  AddPropertyFactory<bool>(bluetooth_gatt_characteristic::kNotifyingProperty);
  AddPropertyFactory<std::vector<std::string>>(
      bluetooth_gatt_characteristic::kFlagsProperty);

  AddMethodName(bluetooth_gatt_characteristic::kReadValue);
  AddMethodName(bluetooth_gatt_characteristic::kWriteValue);
  AddMethodName(bluetooth_gatt_characteristic::kStartNotify);
  AddMethodName(bluetooth_gatt_characteristic::kStopNotify);
}

BluezInputInterfaceHandler::BluezInputInterfaceHandler() {
  AddPropertyFactory<std::string>(bluetooth_input::kReconnectModeProperty);
}

BluezGattServiceInterfaceHandler::BluezGattServiceInterfaceHandler() {
  AddPropertyFactory<std::string>(bluetooth_gatt_service::kUUIDProperty);
  AddPropertyFactory<dbus::ObjectPath>(bluetooth_gatt_service::kDeviceProperty);
  AddPropertyFactory<bool>(bluetooth_gatt_service::kPrimaryProperty);
  AddPropertyFactory<std::vector<dbus::ObjectPath>>(
      bluetooth_gatt_service::kIncludesProperty);
}

BluezGattDescriptorInterfaceHandler::BluezGattDescriptorInterfaceHandler() {
  AddPropertyFactory<std::string>(bluetooth_gatt_descriptor::kUUIDProperty);
  AddPropertyFactory<dbus::ObjectPath>(
      bluetooth_gatt_descriptor::kCharacteristicProperty);
  AddPropertyFactory<std::vector<uint8_t>>(
      bluetooth_gatt_descriptor::kValueProperty);

  AddMethodName(bluetooth_gatt_descriptor::kReadValue);
  AddMethodName(bluetooth_gatt_descriptor::kWriteValue);
}

BluezMediaTransportInterfaceHandler::BluezMediaTransportInterfaceHandler() {
  AddPropertyFactory<dbus::ObjectPath>(
      bluetooth_media_transport::kDeviceProperty);
  AddPropertyFactory<std::string>(bluetooth_media_transport::kUUIDProperty);
  AddPropertyFactory<uint8_t>(bluetooth_media_transport::kCodecProperty);
  AddPropertyFactory<std::vector<uint8_t>>(
      bluetooth_media_transport::kConfigurationProperty);
  AddPropertyFactory<std::string>(bluetooth_media_transport::kStateProperty);
  AddPropertyFactory<uint16_t>(bluetooth_media_transport::kDelayProperty);
  AddPropertyFactory<uint16_t>(bluetooth_media_transport::kVolumeProperty);
}

}  // namespace bluetooth
