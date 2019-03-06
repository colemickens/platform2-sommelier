// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <arpa/inet.h>
#include <stdint.h>

#include <algorithm>
#include <deque>
#include <memory>
#include <string>
#include <utility>

#include <base/rand_util.h>
#include <base/strings/stringprintf.h>
#include <gtest/gtest.h>

#include "arc/network/subnet.h"
#include "arc/network/subnet_pool.h"

using std::string;

namespace arc_networkd {
namespace {

// The maximum number of subnets that can be allocated at a given time.
constexpr uint32_t kMaxSubnets = 32;
constexpr uint32_t kBaseAddress = 0x44556677;
constexpr uint32_t kPrefix = 30;

}  // namespace

// Tests cannot create a pool with more than 32 supported subnets.
TEST(SubnetPool, MaxSubnets) {
  auto pool = SubnetPool::New(kBaseAddress, kPrefix, kMaxSubnets + 1);
  EXPECT_TRUE(pool == nullptr);
}

// Tests that the SubnetPool does not allocate more than max subnets at a time.
TEST(SubnetPool, AllocationRange) {
  auto pool = SubnetPool::New(kBaseAddress, kPrefix, kMaxSubnets);

  std::deque<std::unique_ptr<Subnet>> subnets;
  for (size_t i = 0; i < kMaxSubnets; ++i) {
    auto subnet = pool->Allocate();
    ASSERT_TRUE(subnet);

    subnets.emplace_back(std::move(subnet));
  }

  EXPECT_FALSE(pool->Allocate());
}

// Tests that subnets are properly released and reused.
TEST(SubnetPool, Release) {
  auto pool = SubnetPool::New(kBaseAddress, kPrefix, kMaxSubnets);

  // First allocate all the subnets.
  std::deque<std::unique_ptr<Subnet>> subnets;
  for (size_t i = 0; i < kMaxSubnets; ++i) {
    auto subnet = pool->Allocate();
    ASSERT_TRUE(subnet);

    subnets.emplace_back(std::move(subnet));
  }
  ASSERT_FALSE(pool->Allocate());

  // Now shuffle the elements.
  std::random_shuffle(subnets.begin(), subnets.end(), base::RandGenerator);

  // Pop off the first element.
  auto subnet = std::move(subnets.front());
  subnets.pop_front();

  // Store the gateway and address for testing later.
  uint32_t gateway = subnet->AddressAtOffset(0);
  uint32_t address = subnet->AddressAtOffset(1);

  // Release the subnet.
  subnet.reset();

  // Get a new subnet.
  subnet = pool->Allocate();
  ASSERT_TRUE(subnet);

  EXPECT_EQ(gateway, subnet->AddressAtOffset(0));
  EXPECT_EQ(address, subnet->AddressAtOffset(1));
}

}  // namespace arc_networkd
