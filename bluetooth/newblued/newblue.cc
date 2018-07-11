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

    // TODO(sonnysasaka): Implement more EIR types.
    switch (eir_type) {
      // name
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
      case EirType::CLASS_OF_DEV:
        if (data_len != 3)
          break;
        // 24-bit little endian data
        device->eir_class = data[0] | (data[1] << 8) | (data[2] << 16);
        break;
      case EirType::GAP_APPEARANCE:
        if (data_len != 2)
          break;
        // 16-bit little endian data
        device->appearance = data[0] | (data[1] << 8);
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
    discovered_devices_[address] = std::make_unique<Device>();

  Device* device = discovered_devices_[address].get();

  device->address = address;
  device->is_random_addr = (address_type == BT_ADDR_TYPE_LE_RANDOM);
  device->rssi = rssi;
  UpdateEir(device, eir);

  device_discovered_callback_.Run(*device);
}

}  // namespace bluetooth
