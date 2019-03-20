// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arc/network/address_manager.h"

#include <map>
#include <utility>
#include <vector>

#include <arpa/inet.h>

#include <gtest/gtest.h>

namespace arc_networkd {

TEST(AddressManager, OnlyAllocateFromKnownGuests) {
  AddressManager mgr({AddressManager::Guest::ARC,
                      AddressManager::Guest::VM_TERMINA,
                      AddressManager::Guest::CONTAINER});
  EXPECT_TRUE(mgr.AllocateIPv4Subnet(AddressManager::Guest::ARC) != nullptr);
  EXPECT_TRUE(mgr.AllocateIPv4Subnet(AddressManager::Guest::VM_TERMINA) !=
              nullptr);
  EXPECT_TRUE(mgr.AllocateIPv4Subnet(AddressManager::Guest::CONTAINER) !=
              nullptr);
  EXPECT_FALSE(mgr.AllocateIPv4Subnet(AddressManager::Guest::VM_ARC) !=
               nullptr);
  EXPECT_FALSE(mgr.AllocateIPv4Subnet(AddressManager::Guest::ARC_NET) !=
               nullptr);
  EXPECT_FALSE(mgr.AllocateIPv4Subnet(AddressManager::Guest::VM_PLUGIN) !=
               nullptr);
}

TEST(AddressManager, BaseAddresses) {
  std::map<AddressManager::Guest, size_t> addrs = {
      {AddressManager::Guest::ARC, 0x64735c00},         // 100.115.92.0
      {AddressManager::Guest::VM_ARC, 0x64735c04},      // 100.115.92.4
      {AddressManager::Guest::ARC_NET, 0x64735c08},     // 100.115.92.8
      {AddressManager::Guest::VM_TERMINA, 0x64735c18},  // 100.115.92.24
      {AddressManager::Guest::VM_PLUGIN, 0x64735c80},   // 100.115.92.128
      {AddressManager::Guest::VM_TERMINA, 0x64735cc0},  // 100.115.92.192
  };
  AddressManager mgr({
      AddressManager::Guest::ARC,
      AddressManager::Guest::VM_ARC,
      AddressManager::Guest::ARC_NET,
      AddressManager::Guest::VM_TERMINA,
      AddressManager::Guest::VM_PLUGIN,
      AddressManager::Guest::CONTAINER,
  });
  for (const auto a : addrs) {
    auto subnet = mgr.AllocateIPv4Subnet(a.first);
    ASSERT_TRUE(subnet != nullptr);
    // The first address (offset 0) returned by Subnet is not the base address,
    // rather it's the first usable IP address... so the base is 1 less.
    EXPECT_EQ(a.second, ntohl(subnet->AddressAtOffset(0)) - 1);
  }
}

TEST(AddressManager, AddressesPerSubnet) {
  std::map<AddressManager::Guest, size_t> addrs = {
      {AddressManager::Guest::ARC, 2},
      {AddressManager::Guest::VM_ARC, 2},
      {AddressManager::Guest::ARC_NET, 2},
      {AddressManager::Guest::VM_TERMINA, 2},
      {AddressManager::Guest::VM_PLUGIN, 14},
      {AddressManager::Guest::VM_TERMINA, 14},
  };
  AddressManager mgr({
      AddressManager::Guest::ARC,
      AddressManager::Guest::VM_ARC,
      AddressManager::Guest::ARC_NET,
      AddressManager::Guest::VM_TERMINA,
      AddressManager::Guest::VM_PLUGIN,
      AddressManager::Guest::CONTAINER,
  });
  for (const auto a : addrs) {
    auto subnet = mgr.AllocateIPv4Subnet(a.first);
    ASSERT_TRUE(subnet != nullptr);
    EXPECT_EQ(a.second, subnet->AvailableCount());
  }
}

TEST(AddressManager, SubnetsPerPool) {
  std::map<AddressManager::Guest, size_t> addrs = {
      {AddressManager::Guest::ARC, 1},
      {AddressManager::Guest::VM_ARC, 1},
      {AddressManager::Guest::ARC_NET, 4},
      {AddressManager::Guest::VM_TERMINA, 26},
      {AddressManager::Guest::VM_PLUGIN, 1},
      {AddressManager::Guest::VM_TERMINA, 4},
  };
  AddressManager mgr({
      AddressManager::Guest::ARC,
      AddressManager::Guest::VM_ARC,
      AddressManager::Guest::ARC_NET,
      AddressManager::Guest::VM_TERMINA,
      AddressManager::Guest::VM_PLUGIN,
      AddressManager::Guest::CONTAINER,
  });
  for (const auto a : addrs) {
    std::vector<std::unique_ptr<Subnet>> subnets;
    for (size_t i = 0; i < a.second; ++i) {
      auto subnet = mgr.AllocateIPv4Subnet(a.first);
      EXPECT_TRUE(subnet != nullptr);
      subnets.emplace_back(std::move(subnet));
    }
    auto subnet = mgr.AllocateIPv4Subnet(a.first);
    EXPECT_TRUE(subnet == nullptr);
  }
}

}  // namespace arc_networkd