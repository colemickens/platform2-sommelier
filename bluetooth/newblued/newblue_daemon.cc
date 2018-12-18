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
#include "bluetooth/common/util.h"
#include "bluetooth/newblued/libnewblue.h"

namespace bluetooth {

namespace {

constexpr char kDeviceTypeLe[] = "LE";

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

// Converts pair state to string.
std::string ConvertPairStateToString(PairState state) {
  switch (state) {
    case PairState::CANCELED:
      return "canceled";
    case PairState::NOT_PAIRED:
      return "not paired";
    case PairState::FAILED:
      return "failed";
    case PairState::PAIRED:
      return "paired";
    case PairState::STARTED:
      return "started";
    default:
      return "unknown";
  }
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

  adapter_interface_handler_ = std::make_unique<AdapterInterfaceHandler>(
      bus_, newblue_.get(), exported_object_manager_wrapper_.get());

  agent_manager_interface_handler_ =
      std::make_unique<AgentManagerInterfaceHandler>(
          bus_, exported_object_manager_wrapper_.get());
  agent_manager_interface_handler_->Init();
  newblue_->RegisterPairingAgent(agent_manager_interface_handler_.get());

  if (!newblue_->ListenReadyForUp(base::Bind(&NewblueDaemon::OnHciReadyForUp,
                                             base::Unretained(this)))) {
    LOG(ERROR) << "Error listening to HCI ready for up";
    return false;
  }

  LOG(INFO) << "newblued initialized";
  return true;
}

void NewblueDaemon::Shutdown() {
  newblue_->UnregisterPairingAgent();
  newblue_->UnregisterAsPairObserver(pair_observer_id_);

  newblue_.reset();
  agent_manager_interface_handler_.reset();
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
  adapter_interface_handler_->Init(
      base::Bind(&NewblueDaemon::OnDeviceDiscovered, base::Unretained(this)));
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

void NewblueDaemon::AddDeviceMethodHandlers(
    ExportedInterface* device_interface) {
  CHECK(device_interface);

  device_interface->AddMethodHandlerWithMessage(
      bluetooth_device::kPair,
      base::Bind(&NewblueDaemon::HandlePair, base::Unretained(this)));
  device_interface->AddMethodHandlerWithMessage(
      bluetooth_device::kCancelPairing,
      base::Bind(&NewblueDaemon::HandleCancelPairing, base::Unretained(this)));
  device_interface->AddMethodHandlerWithMessage(
      bluetooth_device::kConnect,
      base::Bind(&NewblueDaemon::HandleConnect, base::Unretained(this)));
}

void NewblueDaemon::HandlePair(
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<>> response,
    dbus::Message* message) {
  CHECK(message);

  std::string device_address =
      ConvertDeviceObjectPathToAddress(message->GetPath().value());

  VLOG(1) << "Handling Pair for device " << device_address;

  if (!ongoing_pairing_.address.empty()) {
    LOG(WARNING) << "Pairing in progress with " << device_address;
    response->ReplyWithError(FROM_HERE, brillo::errors::dbus::kDomain,
                             bluetooth_device::kErrorInProgress,
                             "Pairing in progress");
    return;
  }

  ongoing_pairing_.address = device_address;
  ongoing_pairing_.pair_response = std::move(response);
  ongoing_pairing_.cancel_pair_response.reset();

  if (!newblue_->Pair(device_address)) {
    response->ReplyWithError(FROM_HERE, brillo::errors::dbus::kDomain,
                             bluetooth_device::kErrorFailed, "Unknown device");

    // Clear the existing pairing to allow the new pairing request.
    ongoing_pairing_.address.clear();
    ongoing_pairing_.pair_response.reset();
  }
}

void NewblueDaemon::HandleCancelPairing(
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<>> response,
    dbus::Message* message) {
  CHECK(message);

  std::string device_address =
      ConvertDeviceObjectPathToAddress(message->GetPath().value());

  VLOG(1) << "Handling CancelPairing for device " << device_address;

  if (device_address.empty() || !ongoing_pairing_.pair_response) {
    response->ReplyWithError(FROM_HERE, brillo::errors::dbus::kDomain,
                             bluetooth_device::kErrorDoesNotExist,
                             "No ongoing pairing");
    return;
  }

  ongoing_pairing_.cancel_pair_response = std::move(response);

  if (!newblue_->CancelPair(device_address)) {
    response->ReplyWithError(FROM_HERE, brillo::errors::dbus::kDomain,
                             bluetooth_device::kErrorFailed, "Unknown device");
    ongoing_pairing_.cancel_pair_response.reset();
  }
}

void NewblueDaemon::HandleConnect(
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<>> response,
    dbus::Message* message) {
  CHECK(message);

  std::string device_address =
      ConvertDeviceObjectPathToAddress(message->GetPath().value());

  VLOG(1) << "Handling Connect for device " << device_address;

  // TODO(mcchou): Implement org.bluez.Device1.Connect.
  response->ReplyWithError(FROM_HERE, brillo::errors::dbus::kDomain,
                           bluetooth_device::kErrorFailed,
                           "Not implemented yet");
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
  VLOG(1) << "Pairing state changed to " << ConvertPairStateToString(pair_state)
          << " for device " << device.address;

  dbus::ObjectPath device_path(
      ConvertDeviceAddressToObjectPath(device.address.c_str()));
  ExportedInterface* device_interface =
      exported_object_manager_wrapper_->GetExportedInterface(
          device_path, bluetooth_device::kBluetoothDeviceInterface);

  if (device.address != ongoing_pairing_.address) {
    UpdateDeviceProperties(device_interface, device, false);
    return;
  }

  CHECK(!ongoing_pairing_.address.empty());
  CHECK(ongoing_pairing_.pair_response);

  // Reply to the Pair/CancelPairing method calls according to the pairing
  // state.
  switch (pair_state) {
    case PairState::NOT_PAIRED:
      // Falling back to this state indicate an unknown error, so the cancel
      // pairing request should fail as well.
      ongoing_pairing_.pair_response->ReplyWithError(
          FROM_HERE, brillo::errors::dbus::kDomain,
          bluetooth_device::kErrorFailed, "Unknown");

      if (ongoing_pairing_.cancel_pair_response) {
        ongoing_pairing_.cancel_pair_response->ReplyWithError(
            FROM_HERE, brillo::errors::dbus::kDomain,
            bluetooth_device::kErrorDoesNotExist, "No ongoing pairing");
      }
      break;
    case PairState::STARTED:
      // For the start of the pairing, we wait for the result.
      CHECK(!ongoing_pairing_.cancel_pair_response);
      break;
    case PairState::PAIRED:
      ongoing_pairing_.pair_response->SendRawResponse(
          ongoing_pairing_.pair_response->CreateCustomResponse());

      if (ongoing_pairing_.cancel_pair_response) {
        ongoing_pairing_.cancel_pair_response->ReplyWithError(
            FROM_HERE, brillo::errors::dbus::kDomain,
            bluetooth_device::kErrorFailed, "Unknown - pairing done");
      }
      break;
    case PairState::CANCELED:
      ongoing_pairing_.pair_response->ReplyWithError(
          FROM_HERE, brillo::errors::dbus::kDomain, dbus_error,
          "Pairing canceled");

      if (ongoing_pairing_.cancel_pair_response) {
        ongoing_pairing_.cancel_pair_response->SendRawResponse(
            ongoing_pairing_.cancel_pair_response->CreateCustomResponse());
      }
      break;
    case PairState::FAILED:
      ongoing_pairing_.pair_response->ReplyWithError(
          FROM_HERE, brillo::errors::dbus::kDomain, dbus_error,
          "Pairing failed");

      if (ongoing_pairing_.cancel_pair_response) {
        ongoing_pairing_.cancel_pair_response->ReplyWithError(
            FROM_HERE, brillo::errors::dbus::kDomain,
            bluetooth_device::kErrorDoesNotExist, "No ongoing pairing");
      }
      break;
    default:
      LOG(WARNING) << "Unexpected pairing state change";

      ongoing_pairing_.pair_response->ReplyWithError(
          FROM_HERE, brillo::errors::dbus::kDomain,
          bluetooth_device::kErrorFailed, "Unknown");

      if (ongoing_pairing_.cancel_pair_response) {
        ongoing_pairing_.cancel_pair_response->ReplyWithError(
            FROM_HERE, brillo::errors::dbus::kDomain,
            bluetooth_device::kErrorFailed, "Unknown");
      }
      break;
  }

  if (pair_state != PairState::STARTED) {
    ongoing_pairing_.address.clear();
    ongoing_pairing_.pair_response.reset();
  }

  ongoing_pairing_.cancel_pair_response.reset();

  UpdateDeviceProperties(device_interface, device, false);
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
