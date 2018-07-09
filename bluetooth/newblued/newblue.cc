// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bluetooth/newblued/newblue.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include <base/bind.h>
#include <base/message_loop/message_loop.h>
#include <base/strings/stringprintf.h>

#include "bluetooth/newblued/util.h"

namespace bluetooth {

namespace {

// Filters out non-ASCII characters from a string by replacing them with spaces.
std::string AsciiString(std::string name) {
  /* Replace all non-ASCII characters with spaces */
  for (auto& ch : name) {
    if (!isascii(ch))
      ch = ' ';
  }

  return name;
}

}  // namespace

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

  if (!libnewblue_->SmInit(HCI_DISP_CAP_NONE)) {
    LOG(ERROR) << "Failed to init SM";
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
  uint8_t pos = 0;

  // Reset service UUIDs and service data so that |service_uuids| and
  // |service_data| reflect the latest EIR. This is different from BlueZ where
  // it memorizes all service UUIDs and service data ever received for the same
  // device.
  device->service_uuids.clear();
  device->service_data.clear();

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
        // Flags field can be 0 or more octets long. If the length is 1, then
        // flags[0] is octet[0]. Store only octet[0] for now due to lack of
        // definition of the following octets in Supplement to the Bluetooth
        // Core Specification.
        if (data_len > 0)
          device->flags.assign(data, data + 1);
        else
          device->flags.clear();
        break;

      // If there are more than one instance of either COMPLETE or INCOMPLETE
      // type of a UUID size, the later one(s) will be cached as well.
      case EirType::UUID16_INCOMPLETE:
      case EirType::UUID16_COMPLETE:
        UpdateServiceUuids(device, kUuid16Size, data, data_len);
        break;
      case EirType::UUID32_INCOMPLETE:
      case EirType::UUID32_COMPLETE:
        UpdateServiceUuids(device, kUuid32Size, data, data_len);
        break;
      case EirType::UUID128_INCOMPLETE:
      case EirType::UUID128_COMPLETE:
        UpdateServiceUuids(device, kUuid128Size, data, data_len);
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

        device->name = AsciiString(c_name);
      } break;

      case EirType::TX_POWER:
        if (data_len == 1)
          device->tx_power = static_cast<int8_t>(*data);
        break;
      case EirType::CLASS_OF_DEV:
        // 24-bit little endian data
        if (data_len == 3)
          device->eir_class = GetNumFromLE24(data);
        break;

      // If the UUID already exists, the service data will be updated.
      case EirType::SVC_DATA16:
        UpdateServiceData(device, kUuid16Size, data, data_len);
        break;
      case EirType::SVC_DATA32:
        UpdateServiceData(device, kUuid32Size, data, data_len);
        break;
      case EirType::SVC_DATA128:
        UpdateServiceData(device, kUuid128Size, data, data_len);
        break;

      case EirType::GAP_APPEARANCE:
        // 16-bit little endian data
        if (data_len == 2)
          device->appearance = GetNumFromLE16(data);
        break;
      case EirType::MANUFACTURER_DATA:
        if (data_len >= 2)
          device->manufacturer_id = GetNumFromLE16(data);
        // The order of manufacturer data is not specified explicitly in
        // Supplement to the Bluetooth Core Specification, so the original
        // order used in BlueZ is adopted.
        if (data_len > 2)
          device->manufacturer_data.assign(data + 2, data + data_len);
        break;
      default:
        // Do nothing for unhandled EIR type.
        break;
    }
    pos += field_len + 1;
  }
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
  if (!ready_for_up_callback_.is_null())
    ready_for_up_callback_.Run();
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
  const uint8_t* mac = addr->addr;
  std::string address =
      base::StringPrintf("%02X:%02X:%02X:%02X:%02X:%02X", mac[5], mac[4],
                         mac[3], mac[2], mac[1], mac[0]);
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

  if (!base::ContainsKey(discovered_devices_, address))
    discovered_devices_[address] = std::make_unique<Device>(address);

  Device* device = discovered_devices_[address].get();
  device->is_random_address = (address_type == BT_ADDR_TYPE_LE_RANDOM);
  device->rssi = rssi;
  UpdateEir(device, eir);

  device_discovered_callback_.Run(*device);
}

void Newblue::UpdateServiceUuids(Device* device,
                                 uint8_t uuid_size,
                                 const uint8_t* data,
                                 uint8_t data_len) {
  CHECK(device && data);

  if (!data_len || data_len % uuid_size != 0) {
    LOG(WARNING) << "Failed to parse EIR service UUIDs";
    return;
  }

  // Service UUIDs are presented in little-endian order.
  for (uint8_t i = 0; i < data_len; i += uuid_size) {
    Uuid uuid(GetBytesFromLE(data + i, uuid_size));
    CHECK(uuid.format() != UuidFormat::UUID_INVALID);
    device->service_uuids.insert(uuid);
  }
}

void Newblue::UpdateServiceData(Device* device,
                                uint8_t uuid_size,
                                const uint8_t* data,
                                uint8_t data_len) {
  CHECK(device && data);

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
  device->service_data[uuid] =
      GetBytesFromLE(data + uuid_size, data_len - uuid_size);
}

}  // namespace bluetooth
