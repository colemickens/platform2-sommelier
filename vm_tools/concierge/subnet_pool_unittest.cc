// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <algorithm>
#include <deque>
#include <memory>
#include <string>
#include <utility>

#include <base/rand_util.h>
#include <base/strings/stringprintf.h>
#include <gtest/gtest.h>

#include "vm_tools/concierge/subnet_pool.h"

using std::string;

namespace vm_tools {
namespace concierge {
namespace {

// The 100.115.92.0/24 subnet is reserved and cannot be publicly routed.
constexpr char kIpFormat[] = "100.115.92.%hhu";

// The first subnet that will be allocated by the SubnetPool.  Subnet 0 is
// reserved for ARC++.
constexpr size_t kFirstSubnet = 1;

// The maximum number of subnets that can be allocated at a given time.
constexpr size_t kMaxSubnets = 64;

class SubnetTest : public ::testing::TestWithParam<size_t> {
 protected:
  SubnetPool pool_;
};

}  // namespace

TEST_P(SubnetTest, GatewayAddress) {
  size_t index = GetParam();
  auto subnet = pool_.CreateForTesting(index);
  ASSERT_TRUE(subnet);

  uint8_t octet = index * 4 + 1;
  string expected = base::StringPrintf(kIpFormat, octet);
  EXPECT_EQ(expected, subnet->GatewayAddress());
}

TEST_P(SubnetTest, IPv4Address) {
  size_t index = GetParam();
  auto subnet = pool_.CreateForTesting(index);
  ASSERT_TRUE(subnet);

  uint8_t octet = index * 4 + 2;
  string expected = base::StringPrintf(kIpFormat, octet);
  EXPECT_EQ(expected, subnet->IPv4Address());
}

INSTANTIATE_TEST_CASE_P(AllValues,
                        SubnetTest,
                        ::testing::Range(kFirstSubnet, kMaxSubnets));

// Tests that the SubnetPool does not allocate more than 64 subnets at a time.
TEST(SubnetPool, AllocationRange) {
  SubnetPool pool;

  std::deque<std::unique_ptr<SubnetPool::Subnet>> subnets;
  for (size_t i = kFirstSubnet; i < kMaxSubnets; ++i) {
    auto subnet = pool.Allocate();
    ASSERT_TRUE(subnet);

    subnets.emplace_back(std::move(subnet));
  }

  EXPECT_FALSE(pool.Allocate());
}

// Tests that subnets are properly released and reused.
TEST(SubnetPool, Release) {
  SubnetPool pool;

  // First allocate all the subnets.
  std::deque<std::unique_ptr<SubnetPool::Subnet>> subnets;
  for (size_t i = kFirstSubnet; i < kMaxSubnets; ++i) {
    auto subnet = pool.Allocate();
    ASSERT_TRUE(subnet);

    subnets.emplace_back(std::move(subnet));
  }
  ASSERT_FALSE(pool.Allocate());

  // Now shuffle the elements.
  std::random_shuffle(subnets.begin(), subnets.end(), base::RandGenerator);

  // Pop off the first element.
  auto subnet = std::move(subnets.front());
  subnets.pop_front();

  // Store the gateway and address for testing later.
  string gateway = subnet->GatewayAddress();
  string address = subnet->IPv4Address();

  // Release the subnet.
  subnet.reset();

  // Get a new subnet.
  subnet = pool.Allocate();
  ASSERT_TRUE(subnet);

  EXPECT_EQ(gateway, subnet->GatewayAddress());
  EXPECT_EQ(address, subnet->IPv4Address());
}

}  // namespace concierge
}  // namespace vm_tools
