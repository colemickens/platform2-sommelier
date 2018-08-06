// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bluetooth/newblued/newblue_daemon.h"

#include <algorithm>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <sysexits.h>

#include <base/logging.h>
#include <base/strings/stringprintf.h>
#include <base/threading/platform_thread.h>
#include <chromeos/dbus/service_constants.h>

#include "bluetooth/common/dbus_daemon.h"
#include "bluetooth/newblued/libnewblue.h"

namespace bluetooth {

namespace {

// Chrome OS device has only 1 Bluetooth adapter, so the name is always hci0.
// TODO(sonnysasaka): Add support for more than 1 adapters.
constexpr char kAdapterObjectPath[] = "/org/bluez/hci0";

constexpr char kDeviceTypeLe[] = "LE";

// Called when an interface of a D-Bus object is exported.
void OnInterfaceExported(std::string object_path,
                         std::string interface_name,
                         bool success) {
  VLOG(1) << "Completed interface export " << interface_name << " of object "
          << object_path << ", success = " << success;
}

// Canonicalizes UUIDs and wraps them as a vector for exposing or updating
// service UUIDs.
std::vector<std::string> CanonicalizeUuids(const std::set<Uuid>& uuids) {
  std::vector<std::string> result;
  result.reserve(uuids.size());
  for (const auto& uuid : uuids)
    result.push_back(uuid.canonical_value());

  return result;
}

// Canonicalizes UUIDs associated with service data for exposing or updating
// service data.
std::map<std::string, std::vector<uint8_t>> CanonicalizeServiceData(
    const std::map<Uuid, std::vector<uint8_t>>& service_data) {
  std::map<std::string, std::vector<uint8_t>> result;
  for (const auto& data : service_data)
    result.emplace(data.first.canonical_value(), data.second);

  return result;
}

// Converts device object path from device address, e.g.
// 00:01:02:03:04:05 will be /org/bluez/hci0/dev_00_01_02_03_04_05
std::string ConvertDeviceAddressToObjectPath(const std::string& address) {
  std::string path =
      base::StringPrintf("%s/dev_%s", kAdapterObjectPath, address.c_str());
  std::replace(path.begin(), path.end(), ':', '_');
  return path;
}

}  // namespace

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
  newblue_->UnregisterAsPairObserver(pair_observer_id_);

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
  stack_sync_monitor_.RegisterBluezDownCallback(
      bus_.get(),
      base::Bind(&NewblueDaemon::OnBluezDown, weak_ptr_factory_.GetWeakPtr()));
  LOG(INFO) << "NewBlue is up";

  // Register for pairing state changed events.
  pair_observer_id_ = newblue_->RegisterAsPairObserver(
      base::Bind(&NewblueDaemon::OnPairStateChanged, base::Unretained(this)));
  if (pair_observer_id_ == kInvalidUniqueId) {
    LOG(ERROR) << "Failed to register as a pairing observer";
    dbus_daemon_->QuitWithExitCode(EX_UNAVAILABLE);
    return;
  }
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
  adapter_interface
      ->EnsureExportedPropertyRegistered<bool>(
          bluetooth_adapter::kStackSyncQuittingProperty)
      ->SetValue(false);

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
                         bluetooth_adapter::kErrorFailed,
                         "Failed to start discovery");
  }
  return ret;
}

bool NewblueDaemon::HandleStopDiscovery(brillo::ErrorPtr* error,
                                        dbus::Message* message) {
  VLOG(1) << __func__;
  bool ret = newblue_->StopDiscovery();
  if (!ret) {
    brillo::Error::AddTo(error, FROM_HERE, brillo::errors::dbus::kDomain,
                         bluetooth_adapter::kErrorFailed,
                         "Failed to start discovery");
  }
  return ret;
}

void NewblueDaemon::AddDeviceMethodHandlers(
    ExportedInterface* device_interface) {
  CHECK(device_interface);

  device_interface->AddSimpleMethodHandlerWithErrorAndMessage(
      bluetooth_device::kPair, base::Unretained(this),
      &NewblueDaemon::HandlePair);
  device_interface->AddSimpleMethodHandlerWithErrorAndMessage(
      bluetooth_device::kConnect, base::Unretained(this),
      &NewblueDaemon::HandleConnect);
}

bool NewblueDaemon::HandlePair(brillo::ErrorPtr* error,
                               dbus::Message* message) {
  // TODO(sonnysasaka): Implement org.bluez.Device1.Pair.
  VLOG(1) << "Handling Pair for object " << message->GetPath().value();
  bool ret = false;
  if (!ret) {
    *error =
        brillo::Error::Create(FROM_HERE, brillo::errors::dbus::kDomain,
                              bluetooth_device::kErrorFailed, "Failed to pair");
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
                                   bluetooth_device::kErrorFailed,
                                   "Failed to connect");
  }
  return ret;
}

void NewblueDaemon::OnDeviceDiscovered(const Device& device) {
  VLOG(1) << base::StringPrintf("Discovered device with %s address %s, rssi %d",
                                device.is_random_address ? "random" : "public",
                                device.address.c_str(), device.rssi.value());

  bool is_new_device = false;
  dbus::ObjectPath device_path(
      ConvertDeviceAddressToObjectPath(device.address.c_str()));

  ExportedInterface* device_interface =
      exported_object_manager_wrapper_->GetExportedInterface(
          device_path, bluetooth_device::kBluetoothDeviceInterface);
  // The first time a device of this address is discovered, create the D-Bus
  // object representing that device.
  if (device_interface == nullptr) {
    is_new_device = true;
    exported_object_manager_wrapper_->AddExportedInterface(
        device_path, bluetooth_device::kBluetoothDeviceInterface);

    device_interface = exported_object_manager_wrapper_->GetExportedInterface(
        device_path, bluetooth_device::kBluetoothDeviceInterface);

    AddDeviceMethodHandlers(device_interface);

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

  UpdateDeviceProperties(device_interface, device, is_new_device);
}

void NewblueDaemon::OnPairStateChanged(const Device& device,
                                       PairState pair_state,
                                       const std::string& dbus_error) {
  dbus::ObjectPath device_path(
      ConvertDeviceAddressToObjectPath(device.address.c_str()));

  ExportedInterface* device_interface =
      exported_object_manager_wrapper_->GetExportedInterface(
          device_path, bluetooth_device::kBluetoothDeviceInterface);

  UpdateDeviceProperties(device_interface, device, false);

  // TODO(mcchou): Reply to the original D-Bus call.
}

void NewblueDaemon::UpdateDeviceProperties(ExportedInterface* interface,
                                           const Device& device,
                                           bool is_new_device) {
  CHECK(interface);

  // TODO(mcchou): Properties Modalias and MTU is not yet sorted out.

  // The following properties are exported when |is_new_device| is true or when
  // they are updated.
  if (is_new_device) {
    // Expose immutable and non-optional properties for the new device.
    interface->EnsureExportedPropertyRegistered<std::string>(
        bluetooth_device::kAddressProperty)->SetValue(device.address);
    interface->EnsureExportedPropertyRegistered<std::string>(
        bluetooth_device::kTypeProperty)->SetValue(kDeviceTypeLe);
    interface->EnsureExportedPropertyRegistered<bool>(
        bluetooth_device::kLegacyPairingProperty)->SetValue(false);
  }

  UpdateDeviceProperty(interface, bluetooth_device::kPairedProperty,
                       device.paired, is_new_device);
  UpdateDeviceProperty(interface, bluetooth_device::kConnectedProperty,
                       device.connected, is_new_device);
  UpdateDeviceProperty(interface, bluetooth_device::kTrustedProperty,
                       device.trusted, is_new_device);
  UpdateDeviceProperty(interface, bluetooth_device::kBlockedProperty,
                       device.blocked, is_new_device);
  UpdateDeviceProperty(interface, bluetooth_device::kAliasProperty,
                       device.alias, is_new_device);
  UpdateDeviceProperty(interface, bluetooth_device::kServicesResolvedProperty,
                       device.services_resolved, is_new_device);
  UpdateDeviceProperty(interface,
                       bluetooth_device::kAdvertisingDataFlagsProperty,
                       device.flags, is_new_device);
  // Although RSSI is an optional device property in BlueZ, it is always
  // provided by libnewblue, thus it is exposed by default.
  UpdateDeviceProperty(interface, bluetooth_device::kRSSIProperty, device.rssi,
                       is_new_device);

  // The following properties are exported only when they are updated.
  UpdateDeviceProperty(interface, bluetooth_device::kUUIDsProperty,
                       device.service_uuids, &CanonicalizeUuids, false);
  UpdateDeviceProperty(interface, bluetooth_device::kServiceDataProperty,
                       device.service_data, &CanonicalizeServiceData, false);
  UpdateDeviceProperty(interface, bluetooth_device::kNameProperty, device.name,
                       false);
  UpdateDeviceProperty(interface, bluetooth_device::kTxPowerProperty,
                       device.tx_power, false);
  UpdateDeviceProperty(interface, bluetooth_device::kClassProperty,
                       device.eir_class, false);
  UpdateDeviceProperty(interface, bluetooth_device::kAppearanceProperty,
                       device.appearance, false);
  UpdateDeviceProperty(interface, bluetooth_device::kIconProperty, device.icon,
                       false);
  UpdateDeviceProperty(interface, bluetooth_device::kManufacturerDataProperty,
                       device.manufacturer, false);
}

void NewblueDaemon::OnBluezDown() {
  ExportedInterface* adapter_interface =
      exported_object_manager_wrapper_->GetExportedInterface(
          dbus::ObjectPath(kAdapterObjectPath),
          bluetooth_adapter::kBluetoothAdapterInterface);
  if (!adapter_interface)
    return;

  LOG(INFO) << "Quitting due to BlueZ down detected";
  adapter_interface
      ->EnsureExportedPropertyRegistered<bool>(
          bluetooth_adapter::kStackSyncQuittingProperty)
      ->SetValue(true);
  exit(0);  // TODO(crbug/873905): Quit gracefully after this is fixed.
}

}  // namespace bluetooth
