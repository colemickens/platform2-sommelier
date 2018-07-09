// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bluetooth/newblued/newblue_daemon.h"

#include <memory>
#include <string>
#include <utility>

#include <sysexits.h>

#include <base/logging.h>
#include <base/strings/stringprintf.h>
#include <base/threading/platform_thread.h>
#include <chromeos/dbus/service_constants.h>

#include "bluetooth/common/dbus_daemon.h"
#include "bluetooth/newblued/libnewblue.h"

namespace {

// Chrome OS device has only 1 Bluetooth adapter, so the name is always hci0.
// TODO(sonnysasaka): Add support for more than 1 adapters.
constexpr char kAdapterObjectPath[] = "/org/bluez/hci0";

constexpr char kErrorBluezFailed[] = "org.bluez.Failed";

// Called when an interface of a D-Bus object is exported.
void OnInterfaceExported(std::string object_path,
                         std::string interface_name,
                         bool success) {
  VLOG(1) << "Completed interface export " << interface_name << " of object "
          << object_path << ", success = " << success;
}

}  // namespace

namespace bluetooth {

NewblueDaemon::NewblueDaemon(std::unique_ptr<Newblue> newblue)
    : newblue_(std::move(newblue)), weak_ptr_factory_(this) {}

bool NewblueDaemon::Init(scoped_refptr<dbus::Bus> bus,
                         DBusDaemon* dbus_daemon) {
  bus_ = bus;
  dbus_daemon_ = dbus_daemon;

  if (!bus_->RequestOwnershipAndBlock(
          newblue_object_manager::kNewblueObjectManagerServiceName,
          dbus::Bus::REQUIRE_PRIMARY)) {
    LOG(ERROR) << "Failed to acquire D-Bus name ownership: "
               << newblue_object_manager::kNewblueObjectManagerServiceName;
  }

  auto exported_object_manager =
      std::make_unique<brillo::dbus_utils::ExportedObjectManager>(
          bus_, dbus::ObjectPath(
                    newblue_object_manager::kNewblueObjectManagerServicePath));

  exported_object_manager_wrapper_ =
      std::make_unique<ExportedObjectManagerWrapper>(
          bus_, std::move(exported_object_manager));

  exported_object_manager_wrapper_->SetPropertyHandlerSetupCallback(
      base::Bind(&NewblueDaemon::SetupPropertyMethodHandlers,
                 weak_ptr_factory_.GetWeakPtr()));

  if (!newblue_->Init()) {
    LOG(ERROR) << "Failed initializing NewBlue";
    return false;
  }

  if (!newblue_->ListenReadyForUp(base::Bind(&NewblueDaemon::OnHciReadyForUp,
                                             base::Unretained(this)))) {
    LOG(ERROR) << "Error listening to HCI ready for up";
    return false;
  }

  LOG(INFO) << "newblued initialized";
  return true;
}

void NewblueDaemon::Shutdown() {
  newblue_.reset();
  exported_object_manager_wrapper_.reset();
}

void NewblueDaemon::OnHciReadyForUp() {
  VLOG(1) << "NewBlue ready for up";

  // Workaround to avoid immediately bringing up the stack as this may result
  // in chip hang.
  // TODO(sonnysasaka): Remove this sleep when the kernel LE splitter bug
  // is fixed(https://crbug.com/852446).
  base::PlatformThread::Sleep(base::TimeDelta::FromSeconds(1));

  if (!newblue_->BringUp()) {
    LOG(ERROR) << "error bringing up NewBlue";
    dbus_daemon_->QuitWithExitCode(EX_UNAVAILABLE);
    return;
  }
  ExportAdapterInterface();
  LOG(INFO) << "NewBlue is up";
}

void NewblueDaemon::SetupPropertyMethodHandlers(
    brillo::dbus_utils::DBusInterface* prop_interface,
    brillo::dbus_utils::ExportedPropertySet* property_set) {
  // Install standard property handlers.
  prop_interface->AddSimpleMethodHandler(
      dbus::kPropertiesGetAll, base::Unretained(property_set),
      &brillo::dbus_utils::ExportedPropertySet::HandleGetAll);
  prop_interface->AddSimpleMethodHandlerWithError(
      dbus::kPropertiesGet, base::Unretained(property_set),
      &brillo::dbus_utils::ExportedPropertySet::HandleGet);
  prop_interface->AddSimpleMethodHandlerWithError(
      dbus::kPropertiesSet, base::Unretained(property_set),
      &brillo::dbus_utils::ExportedPropertySet::HandleSet);
}

void NewblueDaemon::ExportAdapterInterface() {
  dbus::ObjectPath adapter_object_path(kAdapterObjectPath);
  exported_object_manager_wrapper_->AddExportedInterface(
      adapter_object_path, bluetooth_adapter::kBluetoothAdapterInterface);
  ExportedInterface* adapter_interface =
      exported_object_manager_wrapper_->GetExportedInterface(
          adapter_object_path, bluetooth_adapter::kBluetoothAdapterInterface);

  // Expose the "Powered" property of the adapter. This property is only
  // controlled by BlueZ, so newblued's "Powered" property is ignored by
  // btdispatch. However, it is useful to have the dummy "Powered" property
  // for testing when Chrome (or any client) connects directly to newblued
  // instead of via btdispatch.
  adapter_interface
      ->EnsureExportedPropertyRegistered<bool>(
          bluetooth_adapter::kPoweredProperty)
      ->SetValue(true);

  AddAdapterMethodHandlers(adapter_interface);

  adapter_interface->ExportAsync(
      base::Bind(&OnInterfaceExported, adapter_object_path.value(),
                 bluetooth_adapter::kBluetoothAdapterInterface));
}

void NewblueDaemon::AddAdapterMethodHandlers(
    ExportedInterface* adapter_interface) {
  adapter_interface->AddSimpleMethodHandlerWithErrorAndMessage(
      bluetooth_adapter::kStartDiscovery, base::Unretained(this),
      &NewblueDaemon::HandleStartDiscovery);
  adapter_interface->AddSimpleMethodHandlerWithErrorAndMessage(
      bluetooth_adapter::kStopDiscovery, base::Unretained(this),
      &NewblueDaemon::HandleStopDiscovery);
}

bool NewblueDaemon::HandleStartDiscovery(brillo::ErrorPtr* error,
                                         dbus::Message* message) {
  VLOG(1) << __func__;
  bool ret = newblue_->StartDiscovery(
      base::Bind(&NewblueDaemon::OnDeviceDiscovered, base::Unretained(this)));
  if (!ret) {
    brillo::Error::AddTo(error, FROM_HERE, brillo::errors::dbus::kDomain,
                         kErrorBluezFailed, "Failed to start discovery");
  }
  return ret;
}

bool NewblueDaemon::HandleStopDiscovery(brillo::ErrorPtr* error,
                                        dbus::Message* message) {
  VLOG(1) << __func__;
  bool ret = newblue_->StopDiscovery();
  if (!ret) {
    brillo::Error::AddTo(error, FROM_HERE, brillo::errors::dbus::kDomain,
                         kErrorBluezFailed, "Failed to start discovery");
  }
  return ret;
}

bool NewblueDaemon::HandlePair(brillo::ErrorPtr* error,
                               dbus::Message* message) {
  // TODO(sonnysasaka): Implement org.bluez.Device1.Pair.
  VLOG(1) << "Handling Pair for object " << message->GetPath().value();
  bool ret = false;
  if (!ret) {
    *error = brillo::Error::Create(FROM_HERE, brillo::errors::dbus::kDomain,
                                   kErrorBluezFailed, "Failed to pair");
  }
  return ret;
}

bool NewblueDaemon::HandleConnect(brillo::ErrorPtr* error,
                                  dbus::Message* message) {
  // TODO(sonnysasaka): Implement org.bluez.Device1.Connect.
  VLOG(1) << "Handling Connect for object " << message->GetPath().value();
  bool ret = false;
  if (!ret) {
    *error = brillo::Error::Create(FROM_HERE, brillo::errors::dbus::kDomain,
                                   kErrorBluezFailed, "Failed to connect");
  }
  return ret;
}

void NewblueDaemon::OnDeviceDiscovered(const Device& device) {
  VLOG(1) << "Discovered device "
          << base::StringPrintf("address = %s, rssi = %d, addr type = %s",
                                device.address.c_str(), device.rssi,
                                device.is_random_address ? "random" : "public");

  std::string device_path_string = base::StringPrintf(
      "%s/dev_%s", kAdapterObjectPath, device.address.c_str());
  std::replace(device_path_string.begin(), device_path_string.end(), ':', '_');
  dbus::ObjectPath device_path(device_path_string);

  ExportedInterface* device_interface =
      exported_object_manager_wrapper_->GetExportedInterface(
          device_path, bluetooth_device::kBluetoothDeviceInterface);
  // The first time a device of this address is discovered, create the D-Bus
  // object representing that device.
  if (device_interface == nullptr) {
    exported_object_manager_wrapper_->AddExportedInterface(
        device_path, bluetooth_device::kBluetoothDeviceInterface);

    device_interface = exported_object_manager_wrapper_->GetExportedInterface(
        device_path, bluetooth_device::kBluetoothDeviceInterface);

    device_interface->AddSimpleMethodHandlerWithErrorAndMessage(
        bluetooth_device::kPair, base::Unretained(this),
        &NewblueDaemon::HandlePair);
    device_interface->AddSimpleMethodHandlerWithErrorAndMessage(
        bluetooth_device::kConnect, base::Unretained(this),
        &NewblueDaemon::HandleConnect);

    // The "Adapter" property of this device object has to be set before
    // ExportAsync() below. This is to make sure that as soon as a client
    // realizes that this object is exported, it can immediately check this
    // property value. This at least satisfies Chrome's behavior which checks
    // whether this device belongs to the adapter it's interested in.
    device_interface
        ->EnsureExportedPropertyRegistered<dbus::ObjectPath>(
            bluetooth_device::kAdapterProperty)
        ->SetValue(dbus::ObjectPath(kAdapterObjectPath));

    device_interface->ExportAsync(
        base::Bind(&OnInterfaceExported, device_path.value(),
                   bluetooth_device::kBluetoothDeviceInterface));
  }

  // Update the device object's properties.
  device_interface
      ->EnsureExportedPropertyRegistered<std::string>(
          bluetooth_device::kAddressProperty)
      ->SetValue(device.address);
  device_interface
      ->EnsureExportedPropertyRegistered<std::string>(
          bluetooth_device::kNameProperty)
      ->SetValue(device.name);
  device_interface
      ->EnsureExportedPropertyRegistered<int16_t>(
          bluetooth_device::kRSSIProperty)
      ->SetValue(device.rssi);
  device_interface
      ->EnsureExportedPropertyRegistered<uint32_t>(
          bluetooth_device::kClassProperty)
      ->SetValue(device.eir_class);
  device_interface
      ->EnsureExportedPropertyRegistered<uint16_t>(
          bluetooth_device::kAppearanceProperty)
      ->SetValue(device.appearance);

  // These are properties that are required to exist (bluez: doc/device-api.txt)
  // But newblued doesn't support these yet, so assign static values.
  device_interface
      ->EnsureExportedPropertyRegistered<bool>(
          bluetooth_device::kPairedProperty)
      ->SetValue(false);
  device_interface
      ->EnsureExportedPropertyRegistered<bool>(
          bluetooth_device::kConnectedProperty)
      ->SetValue(false);
  device_interface
      ->EnsureExportedPropertyRegistered<bool>(
          bluetooth_device::kTrustedProperty)
      ->SetValue(false);
  device_interface
      ->EnsureExportedPropertyRegistered<bool>(
          bluetooth_device::kBlockedProperty)
      ->SetValue(false);
  device_interface
      ->EnsureExportedPropertyRegistered<std::string>(
          bluetooth_device::kAliasProperty)
      ->SetValue("");
  device_interface
      ->EnsureExportedPropertyRegistered<bool>(
          bluetooth_device::kLegacyPairingProperty)
      ->SetValue(false);
  device_interface
      ->EnsureExportedPropertyRegistered<bool>(
          bluetooth_device::kServicesResolvedProperty)
      ->SetValue(false);
}

}  // namespace bluetooth
