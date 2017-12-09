// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "vm_tools/concierge/subnet_pool.h"

#include <utility>

#include <base/bind.h>
#include <base/logging.h>
#include <base/memory/ptr_util.h>
#include <base/strings/stringprintf.h>

using std::string;

namespace vm_tools {
namespace concierge {

namespace {

// Format for producing IPv4 addresses from the subnet.
constexpr char kIpFormat[] = "100.115.92.%hhu";

// The 100.115.92.0/24 subnet is reserved and not publicly routable.  This /24
// subnet is split up into 64 /30 subnets.  Within each subnet:
//   addr 0 - network identifier
//   addr 1 - gateway (host) address
//   addr 2 - VM (guest) address
//   addr 3 - broadcast address

constexpr size_t kAddressesPerIndex = 4;
constexpr size_t kGatewayOffset = 1;
constexpr size_t kIPAddressOffset = 2;

}  // namespace

SubnetPool::Subnet::Subnet(size_t index, base::Closure release_cb)
    : index_(index), release_cb_(std::move(release_cb)) {}

SubnetPool::Subnet::~Subnet() {
  release_cb_.Run();
}

string SubnetPool::Subnet::GatewayAddress() const {
  uint8_t last_octet = (index_ * kAddressesPerIndex) + kGatewayOffset;

  return base::StringPrintf(kIpFormat, last_octet);
}

string SubnetPool::Subnet::IPv4Address() const {
  uint8_t last_octet = (index_ * kAddressesPerIndex) + kIPAddressOffset;

  return base::StringPrintf(kIpFormat, last_octet);
}

string SubnetPool::Subnet::Netmask() const {
  return "255.255.255.252";
}

SubnetPool::SubnetPool() {
  // The first address is always reserved for the ARC++ container.
  subnets_.set(0);
}

SubnetPool::~SubnetPool() {
  // Clear the subnet reserved for ARC++ so that we can test if there are still
  // allocated subnets out in the wild.
  subnets_.reset(0);

  if (subnets_.any()) {
    LOG(ERROR) << "SubnetPool destroyed with unreleased subnets";
  }
}

std::unique_ptr<SubnetPool::Subnet> SubnetPool::CreateForTesting(size_t index) {
  DCHECK(!subnets_.test(index));
  subnets_.set(index);
  return base::WrapUnique(new Subnet(
      index,
      base::Bind(&SubnetPool::Release, weak_ptr_factory_.GetWeakPtr(), index)));
}

std::unique_ptr<SubnetPool::Subnet> SubnetPool::Allocate() {
  // Find the first un-allocated subnet.
  size_t index = 0;
  while (index < subnets_.size() && subnets_.test(index)) {
    ++index;
  }

  if (index == subnets_.size()) {
    // All subnets are allocated.
    return nullptr;
  }

  subnets_.set(index);
  return base::WrapUnique(new Subnet(
      index,
      base::Bind(&SubnetPool::Release, weak_ptr_factory_.GetWeakPtr(), index)));
}

void SubnetPool::Release(size_t index) {
  DCHECK(subnets_.test(index));
  subnets_.reset(index);
}

}  // namespace concierge
}  // namespace vm_tools
