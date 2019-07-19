// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arc/network/net_util.h"

#include <arpa/inet.h>

#include <base/strings/stringprintf.h>

namespace arc_networkd {

std::string IPv4AddressToString(uint32_t addr) {
  char buf[INET_ADDRSTRLEN] = {0};
  struct in_addr ia;
  ia.s_addr = addr;
  return !inet_ntop(AF_INET, &ia, buf, sizeof(buf)) ? "" : buf;
}

std::string IPv4AddressToCidrString(uint32_t addr, uint32_t prefix_length) {
  return IPv4AddressToString(addr) + "/" + std::to_string(prefix_length);
}

std::string MacAddressToString(const MacAddress& addr) {
  return base::StringPrintf("%02x:%02x:%02x:%02x:%02x:%02x", addr[0], addr[1],
                            addr[2], addr[3], addr[4], addr[5]);
}

}  // namespace arc_networkd
