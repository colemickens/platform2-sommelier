// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bluetooth/newblued/util.h"

#include <algorithm>

#include <base/stl_util.h>

namespace {

uint64_t GetNumFromLE(const uint8_t* buf, uint8_t bits) {
  uint64_t val = 0;
  uint8_t bytes = bits / 8;

  CHECK(buf);

  buf += bytes;

  while (bytes--)
    val = (val << 8) | *--buf;

  return val;
}

}  // namespace

namespace bluetooth {

// Turns the content of |buf| into a uint16_t in host order. This should be used
// when reading the little-endian data from Bluetooth packet.
uint16_t GetNumFromLE16(const uint8_t* buf) {
  return static_cast<uint16_t>(GetNumFromLE(buf, 16));
}
// Turns the content of |buf| into a uint32_t in host order. This should be used
// when reading the little-endian data from Bluetooth packet.
uint32_t GetNumFromLE24(const uint8_t* buf) {
  return static_cast<uint32_t>(GetNumFromLE(buf, 24));
}

// Reverses the content of |buf| and returns bytes in big-endian order. This
// should be used when reading the little-endian data from Bluetooth packet.
std::vector<uint8_t> GetBytesFromLE(const uint8_t* buf, size_t buf_len) {
  std::vector<uint8_t> ret;

  CHECK(buf);

  if (!buf_len)
    return ret;

  ret.assign(buf, buf + buf_len);
  std::reverse(ret.begin(), ret.end());
  return ret;
}

}  // namespace bluetooth
