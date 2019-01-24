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
constexpr size_t kMaxSubnets = 26;

}  // namespace

// Tests that the SubnetPool does not allocate more than 26 subnets at a time.
TEST(SubnetPool, AllocationRange) {
  SubnetPool pool;

  std::deque<std::unique_ptr<Subnet>> subnets;
  for (size_t i = 0; i < kMaxSubnets; ++i) {
    auto subnet = pool.AllocateVM();
    ASSERT_TRUE(subnet);

    subnets.emplace_back(std::move(subnet));
  }

  EXPECT_FALSE(pool.AllocateVM());
}

// Tests that subnets are properly released and reused.
TEST(SubnetPool, Release) {
  SubnetPool pool;

  // First allocate all the subnets.
  std::deque<std::unique_ptr<Subnet>> subnets;
  for (size_t i = 0; i < kMaxSubnets; ++i) {
    auto subnet = pool.AllocateVM();
    ASSERT_TRUE(subnet);

    subnets.emplace_back(std::move(subnet));
  }
  ASSERT_FALSE(pool.AllocateVM());

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
  subnet = pool.AllocateVM();
  ASSERT_TRUE(subnet);

  EXPECT_EQ(gateway, subnet->AddressAtOffset(0));
  EXPECT_EQ(address, subnet->AddressAtOffset(1));
}

}  // namespace arc_networkd
