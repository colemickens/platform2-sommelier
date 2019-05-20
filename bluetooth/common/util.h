// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLUETOOTH_COMMON_UTIL_H_
#define BLUETOOTH_COMMON_UTIL_H_

#include <stdint.h>

#include <cstdint>
#include <string>
#include <vector>

#include <newblue/bt.h>
#include <newblue/uuid.h>

#include "bluetooth/common/uuid.h"

namespace bluetooth {

using UniqueId = uint64_t;

constexpr char kAdapterObjectPath[] = "/org/bluez/hci0";

constexpr UniqueId kInvalidUniqueId = 0;

constexpr int16_t kInvalidServiceHandle = -1;
constexpr int32_t kInvalidCharacteristicHandle = -1;
constexpr int16_t kInvalidDescriptorHandle = -1;

// Returns whether LE splitter is enabled based on config in /var/lib/bluetooth.
bool IsBleSplitterEnabled();

uint16_t GetNumFromLE16(const uint8_t* buf);
uint32_t GetNumFromLE24(const uint8_t* buf);
std::vector<uint8_t> GetBytesFromLE(const uint8_t* buf, size_t buf_len);

// Retrieves a unique identifier which can be used for tracking the clients and
// the data associated with them.
UniqueId GetNextId();

// Converts device MAC address (e.g. "00:01:02:03:04:05") to struct bt_addr.
// |result| is valid only if true is returned.
bool ConvertToBtAddr(bool is_random_address,
                     const std::string& addr,
                     struct bt_addr* result);

bool TrimAdapterFromObjectPath(std::string* path);
std::string TrimDeviceFromObjectPath(std::string* device);
int32_t TrimServiceFromObjectPath(std::string* service);
int32_t TrimCharacteristicFromObjectPath(std::string* characteristic);
int32_t TrimDescriptorFromObjectPath(std::string* descriptor);

// Converts device object path to device address, e.g.
// /org/bluez/hci0/dev_00_01_02_03_04_05 will be 00:01:02:03:04:05.
// Return a valid address if |path| is valid; empty string otherwise.
std::string ConvertDeviceObjectPathToAddress(const std::string& path);

// Converts device object path from device address, e.g.
// 00:01:02:03:04:05 will be /org/bluez/hci0/dev_00_01_02_03_04_05
std::string ConvertDeviceAddressToObjectPath(const std::string& address);

// Converts GATT service object path to service handle, e.g.
// /org/bluez/hci0/dev_00_01_02_03_04_05/service01FF will return true with
// device address 00:01:02:03:04:05 and service handle 0X01FF.
bool ConvertServiceObjectPathToHandle(std::string* address,
                                      uint16_t* handle,
                                      const std::string& path);

// Converts service handle to service object path, e.g. with device address
// 00:01:02:03:04:05 and service handle 0X01FF, this will return
// /org/bluez/hci0/dev_00_01_02_03_04_05/service01FF.
std::string ConvertServiceHandleToObjectPath(const std::string& address,
                                             uint16_t handle);

// Converts GATT characteristic object path to service handle, e.g.
// /org/bluez/hci0/dev_00_01_02_03_04_05/service01FF/char0123 will return true
// with device address 00:01:02:03:04:05, service handle 0X01FF and
// characteristic handle 0x0123.
bool ConvertCharacteristicObjectPathToHandles(std::string* address,
                                              uint16_t* service_handle,
                                              uint16_t* char_handle,
                                              const std::string& path);

// Converts characteristic handle to characteristic object path, e.g. with
// device address 00:01:02:03:04:05, service handle 0X01FF and characteristic
// handle 0x0123, this will return
// /org/bluez/hci0/dev_00_01_02_03_04_05/service01FF/char0123.
std::string ConvertCharacteristicHandleToObjectPath(const std::string& address,
                                                    uint16_t service_handle,
                                                    uint16_t char_handle);

// Converts GATT descriptor object path to service handle, e.g.
// /org/bluez/hci0/dev_00_01_02_03_04_05/service01FF/char0123/descriptor0012
// will return true with device address 00:01:02:03:04:05, service handle
// 0X01FF, characteristic handle 0x0123 and descriptor handle 0x0012.
bool ConvertDescriptorObjectPathToHandles(std::string* address,
                                          uint16_t* service_handle,
                                          uint16_t* char_handle,
                                          uint16_t* desc_handle,
                                          const std::string& path);

// Converts descriptor handle to descriptor object path, e.g. with device
// address 00:01:02:03:04:05, service handle 0X01FF, characteristic handle
// 0x0123 and descriptor handle 0x0012, this will return
// /org/bluez/hci0/dev_00_01_02_03_04_05/service01FF/char0123/descriptor0012.
std::string ConvertDescriptorHandleToObjectPath(const std::string& address,
                                                uint16_t service_handle,
                                                uint16_t char_handle,
                                                uint16_t desc_handle);

// Converts struct uuid to bluetooth::Uuid.
Uuid ConvertToUuid(const struct uuid& from);

// Called when an interface of a D-Bus object is exported.
// At the moment this function only does VLOG, but it's commonly used by some
// components so it's suitable to live in this util file.
void OnInterfaceExported(std::string object_path,
                         std::string interface_name,
                         bool success);

}  // namespace bluetooth

#endif  // BLUETOOTH_COMMON_UTIL_H_
