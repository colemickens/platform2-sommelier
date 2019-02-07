// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "vm_tools/concierge/subnet_pool.h"

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
// +-------+----------------+-----------+------------+----------------------+
// |  0-23 |      24-127    |  128-143  |   144-191  |        192-255       |
// +-------+----------------+-----------+------------+----------------------+
// | ARC++ |  VM pool (/30) | Plugin VM | Future use | Container pool (/28) |
// +-------+----------------+-----------+------------+----------------------+
// Within each /30 subnet:
//   addr 0 - network identifier
//   addr 1 - gateway (host) address
//   addr 2 - VM (guest) address
//   addr 3 - broadcast address

constexpr size_t kContainerAddressesPerIndex = 16;
constexpr size_t kContainerBaseAddress = 0x64735cc0;  // 100.115.92.192
constexpr size_t kVmAddressesPerIndex = 4;
constexpr size_t kVmBaseAddress = 0x64735c18;  // 100.115.92.24
constexpr size_t kContainerSubnetPrefix = 28;
constexpr size_t kVmSubnetPrefix = 30;

}  // namespace

SubnetPool::~SubnetPool() {
  if (vm_subnets_.any() || container_subnets_.any()) {
    LOG(ERROR) << "SubnetPool destroyed with unreleased subnets";
  }
}

std::unique_ptr<Subnet> SubnetPool::AllocateVM() {
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

std::unique_ptr<Subnet> SubnetPool::AllocateContainer() {
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
