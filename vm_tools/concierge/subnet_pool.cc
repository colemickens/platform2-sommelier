// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "vm_tools/concierge/subnet_pool.h"

#include <arpa/inet.h>

#include <string>
#include <utility>

#include <base/bind.h>
#include <base/logging.h>
#include <base/memory/ptr_util.h>
#include <base/strings/stringprintf.h>

using std::string;

namespace vm_tools {
namespace concierge {

namespace {

// The 100.115.92.0/24 subnet is reserved and not publicly routable. This subnet
// is then sliced into the following IP pools:
// +-------+----------------+------------+----------------------+
// |  0-3  |      4-127     |   128-191  |        192-255       |
// +-------+----------------+------------+----------------------+
// | ARC++ |  VM pool (/30) | Future use | Container pool (/28) |
// +-------+----------------+------------+----------------------+
// Within each /30 subnet:
//   addr 0 - network identifier
//   addr 1 - gateway (host) address
//   addr 2 - VM (guest) address
//   addr 3 - broadcast address

constexpr size_t kContainerAddressesPerIndex = 16;
constexpr size_t kContainerBaseAddress = 0x64735cc0;  // 100.115.92.192
constexpr size_t kVmAddressesPerIndex = 4;
constexpr size_t kVmBaseAddress = 0x64735c00;  // 100.115.92.0
constexpr size_t kContainerSubnetPrefix = 28;
constexpr size_t kVmSubnetPrefix = 30;

}  // namespace

SubnetPool::Subnet::Subnet(uint32_t network_id,
                           size_t prefix,
                           base::Closure release_cb)
    : network_id_(network_id),
      prefix_(prefix),
      release_cb_(std::move(release_cb)) {
  CHECK_LT(prefix, 32);
}

SubnetPool::Subnet::~Subnet() {
  release_cb_.Run();
}

uint32_t SubnetPool::Subnet::AddressAtOffset(uint32_t offset) const {
  if (offset >= AvailableCount())
    return INADDR_ANY;

  // The first usable IP is after the network id.
  return htonl(network_id_ + 1 + offset);
}

size_t SubnetPool::Subnet::AvailableCount() const {
  // The available IP count is all IPs in a subnet, minus the network ID
  // and the broadcast address.
  return (1 << (32 - prefix_)) - 2;
}

uint32_t SubnetPool::Subnet::Netmask() const {
  return htonl(0xffffffff << (32 - prefix_));
}

size_t SubnetPool::Subnet::Prefix() const {
  return prefix_;
}

SubnetPool::SubnetPool() {
  // The first address is always reserved for the ARC++ container.
  vm_subnets_.set(0);
}

SubnetPool::~SubnetPool() {
  // Clear the subnet reserved for ARC++ so that we can test if there are still
  // allocated subnets out in the wild.
  vm_subnets_.reset(0);

  if (vm_subnets_.any() || container_subnets_.any()) {
    LOG(ERROR) << "SubnetPool destroyed with unreleased subnets";
  }
}

std::unique_ptr<SubnetPool::Subnet> SubnetPool::CreateVMForTesting(
    size_t index) {
  DCHECK(!vm_subnets_.test(index));
  vm_subnets_.set(index);
  return base::WrapUnique(new Subnet(
      kVmBaseAddress + (index * kVmAddressesPerIndex), kVmSubnetPrefix,
      base::Bind(&SubnetPool::ReleaseVM, weak_ptr_factory_.GetWeakPtr(),
                 index)));
}

std::unique_ptr<SubnetPool::Subnet> SubnetPool::CreateContainerForTesting(
    size_t index) {
  DCHECK(!container_subnets_.test(index));
  container_subnets_.set(index);
  return base::WrapUnique(
      new Subnet(kContainerBaseAddress + (index * kContainerAddressesPerIndex),
                 kContainerSubnetPrefix,
                 base::Bind(&SubnetPool::ReleaseContainer,
                            weak_ptr_factory_.GetWeakPtr(), index)));
}

std::unique_ptr<SubnetPool::Subnet> SubnetPool::AllocateVM() {
  // Find the first un-allocated subnet.
  size_t index = 0;
  while (index < vm_subnets_.size() && vm_subnets_.test(index)) {
    ++index;
  }

  if (index == vm_subnets_.size()) {
    // All subnets are allocated.
    return nullptr;
  }

  vm_subnets_.set(index);
  return base::WrapUnique(new Subnet(
      kVmBaseAddress + (index * kVmAddressesPerIndex), kVmSubnetPrefix,
      base::Bind(&SubnetPool::ReleaseVM, weak_ptr_factory_.GetWeakPtr(),
                 index)));
}

std::unique_ptr<SubnetPool::Subnet> SubnetPool::AllocateContainer() {
  // Find the first un-allocated subnet.
  size_t index = 0;
  while (index < container_subnets_.size() && container_subnets_.test(index)) {
    ++index;
  }

  if (index == container_subnets_.size()) {
    // All subnets are allocated.
    return nullptr;
  }

  container_subnets_.set(index);
  return base::WrapUnique(
      new Subnet(kContainerBaseAddress + (index * kContainerAddressesPerIndex),
                 kContainerSubnetPrefix,
                 base::Bind(&SubnetPool::ReleaseContainer,
                            weak_ptr_factory_.GetWeakPtr(), index)));
}

void SubnetPool::ReleaseVM(size_t index) {
  DCHECK(vm_subnets_.test(index));
  vm_subnets_.reset(index);
}

void SubnetPool::ReleaseContainer(size_t index) {
  DCHECK(container_subnets_.test(index));
  container_subnets_.reset(index);
}

}  // namespace concierge
}  // namespace vm_tools
