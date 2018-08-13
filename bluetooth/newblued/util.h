// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLUETOOTH_NEWBLUED_UTIL_H_
#define BLUETOOTH_NEWBLUED_UTIL_H_

#include <stdint.h>

#include <newblue/bt.h>

#include <string>
#include <vector>

namespace bluetooth {

using UniqueId = uint64_t;

constexpr UniqueId kInvalidUniqueId = 0;

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

// Converts device object path to device address, e.g.
// /org/bluez/hci0/dev_00_01_02_03_04_05 will be 00:01:02:03:04:05.
// Return a valid address if |path| is valid; empty string otherwise.
std::string ConvertDeviceObjectPathToAddress(const std::string& path);

}  // namespace bluetooth

#endif  // BLUETOOTH_NEWBLUED_UTIL_H_
