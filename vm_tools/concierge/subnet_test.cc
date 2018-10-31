// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "vm_tools/concierge/subnet.h"

#include <arpa/inet.h>
#include <stdint.h>

#include <base/bind.h>
#include <gtest/gtest.h>

namespace vm_tools {
namespace concierge {
namespace {

constexpr size_t kContainerBaseAddress = 0x64735cc0;  // 100.115.92.192
constexpr size_t kVmBaseAddress = 0x64735c00;         // 100.115.92.0

constexpr size_t kContainerSubnetPrefix = 28;
constexpr size_t kVmSubnetPrefix = 30;

class VmSubnetTest : public ::testing::TestWithParam<size_t> {};
class ContainerSubnetTest : public ::testing::TestWithParam<size_t> {};
class PrefixTest : public ::testing::TestWithParam<size_t> {};

void DoNothing() {}

void SetTrue(bool* value) {
  *value = true;
}

}  // namespace

TEST_P(VmSubnetTest, AddressAtOffset) {
  size_t index = GetParam();
  Subnet subnet(kVmBaseAddress + index * 4, kVmSubnetPrefix,
                base::Bind(&DoNothing));

  for (uint32_t offset = 0; offset < subnet.AvailableCount(); ++offset) {
    uint32_t address = htonl(kVmBaseAddress + index * 4 + offset + 1);
    EXPECT_EQ(address, subnet.AddressAtOffset(offset));
  }
}

INSTANTIATE_TEST_CASE_P(AllValues,
                        VmSubnetTest,
                        ::testing::Range(size_t{1}, size_t{32}));

TEST_P(ContainerSubnetTest, AddressAtOffset) {
  size_t index = GetParam();
  Subnet subnet(kContainerBaseAddress + index * 16, kContainerSubnetPrefix,
                base::Bind(&DoNothing));

  for (uint32_t offset = 0; offset < subnet.AvailableCount(); ++offset) {
    uint32_t address = htonl(kContainerBaseAddress + index * 16 + offset + 1);
    EXPECT_EQ(address, subnet.AddressAtOffset(offset));
  }
}

INSTANTIATE_TEST_CASE_P(AllValues,
                        ContainerSubnetTest,
                        ::testing::Range(size_t{1}, size_t{4}));

TEST_P(PrefixTest, AvailableCount) {
  size_t prefix = GetParam();

  Subnet subnet(0, prefix, base::Bind(&DoNothing));
  EXPECT_EQ((1ull << (32 - prefix)) - 2, subnet.AvailableCount());
}

TEST_P(PrefixTest, Netmask) {
  size_t prefix = GetParam();

  Subnet subnet(0, prefix, base::Bind(&DoNothing));
  EXPECT_EQ(htonl(0xffffffff << (32 - prefix)), subnet.Netmask());
}

INSTANTIATE_TEST_CASE_P(AllValues,
                        PrefixTest,
                        ::testing::Range(size_t{0}, size_t{32}));

// Tests that the Subnet runs the provided cleanup callback when it gets
// destroyed.
TEST(Subnet, Cleanup) {
  bool called = false;

  { Subnet subnet(0, 24, base::Bind(&SetTrue, &called)); }

  EXPECT_TRUE(called);
}

}  // namespace concierge
}  // namespace vm_tools
