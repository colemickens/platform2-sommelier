// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "portier/neighbor_cache.h"

#include <linux/neighbour.h>

#include <string>

#include <gtest/gtest.h>
#include <shill/net/ip_address.h>

namespace portier {

using std::string;

using base::TimeDelta;
using base::TimeTicks;
using shill::IPAddress;

namespace {

constexpr TimeDelta kEntryExpiryTimeout = TimeDelta::FromSeconds(30);

// Time differences which will not cause an entry to become expired.
constexpr TimeDelta kSmallTimeDiff1 = TimeDelta::FromSeconds(1);
constexpr TimeDelta kSmallTimeDiff2 = TimeDelta::FromSeconds(12);
constexpr TimeDelta kSmallTimeDiff3 = TimeDelta::FromSeconds(29);

// Time differences which will cause an entry to become expired.
constexpr TimeDelta kLargeTimeDiff1 = TimeDelta::FromSeconds(30);
constexpr TimeDelta kLargeTimeDiff2 = TimeDelta::FromMinutes(30);
constexpr TimeDelta kLargeTimeDiff3 = TimeDelta::FromHours(30);

constexpr char kGroupName1[] = "lan";
constexpr char kGroupName2[] = "wifi";

constexpr char kUpsteamInterface1[] = "eth0";
constexpr char kUpsteamInterface2[] = "wan0";
constexpr char kDownstreamInterface1[] = "vmtap0";
constexpr char kDownstreamInterface2[] = "vmtap1";

const IPAddress kRouterAddress1("2620:15c:202:201::1");
const LLAddress kRouterMacAddress1(LLAddress::Type::kEui48,
                                   "00:00:5e:00:02:65");
constexpr uint8_t kRouterNudState1 = NUD_STALE;

const IPAddress kRouterAddress2("2620:15c:202:201::2");
const LLAddress kRouterMacAddress2(LLAddress::Type::kEui48,
                                   "ac:4b:c8:4c:c7:f0");
constexpr uint8_t kRouterNudState2 = NUD_REACHABLE;

const IPAddress kRouterAddress3("2620:15c:202:201::3");
const LLAddress kRouterMacAddress3(LLAddress::Type::kEui48,
                                   "ac:4b:c8:4c:66:33");
constexpr uint8_t kRouterNudState3 = NUD_DELAY;

const IPAddress kNodeAddress1("2620:15c:202:201:f155:a038:ae18:faf2");
const LLAddress kNodeMacAddress1(LLAddress::Type::kEui48, "a0:8c:fd:c3:b3:c0");
constexpr uint8_t kNodeNudState1 = NUD_REACHABLE;

const IPAddress kNodeAddress2("2620:15c:202:201:3c20:87b0:b0ce:23f4");
const LLAddress kNodeMacAddress2(LLAddress::Type::kEui48, "a0:8c:fd:c3:b3:bf");
constexpr uint8_t kNodeNudState2 = NUD_INCOMPLETE;

const IPAddress kNodeAddress3("fe80::ae4b:c805:bf4c:c7f0");
const LLAddress kNodeMacAddress3(LLAddress::Type::kEui48, "ac:4b:c8:4c:c7:f0");
constexpr uint8_t kNodeNudState3 = NUD_STALE;

const IPAddress kNodeAddress4("fe80::200:5eff:fe00:265");
const LLAddress kNodeMacAddress4(LLAddress::Type::kEui48, "00:00:5e:00:02:65");
constexpr uint8_t kNodeNudState4 = NUD_STALE;

const IPAddress kNodeAddress5("2401:fa00:480:56:5a6d:8fff:fe99:e5be");
const LLAddress kNodeMacAddress5(LLAddress::Type::kEui48, "5a:6d:8f:99:e5:be");
constexpr uint8_t kNodeNudState5 = NUD_STALE;

// Bad data.
const IPAddress kIPv4Address("127.0.0.1");
const LLAddress kBadMacAddress(LLAddress::Type::kEui48, "adasdasddsadssdads");
constexpr char kBadGroupName[] = "";
constexpr char kBadInterfaceName[] = "";
constexpr uint8_t kBadNudState = NUD_NONE;

bool CompareEntries(const NeighborCacheEntry& entry1,
                    const NeighborCacheEntry& entry2) {
  return entry1.ip_address.Equals(entry2.ip_address) &&
         entry1.ll_address.Equals(entry2.ll_address) &&
         entry1.if_name == entry2.if_name &&
         entry1.nud_state == entry2.nud_state;
}

}  // namespace

//  Neighbor Layout
//                          +---------------+
//                          |   lan group   |
//   Router 1 (STALE)       |               |     Node 1 (REACHABLE)
//   Node 3   (STALE)   ----+ eth0   vmtap0 +---- Node 2 (INCOMPLETE)
//                          +---------------+
//
//                          +---------------+
//                          |  wifi group   |
//   Router 2 (REACHABLE)   |               |     Node 4 (STALE)
//   Router 3 (DELAY)   ----+ wan0   vmtap1 +---- Node 5 (STALE)
//                          +---------------+
class NeighborCacheTest : public testing::Test {
 protected:
  void SetUp() override {
    now_ = base::TimeTicks::Now();

    router1_.ip_address = kRouterAddress1;
    router1_.ll_address = kRouterMacAddress1;
    router1_.if_name = kUpsteamInterface1;
    router1_.is_router = true;
    router1_.nud_state = kRouterNudState1;

    router2_.ip_address = kRouterAddress2;
    router2_.ll_address = kRouterMacAddress2;
    router2_.if_name = kUpsteamInterface2;
    router2_.is_router = true;
    router2_.nud_state = kRouterNudState2;

    router3_.ip_address = kRouterAddress3;
    router3_.ll_address = kRouterMacAddress3;
    router3_.if_name = kUpsteamInterface2;
    router3_.is_router = true;
    router3_.nud_state = kRouterNudState3;

    node1_.ip_address = kNodeAddress1;
    node1_.ll_address = kNodeMacAddress1;
    node1_.if_name = kDownstreamInterface1;
    node1_.is_router = false;
    node1_.nud_state = kNodeNudState1;

    node2_.ip_address = kNodeAddress2;
    node2_.ll_address = kNodeMacAddress2;
    node2_.if_name = kDownstreamInterface1;
    node2_.is_router = false;
    node2_.nud_state = kNodeNudState2;

    node3_.ip_address = kNodeAddress3;
    node3_.ll_address = kNodeMacAddress3;
    node3_.if_name = kUpsteamInterface1;
    node3_.is_router = false;
    node3_.nud_state = kNodeNudState3;

    node4_.ip_address = kNodeAddress4;
    node4_.ll_address = kNodeMacAddress4;
    node4_.if_name = kDownstreamInterface2;
    node4_.is_router = false;
    node4_.nud_state = kNodeNudState4;

    node5_.ip_address = kNodeAddress5;
    node5_.ll_address = kNodeMacAddress5;
    node5_.if_name = kDownstreamInterface2;
    node5_.is_router = false;
    node5_.nud_state = kNodeNudState5;
  }

  bool InsertAll(NeighborCache* cache) {
    return cache->InsertEntry(kGroupName1, router1_, now_) &&
           cache->InsertEntry(kGroupName2, router2_, now_) &&
           cache->InsertEntry(kGroupName2, router3_, now_) &&
           cache->InsertEntry(kGroupName1, node1_, now_) &&
           cache->InsertEntry(kGroupName1, node2_, now_) &&
           cache->InsertEntry(kGroupName1, node3_, now_) &&
           cache->InsertEntry(kGroupName2, node4_, now_) &&
           cache->InsertEntry(kGroupName2, node5_, now_);
  }

  // A fake now entry.
  TimeTicks now_;

  NeighborCacheEntry router1_;
  NeighborCacheEntry router2_;
  NeighborCacheEntry router3_;

  NeighborCacheEntry node1_;
  NeighborCacheEntry node2_;
  NeighborCacheEntry node3_;
  NeighborCacheEntry node4_;
  NeighborCacheEntry node5_;
};

TEST_F(NeighborCacheTest, Insertion) {
  NeighborCache cache;

  EXPECT_TRUE(cache.InsertEntry(kGroupName1, router1_));
  EXPECT_TRUE(cache.InsertEntry(kGroupName2, router2_));
  EXPECT_TRUE(cache.InsertEntry(kGroupName2, router3_));

  EXPECT_TRUE(cache.InsertEntry(kGroupName1, node1_));
  EXPECT_TRUE(cache.InsertEntry(kGroupName1, node2_));
  EXPECT_TRUE(cache.InsertEntry(kGroupName1, node3_));
  EXPECT_TRUE(cache.InsertEntry(kGroupName2, node4_));
  EXPECT_TRUE(cache.InsertEntry(kGroupName2, node5_));

  EXPECT_TRUE(cache.HasEntry(router1_.ip_address, kGroupName1));
  EXPECT_TRUE(cache.HasEntry(router2_.ip_address, kGroupName2));
  EXPECT_TRUE(cache.HasEntry(router3_.ip_address, kGroupName2));

  EXPECT_TRUE(cache.HasEntry(node1_.ip_address, kGroupName1));
  EXPECT_TRUE(cache.HasEntry(node2_.ip_address, kGroupName1));
  EXPECT_TRUE(cache.HasEntry(node3_.ip_address, kGroupName1));
  EXPECT_TRUE(cache.HasEntry(node4_.ip_address, kGroupName2));
  EXPECT_TRUE(cache.HasEntry(node5_.ip_address, kGroupName2));

  NeighborCacheEntry entry;
  EXPECT_TRUE(cache.GetEntry(router1_.ip_address, kGroupName1, &entry));
  EXPECT_TRUE(CompareEntries(router1_, entry));
  EXPECT_TRUE(cache.GetEntry(router2_.ip_address, kGroupName2, &entry));
  EXPECT_TRUE(CompareEntries(router2_, entry));
  EXPECT_TRUE(cache.GetEntry(router3_.ip_address, kGroupName2, &entry));
  EXPECT_TRUE(CompareEntries(router3_, entry));

  EXPECT_TRUE(cache.GetEntry(node1_.ip_address, kGroupName1, &entry));
  EXPECT_TRUE(CompareEntries(node1_, entry));
  EXPECT_TRUE(cache.GetEntry(node2_.ip_address, kGroupName1, &entry));
  EXPECT_TRUE(CompareEntries(node2_, entry));
  EXPECT_TRUE(cache.GetEntry(node3_.ip_address, kGroupName1, &entry));
  EXPECT_TRUE(CompareEntries(node3_, entry));
  EXPECT_TRUE(cache.GetEntry(node4_.ip_address, kGroupName2, &entry));
  EXPECT_TRUE(CompareEntries(node4_, entry));
  EXPECT_TRUE(cache.GetEntry(node5_.ip_address, kGroupName2, &entry));
  EXPECT_TRUE(CompareEntries(node5_, entry));
}

TEST_F(NeighborCacheTest, RemoveSpecificEntry) {
  NeighborCache cache;
  EXPECT_TRUE(InsertAll(&cache));

  // Remove router 1 and Node 4
  cache.RemoveEntry(router1_.ip_address, kGroupName1);
  cache.RemoveEntry(node4_.ip_address, kGroupName2);

  EXPECT_FALSE(cache.HasEntry(router1_.ip_address, kGroupName1));
  EXPECT_TRUE(cache.HasEntry(router2_.ip_address, kGroupName2));
  EXPECT_TRUE(cache.HasEntry(router3_.ip_address, kGroupName2));

  EXPECT_TRUE(cache.HasEntry(node1_.ip_address, kGroupName1));
  EXPECT_TRUE(cache.HasEntry(node2_.ip_address, kGroupName1));
  EXPECT_TRUE(cache.HasEntry(node3_.ip_address, kGroupName1));
  EXPECT_FALSE(cache.HasEntry(node4_.ip_address, kGroupName2));
  EXPECT_TRUE(cache.HasEntry(node5_.ip_address, kGroupName2));
}

TEST_F(NeighborCacheTest, RemoveByInterface) {
  NeighborCache cache;
  EXPECT_TRUE(InsertAll(&cache));

  // Remove all on downstream 2 (Node 4 and 5)
  cache.ClearForInterface(kDownstreamInterface2);

  EXPECT_TRUE(cache.HasEntry(router1_.ip_address, kGroupName1));
  EXPECT_TRUE(cache.HasEntry(router2_.ip_address, kGroupName2));
  EXPECT_TRUE(cache.HasEntry(router3_.ip_address, kGroupName2));

  EXPECT_TRUE(cache.HasEntry(node1_.ip_address, kGroupName1));
  EXPECT_TRUE(cache.HasEntry(node2_.ip_address, kGroupName1));
  EXPECT_TRUE(cache.HasEntry(node3_.ip_address, kGroupName1));
  EXPECT_FALSE(cache.HasEntry(node4_.ip_address, kGroupName2));
  EXPECT_FALSE(cache.HasEntry(node5_.ip_address, kGroupName2));

  // Remove all on upstream 1 (Router 1 and Nore 3)
  cache.ClearForInterface(kUpsteamInterface1);
  EXPECT_FALSE(cache.HasEntry(router1_.ip_address, kGroupName1));
  EXPECT_TRUE(cache.HasEntry(router2_.ip_address, kGroupName2));
  EXPECT_TRUE(cache.HasEntry(router3_.ip_address, kGroupName2));

  EXPECT_TRUE(cache.HasEntry(node1_.ip_address, kGroupName1));
  EXPECT_TRUE(cache.HasEntry(node2_.ip_address, kGroupName1));
  EXPECT_FALSE(cache.HasEntry(node3_.ip_address, kGroupName1));
}

TEST_F(NeighborCacheTest, RemoveByGroup) {
  NeighborCache cache;
  EXPECT_TRUE(InsertAll(&cache));

  // Remove group 1 (Router 1, Nodes 1-3)
  cache.ClearForGroup(kGroupName1);
  EXPECT_FALSE(cache.HasEntry(router1_.ip_address, kGroupName1));
  EXPECT_TRUE(cache.HasEntry(router2_.ip_address, kGroupName2));
  EXPECT_TRUE(cache.HasEntry(router3_.ip_address, kGroupName2));

  EXPECT_FALSE(cache.HasEntry(node1_.ip_address, kGroupName1));
  EXPECT_FALSE(cache.HasEntry(node2_.ip_address, kGroupName1));
  EXPECT_FALSE(cache.HasEntry(node3_.ip_address, kGroupName1));
  EXPECT_TRUE(cache.HasEntry(node4_.ip_address, kGroupName2));
  EXPECT_TRUE(cache.HasEntry(node5_.ip_address, kGroupName2));
}

TEST_F(NeighborCacheTest, RemoveAll) {
  NeighborCache cache;
  EXPECT_TRUE(InsertAll(&cache));

  cache.Clear();
  EXPECT_FALSE(cache.HasEntry(router1_.ip_address, kGroupName1));
  EXPECT_FALSE(cache.HasEntry(router2_.ip_address, kGroupName2));
  EXPECT_FALSE(cache.HasEntry(router3_.ip_address, kGroupName2));

  EXPECT_FALSE(cache.HasEntry(node1_.ip_address, kGroupName1));
  EXPECT_FALSE(cache.HasEntry(node2_.ip_address, kGroupName1));
  EXPECT_FALSE(cache.HasEntry(node3_.ip_address, kGroupName1));
  EXPECT_FALSE(cache.HasEntry(node4_.ip_address, kGroupName2));
  EXPECT_FALSE(cache.HasEntry(node5_.ip_address, kGroupName2));
}

TEST_F(NeighborCacheTest, GetRouter) {
  NeighborCache cache;
  EXPECT_TRUE(InsertAll(&cache));

  // Router on upstream 1 (Router 1).
  NeighborCacheEntry entry;
  EXPECT_TRUE(
      cache.GetInterfaceRouter(kUpsteamInterface1, kGroupName1, &entry));
  EXPECT_TRUE(CompareEntries(entry, router1_));

  // Router on upstream 2 (Router 2).
  EXPECT_TRUE(
      cache.GetInterfaceRouter(kUpsteamInterface2, kGroupName2, &entry));
  EXPECT_TRUE(CompareEntries(entry, router2_));

  // Router on upstream 1, group 2 (No entry).
  EXPECT_FALSE(
      cache.GetInterfaceRouter(kUpsteamInterface1, kGroupName2, &entry));

  // Router on upstream 2, group 1 (No entry).
  EXPECT_FALSE(
      cache.GetInterfaceRouter(kUpsteamInterface2, kGroupName1, &entry));

  // Router on downstream 1 (No entry).
  EXPECT_FALSE(
      cache.GetInterfaceRouter(kDownstreamInterface1, kGroupName1, &entry));

  // Router on downstream 2 (No entry).
  EXPECT_FALSE(
      cache.GetInterfaceRouter(kDownstreamInterface1, kGroupName1, &entry));

  // Remove Router 2, making Router 3 best option.
  cache.RemoveEntry(router2_.ip_address, kGroupName2);
  EXPECT_TRUE(
      cache.GetInterfaceRouter(kUpsteamInterface2, kGroupName2, &entry));
  EXPECT_TRUE(CompareEntries(entry, router3_));
}

TEST_F(NeighborCacheTest, InvalidInsertion) {
  NeighborCache cache;
  NeighborCacheEntry entry;

  entry.ll_address = kNodeMacAddress1;
  entry.if_name = kDownstreamInterface1;
  entry.nud_state = NUD_REACHABLE;

  // Bad IP Address type.
  entry.ip_address = kIPv4Address;
  EXPECT_FALSE(cache.InsertEntry(kGroupName1, entry));
  entry.ip_address = kNodeAddress1;

  // Bad LL Address.
  entry.ll_address = kBadMacAddress;
  EXPECT_FALSE(cache.InsertEntry(kGroupName1, entry));
  entry.ll_address = kNodeMacAddress1;

  // Bad interface name.
  entry.if_name = kBadInterfaceName;
  EXPECT_FALSE(cache.InsertEntry(kGroupName1, entry));
  entry.if_name = kDownstreamInterface1;

  // Bad NUD state.
  entry.nud_state = kBadNudState;
  EXPECT_FALSE(cache.InsertEntry(kGroupName1, entry));
  entry.nud_state = NUD_REACHABLE;

  // Bad group name.
  EXPECT_FALSE(cache.InsertEntry(kBadGroupName, entry));

  // A good entry.
  EXPECT_TRUE(cache.InsertEntry(kGroupName1, entry));
}

TEST_F(NeighborCacheTest, NoFailedRouter) {
  NeighborCache cache;
  // Change router 1 to failed before inserting.
  router1_.nud_state = NUD_FAILED;
  EXPECT_TRUE(InsertAll(&cache));

  // Check that a failed routers are not returned.
  NeighborCacheEntry entry;
  EXPECT_FALSE(
      cache.GetInterfaceRouter(kUpsteamInterface1, kGroupName1, &entry));
}

TEST_F(NeighborCacheTest, ExpiredRemoval) {
  NeighborCache cache;
  // Change some of the times.
  cache.InsertEntry(kGroupName1, router1_, now_ - kSmallTimeDiff1);
  cache.InsertEntry(kGroupName2, router2_, now_ - kSmallTimeDiff2);
  cache.InsertEntry(kGroupName2, router3_, now_ - kLargeTimeDiff2);
  cache.InsertEntry(kGroupName1, node1_, now_ - kSmallTimeDiff3);
  cache.InsertEntry(kGroupName1, node2_, now_ - kLargeTimeDiff1);
  cache.InsertEntry(kGroupName1, node3_, now_ - kLargeTimeDiff3);
  cache.InsertEntry(kGroupName2, node4_, now_ - kSmallTimeDiff3);
  cache.InsertEntry(kGroupName2, node5_, now_ - kLargeTimeDiff3);

  // Remove all of the currently expired entries.
  // Expected to clear: Nodes 2, 3 and 5 and router 3.
  cache.ClearExpired(now_);

  EXPECT_TRUE(cache.HasEntry(router1_.ip_address, kGroupName1));
  EXPECT_TRUE(cache.HasEntry(router2_.ip_address, kGroupName2));
  EXPECT_FALSE(cache.HasEntry(router3_.ip_address, kGroupName2));

  EXPECT_TRUE(cache.HasEntry(node1_.ip_address, kGroupName1));
  EXPECT_FALSE(cache.HasEntry(node2_.ip_address, kGroupName1));
  EXPECT_FALSE(cache.HasEntry(node3_.ip_address, kGroupName1));
  EXPECT_TRUE(cache.HasEntry(node4_.ip_address, kGroupName2));
  EXPECT_FALSE(cache.HasEntry(node5_.ip_address, kGroupName2));
}

TEST_F(NeighborCacheTest, UpdateExpiry) {
  NeighborCache cache;
  EXPECT_TRUE(InsertAll(&cache));

  NeighborCacheEntry entry;
  EXPECT_TRUE(cache.GetEntry(router1_.ip_address, kGroupName1, &entry));
  const TimeTicks original_expiry = entry.expiry_time;
  EXPECT_EQ(original_expiry, now_ + kEntryExpiryTimeout);

  // Reinsert a node at a different time, should update.
  EXPECT_TRUE(cache.InsertEntry(kGroupName1, router1_, now_ + kLargeTimeDiff2));
  EXPECT_TRUE(cache.GetEntry(router1_.ip_address, kGroupName1, &entry));

  const TimeTicks new_expiry = entry.expiry_time;

  EXPECT_NE(original_expiry, new_expiry);
  EXPECT_EQ(new_expiry - original_expiry, kLargeTimeDiff2);
}

}  // namespace portier
