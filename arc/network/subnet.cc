// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arc/network/subnet.h"

#include <arpa/inet.h>

#include <string>
#include <utility>

#include <base/bind.h>
#include <base/logging.h>

namespace arc_networkd {

SubnetAddress::SubnetAddress(uint32_t addr,
                             uint32_t prefix_length,
                             base::Closure release_cb)
    : addr_(addr),
      prefix_length_(prefix_length),
      release_cb_(std::move(release_cb)) {}

SubnetAddress::~SubnetAddress() {
  release_cb_.Run();
}

uint32_t SubnetAddress::Address() const {
  return htonl(addr_);
}

std::string SubnetAddress::ToCidrString() const {
  return IPv4AddressToCidrString(htonl(addr_), prefix_length_);
}

std::string SubnetAddress::ToIPv4String() const {
  return IPv4AddressToString(htonl(addr_));
}

Subnet::Subnet(uint32_t base_addr,
               uint32_t prefix_length,
               base::Closure release_cb)
    : network_id_(base_addr),
      prefix_length_(prefix_length),
      release_cb_(std::move(release_cb)),
      weak_factory_(this) {
  CHECK_LT(prefix_length, 32);

  addrs_.resize(1ull << (32 - prefix_length), false);

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
      addr, prefix_length_,
      base::Bind(&Subnet::Free, weak_factory_.GetWeakPtr(), offset));
}

std::unique_ptr<SubnetAddress> Subnet::AllocateAtOffset(uint32_t offset) {
  uint32_t addr = AddressAtOffset(offset);
  return (addr != INADDR_ANY) ? Allocate(ntohl(addr)) : nullptr;
}

uint32_t Subnet::AddressAtOffset(uint32_t offset) const {
  if (offset >= AvailableCount())
    return INADDR_ANY;

  // The first usable IP is after the network id.
  return htonl(network_id_ + 1 + offset);
}

uint32_t Subnet::AvailableCount() const {
  // The available IP count is all IPs in a subnet, minus the network ID
  // and the broadcast address.
  return addrs_.size() - 2;
}

uint32_t Subnet::Netmask() const {
  return htonl((0xffffffffull << (32 - prefix_length_)) & 0xffffffff);
}

uint32_t Subnet::Prefix() const {
  return htonl(network_id_) & Netmask();
}

uint32_t Subnet::PrefixLength() const {
  return prefix_length_;
}

std::string Subnet::ToCidrString() const {
  return IPv4AddressToCidrString(htonl(network_id_), prefix_length_);
}

void Subnet::Free(uint32_t offset) {
  DCHECK_NE(offset, 0);
  DCHECK_LT(offset, addrs_.size() - 1);

  addrs_[offset] = false;
}

// static
std::string IPv4AddressToString(uint32_t addr) {
  char buf[INET_ADDRSTRLEN] = {0};
  struct in_addr ia;
  ia.s_addr = addr;
  return !inet_ntop(AF_INET, &ia, buf, sizeof(buf)) ? "" : buf;
}

// static
std::string IPv4AddressToCidrString(uint32_t addr, uint32_t prefix_length) {
  return IPv4AddressToString(addr) + "/" + std::to_string(prefix_length);
}

}  // namespace arc_networkd
