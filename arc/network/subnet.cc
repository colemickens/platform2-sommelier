// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arc/network/subnet.h"

#include <arpa/inet.h>

#include <utility>

#include <base/bind.h>
#include <base/logging.h>

namespace arc_networkd {

SubnetAddress::SubnetAddress(uint32_t addr, base::Closure release_cb)
    : addr_(addr), release_cb_(std::move(release_cb)) {}

SubnetAddress::~SubnetAddress() {
  release_cb_.Run();
}

uint32_t SubnetAddress::Address() const {
  return htonl(addr_);
}

Subnet::Subnet(uint32_t network_id, size_t prefix, base::Closure release_cb)
    : network_id_(network_id),
      prefix_(prefix),
      release_cb_(std::move(release_cb)),
      weak_factory_(this) {
  CHECK_LT(prefix, 32);

  addrs_.resize(1ull << (32 - prefix), false);

  // Mark the network id and broadcast address as allocated.
  addrs_.front() = true;
  addrs_.back() = true;
}

Subnet::~Subnet() {
  release_cb_.Run();
}

std::unique_ptr<SubnetAddress> Subnet::Allocate(uint32_t addr) {
  if (addr <= network_id_ || addr >= network_id_ + addrs_.size() - 1) {
    // Address is out of bounds.
    return nullptr;
  }

  uint32_t offset = addr - network_id_;
  if (addrs_[offset]) {
    // Address is already allocated.
    return nullptr;
  }

  addrs_[offset] = true;
  return std::make_unique<SubnetAddress>(
      addr, base::Bind(&Subnet::Free, weak_factory_.GetWeakPtr(), offset));
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
  return addrs_.size() - 2;
}

uint32_t Subnet::Netmask() const {
  return htonl((0xffffffffull << (32 - prefix_)) & 0xffffffff);
}

size_t Subnet::Prefix() const {
  return prefix_;
}

void Subnet::Free(uint32_t offset) {
  DCHECK_NE(offset, 0);
  DCHECK_LT(offset, addrs_.size() - 1);

  addrs_[offset] = false;
}

}  // namespace arc_networkd
