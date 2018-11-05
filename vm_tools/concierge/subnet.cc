// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "vm_tools/concierge/subnet.h"

#include <arpa/inet.h>

#include <utility>

#include <base/logging.h>

namespace vm_tools {
namespace concierge {

Subnet::Subnet(uint32_t network_id, size_t prefix, base::Closure release_cb)
    : network_id_(network_id),
      prefix_(prefix),
      release_cb_(std::move(release_cb)) {
  CHECK_LT(prefix, 32);
}

Subnet::~Subnet() {
  release_cb_.Run();
}

uint32_t Subnet::AddressAtOffset(uint32_t offset) const {
  if (offset >= AvailableCount())
    return INADDR_ANY;

  // The first usable IP is after the network id.
  return htonl(network_id_ + 1 + offset);
}

size_t Subnet::AvailableCount() const {
  // The available IP count is all IPs in a subnet, minus the network ID
  // and the broadcast address.
  return (1ull << (32 - prefix_)) - 2;
}

uint32_t Subnet::Netmask() const {
  return htonl((0xffffffffull << (32 - prefix_)) & 0xffffffff);
}

size_t Subnet::Prefix() const {
  return prefix_;
}

}  // namespace concierge
}  // namespace vm_tools
