// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bluetooth/newblued/newblue.h"

#include <newblue/sm.h>

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include <base/bind.h>
#include <base/message_loop/message_loop.h>
#include <base/strings/stringprintf.h>
#include <chromeos/dbus/service_constants.h>

#include "bluetooth/common/util.h"

namespace bluetooth {

namespace {

// The default values of Flags, TX Power, EIR Class, Appearance and
// Manufacturer ID come from the assigned numbers and Supplement to the
// Bluetooth Core Specification defined by Bluetooth SIG. These default values
// are used to determine whether the fields are ever received in EIR and set
// for a device.
constexpr int8_t kDefaultRssi = -128;
constexpr int8_t kDefaultTxPower = -128;
constexpr uint32_t kDefaultEirClass = 0x1F00;
constexpr uint16_t kDefaultAppearance = 0;
constexpr uint16_t kDefaultManufacturerId = 0xFFFF;

// Filters out non-ASCII characters from a string by replacing them with spaces.
std::string AsciiString(std::string name) {
  /* Replace all non-ASCII characters with spaces */
  for (auto& ch : name) {
    if (!isascii(ch))
      ch = ' ';
  }

  return name;
}

std::map<uint16_t, std::vector<uint8_t>> MakeManufacturer(
    uint16_t manufacturer_id, std::vector<uint8_t> manufacturer_data) {
  std::map<uint16_t, std::vector<uint8_t>> manufacturer;
  manufacturer.emplace(manufacturer_id, std::move(manufacturer_data));
  return manufacturer;
}

// These value are defined at https://www.bluetooth.com/specifications/gatt/
// viewer?attributeXmlFile=org.bluetooth.characteristic.gap.appearance.xml.
// The translated strings come from BlueZ.
std::string ConvertAppearanceToIcon(uint16_t appearance) {
  switch ((appearance & 0xffc0) >> 6) {
    case 0x00:
      return "unknown";
    case 0x01:
      return "phone";
    case 0x02:
      return "computer";
    case 0x03:
      return "watch";
    case 0x04:
      return "clock";
    case 0x05:
      return "video-display";
    case 0x06:
      return "remote-control";
    case 0x07:
      return "eye-glasses";
    case 0x08:
      return "tag";
    case 0x09:
      return "key-ring";
    case 0x0a:
      return "multimedia-player";
    case 0x0b:
      return "scanner";
    case 0x0c:
      return "thermometer";
    case 0x0d:
      return "heart-rate-sensor";
    case 0x0e:
      return "blood-pressure";
    case 0x0f:  // HID Generic
      switch (appearance & 0x3f) {
        case 0x01:
          return "input-keyboard";
        case 0x02:
          return "input-mouse";
        case 0x03:
        case 0x04:
          return "input-gaming";
        case 0x05:
          return "input-tablet";
        case 0x08:
          return "scanner";
      }
      break;
    case 0x10:
      return "glucose-meter";
    case 0x11:
      return "running-walking-sensor";
    case 0x12:
      return "cycling";
    case 0x31:
      return "pulse-oximeter";
    case 0x32:
      return "weight-scale";
    case 0x33:
      return "personal-mobility-device";
    case 0x34:
      return "continuous-glucose-monitor";
    case 0x35:
      return "insulin-pump";
    case 0x36:
      return "medication-delivery";
    case 0x51:
      return "outdoor-sports-activity";
    default:
      break;
  }
  return std::string();
}

// Converts the uint8_t[6] MAC address into std::string form, e.g.
// {0x05, 0x04, 0x03, 0x02, 0x01, 0x00} will be 00:01:02:03:04:05.
std::string ConvertBtAddrToString(const struct bt_addr& addr) {
  return base::StringPrintf("%02X:%02X:%02X:%02X:%02X:%02X", addr.addr[5],
                            addr.addr[4], addr.addr[3], addr.addr[2],
                            addr.addr[1], addr.addr[0]);
}

// Converts the security manager pairing errors to BlueZ's D-Bus errors.
std::string ConvertSmPairErrorToDbusError(SmPairErr error_code) {
  switch (error_code) {
    case SM_PAIR_ERR_NONE:
      // This comes along with the pairing states other than
      // SM_PAIR_STATE_FAILED.
      return std::string();
    case SM_PAIR_ERR_ALREADY_PAIRED:
      return bluetooth_device::kErrorAlreadyExists;
    case SM_PAIR_ERR_IN_PROGRESS:
      return bluetooth_device::kErrorInProgress;
    case SM_PAIR_ERR_INVALID_PAIR_REQ:
      return bluetooth_device::kErrorInvalidArguments;
    case SM_PAIR_ERR_L2C_CONN:
      return bluetooth_device::kErrorConnectionAttemptFailed;
    case SM_PAIR_ERR_NO_SUCH_DEVICE:
      return bluetooth_device::kErrorDoesNotExist;
    case SM_PAIR_ERR_PASSKEY_FAILED:
    case SM_PAIR_ERR_OOB_NOT_AVAILABLE:
    case SM_PAIR_ERR_AUTH_REQ_INFEASIBLE:
    case SM_PAIR_ERR_CONF_VALUE_MISMATCHED:
    case SM_PAIR_ERR_PAIRING_NOT_SUPPORTED:
    case SM_PAIR_ERR_ENCR_KEY_SIZE:
    case SM_PAIR_ERR_REPEATED_ATTEMPT:
    case SM_PAIR_ERR_INVALID_PARAM:
    case SM_PAIR_ERR_UNEXPECTED_SM_CMD:
    case SM_PAIR_ERR_SEND_SM_CMD:
    case SM_PAIR_ERR_ENCR_CONN:
    case SM_PAIR_ERR_UNEXPECTED_L2C_EVT:
      return bluetooth_device::kErrorAuthenticationFailed;
    case SM_PAIR_ERR_STALLED:
      return bluetooth_device::kErrorAuthenticationTimeout;
    case SM_PAIR_ERR_MEMORY:
    case SM_PAIR_ERR_UNKNOWN:
    default:
      return bluetooth_device::kErrorFailed;
  }
}

}  // namespace

Device::Device() : Device("") {}

Device::Device(const std::string& address)
    : address(address),
      is_random_address(false),
      paired(false),
      connected(false),
      trusted(false),
      blocked(false),
      services_resolved(false),
      tx_power(kDefaultTxPower),
      rssi(kDefaultRssi),
      eir_class(kDefaultEirClass),
      appearance(kDefaultAppearance),
      icon(ConvertAppearanceToIcon(kDefaultAppearance)),
      flags({0}),
      manufacturer(
          MakeManufacturer(kDefaultManufacturerId, std::vector<uint8_t>())) {}

Newblue::Newblue(std::unique_ptr<LibNewblue> libnewblue)
    : libnewblue_(std::move(libnewblue)), weak_ptr_factory_(this) {}

base::WeakPtr<Newblue> Newblue::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

bool Newblue::Init() {
  if (base::MessageLoop::current())
    origin_task_runner_ = base::MessageLoop::current()->task_runner();
  return true;
}

void Newblue::RegisterPairingAgent(PairingAgent* pairing_agent) {
  pairing_agent_ = pairing_agent;
}

void Newblue::UnregisterPairingAgent() {
  pairing_agent_ = nullptr;
}

bool Newblue::ListenReadyForUp(base::Closure callback) {
  // Dummy MAC address. NewBlue doesn't actually use the MAC address as it's
  // exclusively controlled by BlueZ.
  static const uint8_t kZeroMac[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

  if (!libnewblue_->HciUp(kZeroMac, &Newblue::OnStackReadyForUpThunk, this))
    return false;

  ready_for_up_callback_ = callback;
  return true;
}

bool Newblue::BringUp() {
  if (!libnewblue_->HciIsUp()) {
    LOG(ERROR) << "HCI is not ready for up";
    return false;
  }

  if (libnewblue_->L2cInit()) {
    LOG(ERROR) << "Failed to initialize L2CAP";
    return false;
  }

  if (!libnewblue_->AttInit()) {
    LOG(ERROR) << "Failed to initialize ATT";
    return false;
  }

  if (!libnewblue_->GattProfileInit()) {
    LOG(ERROR) << "Failed to initialize GATT";
    return false;
  }

  if (!libnewblue_->GattBuiltinInit()) {
    LOG(ERROR) << "Failed to initialize GATT services";
    return false;
  }

  if (!libnewblue_->SmInit()) {
    LOG(ERROR) << "Failed to init SM";
    return false;
  }

  // Always register passkey display observer, assuming that our UI always
  // supports this.
  // TODO(sonnysasaka): We may optimize this by registering passkey display
  // observer only if there is currently a default agent registered.
  passkey_display_observer_id_ = libnewblue_->SmRegisterPasskeyDisplayObserver(
      this, &Newblue::PasskeyDisplayObserverCallbackThunk);

  pair_state_handle_ = libnewblue_->SmRegisterPairStateObserver(
      this, &Newblue::PairStateCallbackThunk);
  if (!pair_state_handle_) {
    LOG(ERROR) << "Failed to register as an observer of pairing state";
    return false;
  }

  return true;
}

bool Newblue::StartDiscovery(DeviceDiscoveredCallback callback) {
  if (discovery_handle_ != 0) {
    LOG(WARNING) << "Discovery is already started, ignoring request";
    return false;
  }

  discovery_handle_ = libnewblue_->HciDiscoverLeStart(
      &Newblue::DiscoveryCallbackThunk, this, true /* active */,
      false /* use_random_addr */);
  if (discovery_handle_ == 0) {
    LOG(ERROR) << "Failed to start LE discovery";
    return false;
  }

  device_discovered_callback_ = callback;
  return true;
}

bool Newblue::StopDiscovery() {
  if (discovery_handle_ == 0) {
    LOG(WARNING) << "Discovery is not started, ignoring request";
    return false;
  }

  bool ret = libnewblue_->HciDiscoverLeStop(discovery_handle_);
  if (!ret) {
    LOG(ERROR) << "Failed to stop LE discovery";
    return false;
  }

  device_discovered_callback_.Reset();
  discovery_handle_ = 0;
  return true;
}

void Newblue::UpdateEir(Device* device, const std::vector<uint8_t>& eir) {
  CHECK(device);

  uint8_t pos = 0;
  std::set<Uuid> service_uuids;
  std::map<Uuid, std::vector<uint8_t>> service_data;

  while (pos + 1 < eir.size()) {
    uint8_t field_len = eir[pos];

    // End of EIR
    if (field_len == 0)
      break;

    // Corrupt EIR data
    if (pos + field_len >= eir.size())
      break;

    EirType eir_type = static_cast<EirType>(eir[pos + 1]);
    const uint8_t* data = &eir[pos + 2];

    // A field consists of 1 byte field type + data:
    // | 1-byte field_len | 1-byte type | (field length - 1) bytes data ... |
    uint8_t data_len = field_len - 1;

    switch (eir_type) {
      case EirType::FLAGS:
        // No default value should be set for flags according to Supplement to
        // the Bluetooth Core Specification. Flags field can be 0 or more octets
        // long. If the length is 1, then flags[0] is octet[0]. Store only
        // octet[0] for now due to lack of definition of the following octets
        // in Supplement to the Bluetooth Core Specification.
        if (data_len > 0)
          device->flags.SetValue(std::vector<uint8_t>(data, data + 1));
        // If |data_len| is 0, we avoid setting zero-length advertising flags as
        // this currently causes Chrome to crash.
        // TODO(crbug.com/876908): Fix Chrome to not crash with zero-length
        // advertising flags.
        break;

      // If there are more than one instance of either COMPLETE or INCOMPLETE
      // type of a UUID size, the later one(s) will be cached as well.
      case EirType::UUID16_INCOMPLETE:
      case EirType::UUID16_COMPLETE:
        UpdateServiceUuids(&service_uuids, kUuid16Size, data, data_len);
        break;
      case EirType::UUID32_INCOMPLETE:
      case EirType::UUID32_COMPLETE:
        UpdateServiceUuids(&service_uuids, kUuid32Size, data, data_len);
        break;
      case EirType::UUID128_INCOMPLETE:
      case EirType::UUID128_COMPLETE:
        UpdateServiceUuids(&service_uuids, kUuid128Size, data, data_len);
        break;

      // Name
      case EirType::NAME_SHORT:
      case EirType::NAME_COMPLETE: {
        char c_name[HCI_DEV_NAME_LEN + 1];

        // Some device has trailing '\0' at the end of the name data.
        // So make sure we only take the characters before '\0' and limited
        // to the max length allowed by Bluetooth spec (HCI_DEV_NAME_LEN).
        uint8_t name_len =
            std::min(data_len, static_cast<uint8_t>(HCI_DEV_NAME_LEN));
        strncpy(c_name, reinterpret_cast<const char*>(data), name_len);
        c_name[name_len] = '\0';
        device->name.SetValue(AsciiString(c_name));
      } break;

      case EirType::TX_POWER:
        if (data_len == 1)
          device->tx_power.SetValue(static_cast<int8_t>(*data));
        break;
      case EirType::CLASS_OF_DEV:
        // 24-bit little endian data
        if (data_len == 3)
          device->eir_class.SetValue(GetNumFromLE24(data));
        break;

      // If the UUID already exists, the service data will be updated.
      case EirType::SVC_DATA16:
        UpdateServiceData(&service_data, kUuid16Size, data, data_len);
        break;
      case EirType::SVC_DATA32:
        UpdateServiceData(&service_data, kUuid32Size, data, data_len);
        break;
      case EirType::SVC_DATA128:
        UpdateServiceData(&service_data, kUuid128Size, data, data_len);
        break;

      case EirType::GAP_APPEARANCE:
        // 16-bit little endian data
        if (data_len == 2) {
          uint16_t appearance = GetNumFromLE16(data);
          device->appearance.SetValue(appearance);
          device->icon.SetValue(ConvertAppearanceToIcon(appearance));
        }
        break;
      case EirType::MANUFACTURER_DATA:
        if (data_len >= 2) {
          // The order of manufacturer data is not specified explicitly in
          // Supplement to the Bluetooth Core Specification, so the original
          // order used in BlueZ is adopted.
          device->manufacturer.SetValue(MakeManufacturer(
              GetNumFromLE16(data),
              std::vector<uint8_t>(data + 2,
                                   data + std::max<uint8_t>(data_len, 2))));
        }
        break;
      default:
        // Do nothing for unhandled EIR type.
        break;
    }

    pos += field_len + 1;
  }

  // In BlueZ, if alias is never provided or is set to empty for a device, the
  // value of Alias property is set to the value of Name property if
  // |device.name| is not empty. If |device.name| is also empty, then Alias
  // is set to |device.address| in the following string format.
  // xx-xx-xx-xx-xx-xx
  if (device->internal_alias.empty()) {
    std::string alias;
    if (!device->name.value().empty()) {
      alias = device->name.value();
    } else {
      alias = device->address;
      std::replace(alias.begin(), alias.end(), ':', '-');
    }
    device->alias.SetValue(std::move(alias));
  }

  // This is different from BlueZ where it memorizes all service UUIDs and
  // service data ever received for the same device. If there is no service
  // UUIDs/service data being presented, service UUIDs/servicedata will not
  // be updated.
  if (!service_uuids.empty())
    device->service_uuids.SetValue(std::move(service_uuids));
  if (!service_data.empty())
    device->service_data.SetValue(std::move(service_data));
}

UniqueId Newblue::RegisterAsPairObserver(PairStateChangedCallback callback) {
  UniqueId observer_id = GetNextId();
  if (observer_id != kInvalidUniqueId)
    pair_observers_.emplace(observer_id, callback);
  return observer_id;
}

void Newblue::UnregisterAsPairObserver(UniqueId observer_id) {
  pair_observers_.erase(observer_id);
}

bool Newblue::PostTask(const tracked_objects::Location& from_here,
                       const base::Closure& task) {
  CHECK(origin_task_runner_.get());
  return origin_task_runner_->PostTask(from_here, task);
}

void Newblue::OnStackReadyForUpThunk(void* data) {
  Newblue* newblue = static_cast<Newblue*>(data);
  newblue->PostTask(FROM_HERE, base::Bind(&Newblue::OnStackReadyForUp,
                                          newblue->GetWeakPtr()));
}

void Newblue::OnStackReadyForUp() {
  if (ready_for_up_callback_.is_null()) {
    // libnewblue says it's ready for up but I don't have any callback. Most
    // probably another stack (e.g. BlueZ) just re-initialized the adapter.
    LOG(WARNING) << "No callback when stack is ready for up";
    return;
  }

  ready_for_up_callback_.Run();
  // It only makes sense to bring up the stack once and for all. Reset the
  // callback here so we won't bring up the stack twice.
  ready_for_up_callback_.Reset();
}

void Newblue::DiscoveryCallbackThunk(void* data,
                                     const struct bt_addr* addr,
                                     int8_t rssi,
                                     uint8_t reply_type,
                                     const void* eir,
                                     uint8_t eir_len) {
  Newblue* newblue = static_cast<Newblue*>(data);
  std::vector<uint8_t> eir_vector(static_cast<const uint8_t*>(eir),
                                  static_cast<const uint8_t*>(eir) + eir_len);
  std::string address = ConvertBtAddrToString(*addr);
  newblue->PostTask(
      FROM_HERE, base::Bind(&Newblue::DiscoveryCallback, newblue->GetWeakPtr(),
                            address, addr->type, rssi, reply_type, eir_vector));
}

void Newblue::DiscoveryCallback(const std::string& address,
                                uint8_t address_type,
                                int8_t rssi,
                                uint8_t reply_type,
                                const std::vector<uint8_t>& eir) {
  VLOG(1) << __func__;

  if (device_discovered_callback_.is_null()) {
    LOG(WARNING) << "DiscoveryCallback called when not discovering";
    return;
  }

  Device* device = FindDevice(address);
  if (!device) {
    discovered_devices_[address] = std::make_unique<Device>(address);
    device = discovered_devices_[address].get();
  }
  device->is_random_address = (address_type == BT_ADDR_TYPE_LE_RANDOM);
  device->rssi.SetValue(rssi);
  UpdateEir(device, eir);

  device_discovered_callback_.Run(*device);

  ClearPropertiesUpdated(device);
}

void Newblue::UpdateServiceUuids(std::set<Uuid>* service_uuids,
                                 uint8_t uuid_size,
                                 const uint8_t* data,
                                 uint8_t data_len) {
  CHECK(service_uuids && data);

  if (!data_len || data_len % uuid_size != 0) {
    LOG(WARNING) << "Failed to parse EIR service UUIDs";
    return;
  }

  // Service UUIDs are presented in little-endian order.
  for (uint8_t i = 0; i < data_len; i += uuid_size) {
    Uuid uuid(GetBytesFromLE(data + i, uuid_size));
    CHECK(uuid.format() != UuidFormat::UUID_INVALID);
    service_uuids->insert(uuid);
  }
}

void Newblue::UpdateServiceData(
    std::map<Uuid, std::vector<uint8_t>>* service_data,
    uint8_t uuid_size,
    const uint8_t* data,
    uint8_t data_len) {
  CHECK(service_data && data);

  if (!data_len || data_len <= uuid_size) {
    LOG(WARNING) << "Failed to parse EIR service data";
    return;
  }

  // A service UUID and its data are presented in little-endian order where the
  // format is {<bytes of service UUID>, <bytes of service data>}. For instance,
  // the service data associated with the battery service can be
  // {0x0F, 0x18, 0x22, 0x11}
  // where {0x18 0x0F} is the UUID and {0x11, 0x22} is the data.
  Uuid uuid(GetBytesFromLE(data, uuid_size));
  CHECK_NE(UuidFormat::UUID_INVALID, uuid.format());
  service_data->emplace(uuid,
                        GetBytesFromLE(data + uuid_size, data_len - uuid_size));
}

void Newblue::ClearPropertiesUpdated(Device* device) {
  device->paired.ClearUpdated();
  device->connected.ClearUpdated();
  device->trusted.ClearUpdated();
  device->blocked.ClearUpdated();
  device->services_resolved.ClearUpdated();
  device->alias.ClearUpdated();
  device->name.ClearUpdated();
  device->tx_power.ClearUpdated();
  device->rssi.ClearUpdated();
  device->eir_class.ClearUpdated();
  device->appearance.ClearUpdated();
  device->icon.ClearUpdated();
  device->flags.ClearUpdated();
  device->service_uuids.ClearUpdated();
  device->service_data.ClearUpdated();
  device->manufacturer.ClearUpdated();
}

bool Newblue::Pair(const std::string& device_address) {
  // BlueZ doesn't allow pairing with an unknown device.
  const Device* device = FindDevice(device_address);
  if (!device)
    return false;

  struct bt_addr address;
  if (!ConvertToBtAddr(device->is_random_address, device_address, &address))
    return false;

  struct smPairSecurityRequirements security_requirement =
      DetermineSecurityRequirements(*device);
  libnewblue_->SmPair(&address, &security_requirement);
  return true;
}

bool Newblue::CancelPair(const std::string& device_address) {
  // BlueZ doesn't allow cancel pairing with an unknown device.
  const Device* device = FindDevice(device_address);
  if (!device)
    return false;

  struct bt_addr address;
  if (!ConvertToBtAddr(device->is_random_address, device_address, &address))
    return false;

  libnewblue_->SmUnpair(&address);
  return true;
}

void Newblue::PairStateCallbackThunk(void* data,
                                     const void* pair_state_change,
                                     uniq_t observer_id) {
  CHECK(data && pair_state_change);

  Newblue* newblue = static_cast<Newblue*>(data);
  const smPairStateChange* change =
      static_cast<const smPairStateChange*>(pair_state_change);

  newblue->PostTask(
      FROM_HERE, base::Bind(&Newblue::PairStateCallback, newblue->GetWeakPtr(),
                            *change, observer_id));
}

void Newblue::PasskeyDisplayObserverCallbackThunk(
    void* data,
    const struct smPasskeyDisplay* passkey_display,
    uniq_t observer_id) {
  if (!passkey_display) {
    LOG(WARNING) << "passkey display is not given";
    return;
  }

  Newblue* newblue = static_cast<Newblue*>(data);
  newblue->PostTask(
      FROM_HERE,
      base::Bind(&Newblue::PasskeyDisplayObserverCallback,
                 newblue->GetWeakPtr(), *passkey_display, observer_id));
}

void Newblue::PasskeyDisplayObserverCallback(
    struct smPasskeyDisplay passkey_display, uniq_t observer_id) {
  if (observer_id != passkey_display_observer_id_) {
    LOG(WARNING) << "passkey display observer id mismatch";
    return;
  }

  if (passkey_display.valid) {
    CHECK(pairing_agent_);
    pairing_agent_->DisplayPasskey(
        ConvertBtAddrToString(passkey_display.peerAddr),
        passkey_display.passkey);
  } else {
    VLOG(1) << "The passkey session expired with the device";
  }
}

void Newblue::PairStateCallback(const smPairStateChange& change,
                                uniq_t observer_id) {
  VLOG(1) << __func__;

  CHECK_EQ(observer_id, pair_state_handle_);

  std::string address = ConvertBtAddrToString(change.peerAddr);

  // BlueZ doesn't allow the pairing with an undiscovered device, so if
  // receiving pairing state changed with an undiscovered device, we drop it.
  Device* device = FindDevice(address);
  if (!device) {
    VLOG(1) << "Dropping the pairing state change (" << change.pairState
            << ") for an undiscovered device " << address;
    return;
  }

  PairState state = static_cast<PairState>(change.pairState);
  std::string dbus_error;

  switch (state) {
    case PairState::CANCELED:
      device->paired.SetValue(false);
      dbus_error = bluetooth_device::kErrorAuthenticationCanceled;
    case PairState::NOT_PAIRED:
      device->paired.SetValue(false);
      break;
    case PairState::FAILED:
      // If a device is previously paired, security manager will throw a
      // SM_PAIR_ERR_ALREADY_PAIRED error which should not set the pairing state
      // to false.
      device->paired.SetValue(change.pairErr == SM_PAIR_ERR_ALREADY_PAIRED);
      dbus_error = ConvertSmPairErrorToDbusError(change.pairErr);
      break;
    case PairState::PAIRED:
      device->paired.SetValue(true);
      break;
    case PairState::STARTED:
    default:
      // Do not change paired property.
      break;
  }

  // Notify |pair_observers|.
  for (const auto& observer : pair_observers_)
    observer.second.Run(*device, state, dbus_error);
}

struct smPairSecurityRequirements Newblue::DetermineSecurityRequirements(
    const Device& device) {
  struct smPairSecurityRequirements security_requirement = {.bond = false,
                                                            .mitm = false};
  bool security_determined = false;

  // TODO(mcchou): Determine the security requirement for different type of
  // devices based on appearance.
  // These value are defined at https://www.bluetooth.com/specifications/gatt/
  // viewer?attributeXmlFile=org.bluetooth.characteristic.gap.appearance.xml.
  // The translated strings come from BlueZ.
  switch ((device.appearance.value() & 0xffc0) >> 6) {
    case 0x01:  // phone
    case 0x02:  // computer
      security_requirement.bond = true;
      security_requirement.mitm = true;
      security_determined = true;
      break;
    case 0x03:  // watch
      security_requirement.bond = true;
      security_determined = true;
      break;
    case 0x0f:  // HID Generic
      switch (device.appearance.value() & 0x3f) {
        case 0x01:  // keyboard
        case 0x05:  // tablet
          security_requirement.bond = true;
          security_requirement.mitm = true;
          security_determined = true;
          break;
        case 0x02:  // mouse
        case 0x03:  // joystick
        case 0x04:  // gamepad
          security_requirement.bond = true;
          security_determined = true;
          break;
        case 0x08:  // barcode-scanner
        default:
          break;
      }
      break;
    case 0x00:  // unknown
    case 0x04:  // clock
    case 0x05:  // video-display
    case 0x06:  // remote-control
    case 0x07:  // eye-glasses
    case 0x08:  // tag
    case 0x09:  // key-ring
    case 0x0a:  // multimedia-player
    case 0x0b:  // barcode-scanner
    case 0x0c:  // thermometer
    case 0x0d:  // heart-rate-sensor
    case 0x0e:  // blood-pressure
    case 0x10:  // glucose-meter
    case 0x11:  // running-walking-sensor
    case 0x12:  // cycling
    case 0x31:  // pulse-oximeter
    case 0x32:  // weight-scale
    case 0x33:  // personal-mobility-device
    case 0x34:  // continuous-glucose-monitor
    case 0x35:  // insulin-pump
    case 0x36:  // medication-delivery
    case 0x51:  // outdoor-sports-activity
    default:
      break;
  }

  if (!security_determined) {
    security_requirement.bond = true;
    LOG(WARNING) << base::StringPrintf(
        "The default security level (bond:%s MITM:%s) will be "
        "used in pairing with device with appearance 0x%4X",
        security_requirement.bond ? "true" : "false",
        security_requirement.mitm ? "true" : "false",
        device.appearance.value());
  }

  return security_requirement;
}

Device* Newblue::FindDevice(const std::string& device_address) {
  auto iter = discovered_devices_.find(device_address);
  return iter != discovered_devices_.end() ? iter->second.get() : nullptr;
}

}  // namespace bluetooth
