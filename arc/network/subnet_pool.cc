// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arc/network/subnet_pool.h"

#include <memory>
#include <string>
#include <utility>

#include <base/bind.h>
#include <base/logging.h>
#include <base/memory/ptr_util.h>
#include <base/strings/stringprintf.h>

using std::string;

namespace arc_networkd {
namespace {
constexpr uint32_t kMaxSubnets = 32;
}  // namespace

// static
std::unique_ptr<SubnetPool> SubnetPool::New(uint32_t base_addr,
                                            uint32_t prefix_length,
                                            uint32_t num_subnets) {
  if (num_subnets > kMaxSubnets) {
    LOG(ERROR) << "Maximum subnets supported is " << kMaxSubnets << "; got "
               << num_subnets;
    return nullptr;
  }
  return base::WrapUnique(
      new SubnetPool(base_addr, prefix_length, num_subnets));
}

SubnetPool::SubnetPool(uint32_t base_addr,
                       uint32_t prefix_length,
                       uint32_t num_subnets)
    : base_addr_(base_addr),
      prefix_length_(prefix_length),
      num_subnets_(num_subnets),
      addr_per_index_(1ull << (kMaxSubnets - prefix_length)) {}

SubnetPool::~SubnetPool() {
  if (subnets_.any()) {
    LOG(ERROR) << "SubnetPool destroyed with unreleased subnets";
  }
}

std::unique_ptr<Subnet> SubnetPool::Allocate() {
  // Find the first un-allocated subnet.
  uint32_t index = 0;
  while (index < num_subnets_ && subnets_.test(index)) {
    ++index;
  }

  if (index == num_subnets_) {
    // All subnets are allocated.
    return nullptr;
  }

  subnets_.set(index);
  return std::make_unique<Subnet>(
      base_addr_ + (index * addr_per_index_), prefix_length_,
      base::Bind(&SubnetPool::Release, weak_ptr_factory_.GetWeakPtr(), index));
}

void SubnetPool::Release(uint32_t index) {
  DCHECK(subnets_.test(index));
  subnets_.reset(index);
}

}  // namespace arc_networkd
