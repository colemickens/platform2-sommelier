// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bluetooth/newblued/gatt_interface_handler.h"

#include <bits/stdint-uintn.h>
#include <cstdint>
#include <memory>
#include <vector>

#include <chromeos/dbus/service_constants.h>

#include "bluetooth/newblued/util.h"

namespace bluetooth {

namespace {

// Canonicalizes UUID.
std::string CanonicalizeUuid(const Uuid& uuid) {
  return uuid.canonical_value();
}

// Converts GATT service into its D-Bus object path.
std::string ConvertServiceToObjectPath(const GattService* const& service) {
  CHECK(service);
  CHECK(service->HasOwner());

  std::string device_address = service->device_address().value();
  return ConvertServiceHandleToObjectPath(device_address,
                                          service->first_handle());
}

// Converts GATT characteristic into its D-Bus object path.
std::string ConvertCharToObjectPath(
    const GattCharacteristic* const& characteristic) {
  CHECK(characteristic);

  const GattService* service = characteristic->service().value();
  CHECK(service->HasOwner());

  return bluetooth::ConvertCharacteristicHandleToObjectPath(
      service->device_address().value(), service->first_handle(),
      characteristic->first_handle());
}

// Converts GATT characteristic properties to BlueZ GATT characteristic flags.
std::vector<std::string> ConvertPropertiesToStrings(
    const std::uint8_t& properties) {
  std::vector<std::string> flags;

  if (properties & GattCharacteristicPropertyMask::BROADCAST)
    flags.push_back(bluetooth_gatt_characteristic::kFlagBroadcast);
  if (properties & GattCharacteristicPropertyMask::READ)
    flags.push_back(bluetooth_gatt_characteristic::kFlagRead);
  if (properties & GattCharacteristicPropertyMask::WRITE_WITHOUT_RESPONSE)
    flags.push_back(bluetooth_gatt_characteristic::kFlagWriteWithoutResponse);
  if (properties & GattCharacteristicPropertyMask::WRITE)
    flags.push_back(bluetooth_gatt_characteristic::kFlagWrite);
  if (properties & GattCharacteristicPropertyMask::NOTIFY)
    flags.push_back(bluetooth_gatt_characteristic::kFlagNotify);
  if (properties & GattCharacteristicPropertyMask::INDICATE)
    flags.push_back(bluetooth_gatt_characteristic::kFlagIndicate);
  if (properties & GattCharacteristicPropertyMask::AUTHENTICATED_SIGNED_WRITE)
    flags.push_back(
        bluetooth_gatt_characteristic::kFlagAuthenticatedSignedWrites);
  if (properties & GattCharacteristicPropertyMask::EXTENDED_PROPERTIES)
    flags.push_back(bluetooth_gatt_characteristic::kFlagExtendedProperties);

  return flags;
}

// Translates GATT notifying setting to a bool value.
bool ConvertNotifySettingToBool(
    const GattCharacteristic::NotifySetting& setting) {
  return setting != GattCharacteristic::NotifySetting::NONE;
}

}  // namespace

GattInterfaceHandler::GattInterfaceHandler(
    scoped_refptr<dbus::Bus> bus,
    Newblue* newblue,
    ExportedObjectManagerWrapper* exported_object_manager_wrapper,
    Gatt* gatt)
    : bus_(bus),
      newblue_(newblue),
      exported_object_manager_wrapper_(exported_object_manager_wrapper),
      gatt_(gatt) {
  CHECK(bus_);
  CHECK(newblue_);
  CHECK(exported_object_manager_wrapper_);
  CHECK(gatt_);
}

void GattInterfaceHandler::Init() {
  gatt_->AddGattObserver(this);
}

void GattInterfaceHandler::OnGattServiceAdded(const GattService& service) {
  ExportGattServiceInterface(service);
}

void GattInterfaceHandler::OnGattServiceRemoved(const GattService& service) {
  CHECK(service.HasOwner());

  std::string path(ConvertServiceHandleToObjectPath(
      service.device_address().value(), service.first_handle()));
  dbus::ObjectPath service_path(path);

  VLOG(1) << "Unexporting a GATT service object at " << path;

  exported_object_manager_wrapper_->RemoveExportedInterface(
      service_path, bluetooth_gatt_service::kBluetoothGattServiceInterface);
}

void GattInterfaceHandler::OnGattServiceChanged(const GattService& service) {
  ExportGattServiceInterface(service);
}

void GattInterfaceHandler::OnGattCharacteristicAdded(
    const GattCharacteristic& characteristic) {
  ExportGattCharacteristicInterface(characteristic);
}

void GattInterfaceHandler::OnGattCharacteristicRemoved(
    const GattCharacteristic& characteristic) {
  const GattService* service = characteristic.service().value();

  CHECK(service->HasOwner());

  std::string path(ConvertCharacteristicHandleToObjectPath(
      service->device_address().value(), service->first_handle(),
      characteristic.first_handle()));
  dbus::ObjectPath char_path(path);

  VLOG(1) << "Unexporting a GATT characteristic object at " << path;

  exported_object_manager_wrapper_->RemoveExportedInterface(
      char_path,
      bluetooth_gatt_characteristic::kBluetoothGattCharacteristicInterface);
}

void GattInterfaceHandler::OnGattDescriptorAdded(
    const GattDescriptor& descriptor) {
  ExportGattDescriptorInterface(descriptor);
}

void GattInterfaceHandler::OnGattDescriptorRemoved(
    const GattDescriptor& descriptor) {
  const GattCharacteristic* characteristic =
      descriptor.characteristic().value();
  const GattService* service = characteristic->service().value();

  CHECK(service->HasOwner());

  std::string path(ConvertDescriptorHandleToObjectPath(
      service->device_address().value(), service->first_handle(),
      characteristic->first_handle(), descriptor.handle()));
  dbus::ObjectPath desc_path(path);

  VLOG(1) << "Unexporting a GATT descriptor object at " << path;

  exported_object_manager_wrapper_->RemoveExportedInterface(
      desc_path, bluetooth_gatt_descriptor::kBluetoothGattDescriptorInterface);
}

void GattInterfaceHandler::AddGattCharacteristicMethodHandlers(
    ExportedInterface* char_interface) {
  CHECK(char_interface);

  char_interface->AddMethodHandlerWithMessage(
      bluetooth_gatt_characteristic::kReadValue,
      base::Bind(&GattInterfaceHandler::HandleCharacteristicReadValue,
                 base::Unretained(this)));
  char_interface->AddMethodHandlerWithMessage(
      bluetooth_gatt_characteristic::kWriteValue,
      base::Bind(&GattInterfaceHandler::HandleCharacteristicWriteValue,
                 base::Unretained(this)));
  char_interface->AddMethodHandlerWithMessage(
      bluetooth_gatt_characteristic::kStartNotify,
      base::Bind(&GattInterfaceHandler::HandleCharacteristicStartNotify,
                 base::Unretained(this)));
  char_interface->AddMethodHandlerWithMessage(
      bluetooth_gatt_characteristic::kStopNotify,
      base::Bind(&GattInterfaceHandler::HandleCharacteristicStopNotify,
                 base::Unretained(this)));
  char_interface->AddMethodHandlerWithMessage(
      bluetooth_gatt_characteristic::kPrepareWriteValue,
      base::Bind(&GattInterfaceHandler::HandleCharacteristicPrepareWriteValue,
                 base::Unretained(this)));
}

void GattInterfaceHandler::AddGattDescriptorMethodHandlers(
    ExportedInterface* desc_interface) {
  CHECK(desc_interface);

  desc_interface->AddMethodHandlerWithMessage(
      bluetooth_gatt_descriptor::kReadValue,
      base::Bind(&GattInterfaceHandler::HandleDescriptorReadValue,
                 base::Unretained(this)));
  desc_interface->AddMethodHandlerWithMessage(
      bluetooth_gatt_descriptor::kWriteValue,
      base::Bind(&GattInterfaceHandler::HandleDescriptorWriteValue,
                 base::Unretained(this)));
}

void GattInterfaceHandler::ExportGattServiceProperties(
    bool is_new,
    ExportedInterface* service_interface,
    const GattService& service) {
  CHECK(service_interface);

  ExportDBusProperty(service_interface, bluetooth_gatt_service::kUUIDProperty,
                     service.uuid(), &CanonicalizeUuid, is_new);
  ExportDBusProperty(service_interface, bluetooth_gatt_service::kDeviceProperty,
                     service.device_address(),
                     &ConvertDeviceAddressToObjectPath, is_new);
  ExportDBusProperty(service_interface,
                     bluetooth_gatt_service::kPrimaryProperty,
                     service.primary(), is_new);
}

void GattInterfaceHandler::ExportGattCharacteristicProperties(
    bool is_new,
    ExportedInterface* char_interface,
    const GattCharacteristic& characteristic) {
  CHECK(char_interface);

  ExportDBusProperty(char_interface,
                     bluetooth_gatt_characteristic::kUUIDProperty,
                     characteristic.uuid(), &CanonicalizeUuid, is_new);
  ExportDBusProperty(
      char_interface, bluetooth_gatt_characteristic::kServiceProperty,
      characteristic.service(), &ConvertServiceToObjectPath, is_new);

  // GATT characteristic value property is optional, export only if the value
  // is not empty for a new characteristic or there is an update.
  if (!is_new || (is_new && !characteristic.value().value().empty())) {
    ExportDBusProperty(char_interface,
                       bluetooth_gatt_characteristic::kValueProperty,
                       characteristic.value(), is_new);
  }

  // TODO(mcchou): ConvertPropertiesToStrings only includes the properties
  // which come along with the characteristic but not the extended properties.
  // We need to parse the extended properties descriptor and present those in
  // Flags as well.
  ExportDBusProperty(
      char_interface, bluetooth_gatt_characteristic::kFlagsProperty,
      characteristic.properties(), &ConvertPropertiesToStrings, is_new);

  // GATT characteristic notifying property is optional, export only if
  // characteristic properties contains notify and indicate bits.
  uint8_t char_props = characteristic.properties().value();
  if (char_props & GattCharacteristicPropertyMask::NOTIFY ||
      char_props & GattCharacteristicPropertyMask::INDICATE) {
    ExportDBusProperty(
        char_interface, bluetooth_gatt_characteristic::kNotifyingProperty,
        characteristic.notify_setting(), &ConvertNotifySettingToBool, is_new);
  }
}

void GattInterfaceHandler::ExportGattDescriptorProperties(
    bool is_new,
    ExportedInterface* desc_interface,
    const GattDescriptor& descriptor) {
  CHECK(desc_interface);

  ExportDBusProperty(desc_interface, bluetooth_gatt_descriptor::kUUIDProperty,
                     descriptor.uuid(), &CanonicalizeUuid, is_new);
  ExportDBusProperty(
      desc_interface, bluetooth_gatt_descriptor::kCharacteristicProperty,
      descriptor.characteristic(), &ConvertCharToObjectPath, is_new);

  // GATT descriptor value property is optional, export only if the value is
  // not empty for a new descriptor or there is an update.
  if (!is_new || (is_new && !descriptor.value().value().empty())) {
    ExportDBusProperty(desc_interface,
                       bluetooth_gatt_descriptor::kValueProperty,
                       descriptor.value(), is_new);
  }
}

void GattInterfaceHandler::ExportGattServiceInterface(
    const GattService& service) {
  CHECK(service.HasOwner());

  bool is_new = false;
  std::string device_address = service.device_address().value();
  std::string path =
      ConvertServiceHandleToObjectPath(device_address, service.first_handle());
  dbus::ObjectPath service_path(path);
  ExportedInterface* service_interface =
      exported_object_manager_wrapper_->GetExportedInterface(
          service_path, bluetooth_gatt_service::kBluetoothGattServiceInterface);
  if (service_interface == nullptr) {
    is_new = true;

    VLOG(1) << "Exporting a new GATT service object at " << path;

    exported_object_manager_wrapper_->AddExportedInterface(
        service_path, bluetooth_gatt_service::kBluetoothGattServiceInterface,
        base::Bind(
            &ExportedObjectManagerWrapper::SetupStandardPropertyHandlers));
    service_interface = exported_object_manager_wrapper_->GetExportedInterface(
        service_path, bluetooth_gatt_service::kBluetoothGattServiceInterface);
  } else {
    VLOG(2) << "Updating GATT service object at " << path;
  }

  ExportGattServiceProperties(is_new, service_interface, service);

  if (is_new)
    service_interface->ExportAndBlock();
}

void GattInterfaceHandler::ExportGattCharacteristicInterface(
    const GattCharacteristic& characteristic) {
  const GattService* service = characteristic.service().value();
  CHECK(service->HasOwner());

  bool is_new = false;
  std::string device_address = service->device_address().value();
  std::string path = ConvertCharacteristicHandleToObjectPath(
      device_address, service->first_handle(), characteristic.first_handle());
  dbus::ObjectPath char_path(path);

  ExportedInterface* char_interface =
      exported_object_manager_wrapper_->GetExportedInterface(
          char_path,
          bluetooth_gatt_characteristic::kBluetoothGattCharacteristicInterface);
  if (char_interface == nullptr) {
    is_new = true;

    VLOG(1) << "Exporting a new GATT characteristic object at " << path;

    exported_object_manager_wrapper_->AddExportedInterface(
        char_path,
        bluetooth_gatt_characteristic::kBluetoothGattCharacteristicInterface,
        base::Bind(
            &ExportedObjectManagerWrapper::SetupStandardPropertyHandlers));
    char_interface = exported_object_manager_wrapper_->GetExportedInterface(
        char_path,
        bluetooth_gatt_characteristic::kBluetoothGattCharacteristicInterface);

    AddGattCharacteristicMethodHandlers(char_interface);
  } else {
    VLOG(2) << "Updating GATT characteristic object at " << path;
  }

  ExportGattCharacteristicProperties(is_new, char_interface, characteristic);

  if (is_new)
    char_interface->ExportAndBlock();
}

void GattInterfaceHandler::ExportGattDescriptorInterface(
    const GattDescriptor& descriptor) {
  const GattCharacteristic* characteristic =
      descriptor.characteristic().value();
  const GattService* service = characteristic->service().value();
  CHECK(service->HasOwner());

  bool is_new = false;
  std::string device_address = service->device_address().value();
  std::string path = ConvertDescriptorHandleToObjectPath(
      device_address, service->first_handle(), characteristic->first_handle(),
      descriptor.handle());
  dbus::ObjectPath desc_path(path);

  ExportedInterface* desc_interface =
      exported_object_manager_wrapper_->GetExportedInterface(
          desc_path,
          bluetooth_gatt_descriptor::kBluetoothGattDescriptorInterface);
  if (desc_interface == nullptr) {
    is_new = true;

    VLOG(1) << "Exporting a new GATT descriptor object at " << path;

    exported_object_manager_wrapper_->AddExportedInterface(
        desc_path, bluetooth_gatt_descriptor::kBluetoothGattDescriptorInterface,
        base::Bind(
            &ExportedObjectManagerWrapper::SetupStandardPropertyHandlers));
    desc_interface = exported_object_manager_wrapper_->GetExportedInterface(
        desc_path,
        bluetooth_gatt_descriptor::kBluetoothGattDescriptorInterface);

    AddGattDescriptorMethodHandlers(desc_interface);
  } else {
    VLOG(2) << "Updating new GATT descriptor object at " << path;
  }

  ExportGattDescriptorProperties(is_new, desc_interface, descriptor);

  if (is_new)
    desc_interface->ExportAndBlock();
}

void GattInterfaceHandler::HandleCharacteristicReadValue(
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<>> response,
    dbus::Message* message) {
  response->ReplyWithError(FROM_HERE, brillo::errors::dbus::kDomain,
                           bluetooth_gatt_characteristic::kErrorFailed,
                           "Not implemented");
}

void GattInterfaceHandler::HandleCharacteristicWriteValue(
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<>> response,
    dbus::Message* message) {
  response->ReplyWithError(FROM_HERE, brillo::errors::dbus::kDomain,
                           bluetooth_gatt_characteristic::kErrorFailed,
                           "Not implemented");
}

void GattInterfaceHandler::HandleCharacteristicStartNotify(
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<>> response,
    dbus::Message* message) {
  response->ReplyWithError(FROM_HERE, brillo::errors::dbus::kDomain,
                           bluetooth_gatt_characteristic::kErrorFailed,
                           "Not implemented");
}

void GattInterfaceHandler::HandleCharacteristicStopNotify(
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<>> response,
    dbus::Message* message) {
  response->ReplyWithError(FROM_HERE, brillo::errors::dbus::kDomain,
                           bluetooth_gatt_characteristic::kErrorFailed,
                           "Not implemented");
}

void GattInterfaceHandler::HandleCharacteristicPrepareWriteValue(
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<>> response,
    dbus::Message* message) {
  response->ReplyWithError(FROM_HERE, brillo::errors::dbus::kDomain,
                           bluetooth_gatt_characteristic::kErrorFailed,
                           "Not implemented");
}

void GattInterfaceHandler::HandleDescriptorReadValue(
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<>> response,
    dbus::Message* message) {
  response->ReplyWithError(FROM_HERE, brillo::errors::dbus::kDomain,
                           bluetooth_gatt_descriptor::kErrorFailed,
                           "Not implemented");
}

void GattInterfaceHandler::HandleDescriptorWriteValue(
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<>> response,
    dbus::Message* message) {
  response->ReplyWithError(FROM_HERE, brillo::errors::dbus::kDomain,
                           bluetooth_gatt_descriptor::kErrorFailed,
                           "Not implemented");
}

}  // namespace bluetooth
