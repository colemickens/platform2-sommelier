// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLUETOOTH_NEWBLUED_UTIL_H_
#define BLUETOOTH_NEWBLUED_UTIL_H_

#include <stdint.h>
#include <vector>

namespace bluetooth {

uint16_t GetNumFromLE16(const uint8_t* buf);
uint32_t GetNumFromLE24(const uint8_t* buf);
std::vector<uint8_t> GetBytesFromLE(const uint8_t* buf, size_t buf_len);

}  // namespace bluetooth

#endif  // BLUETOOTH_NEWBLUED_UTIL_H_
