// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arc/network/mac_address_generator.h"

#include <base/rand_util.h>

namespace arc_networkd {

MacAddress MacAddressGenerator::Generate() {
  MacAddress addr;
  do {
    base::RandBytes(addr.data(), addr.size());

    // Set the locally administered flag.
    addr[0] |= static_cast<uint8_t>(0x02);

    // Unset the multicast flag.
    addr[0] &= static_cast<uint8_t>(0xfe);
  } while (addrs_.find(addr) != addrs_.end());

  addrs_.insert(addr);

  return addr;
}

bool MacAddressGenerator::Insert(const MacAddress& addr) {
  // Validate the address.
  if ((addr[0] & 0x02U) == 0) {
    // Locally administered bit is not set.
    return false;
  }
  if ((addr[0] & 0x01U) != 0) {
    // Multicast bit is set.
    return false;
  }

  addrs_.insert(addr);

  return true;
}

}  // namespace arc_networkd
