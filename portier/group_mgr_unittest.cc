// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "portier/group_mgr.h"

#include <memory>
#include <string>
#include <vector>

#include <base/stl_util.h>
#include <gtest/gtest.h>
#include <shill/net/ip_address.h>

namespace portier {

using std::string;
using std::vector;

using Code = Status::Code;

// Testing data.
namespace {

constexpr char kInterfaceAName[] = "eth0";
constexpr char kInterfaceBName[] = "eth1";
constexpr char kInterfaceCName[] = "vmtap0";
constexpr char kInterfaceDName[] = "vmtap1";
constexpr char kInterfaceEName[] = "vmtap2";

constexpr char kGroupAName[] = "ethernet";
constexpr char kGroupBName[] = "wifi";
constexpr char kGroupCName[] = "lte";

}  // namespace

TEST(GroupManagerTest, GrouplessManager) {
  GroupManager manager;

  auto group_list = manager.GetGroupNames();
  EXPECT_EQ(group_list.size(), 0);

  EXPECT_FALSE(manager.HasProxyGroup(kGroupAName));
  EXPECT_FALSE(manager.HasProxyGroup(kGroupBName));
  EXPECT_FALSE(manager.HasProxyGroup(kGroupCName));

  EXPECT_FALSE(manager.IsInterfaceMember(kInterfaceAName));
  EXPECT_FALSE(manager.IsInterfaceMember(kInterfaceBName));
  EXPECT_FALSE(manager.IsInterfaceMember(kInterfaceCName));
  EXPECT_FALSE(manager.IsInterfaceMember(kInterfaceDName));
  EXPECT_FALSE(manager.IsInterfaceMember(kInterfaceEName));

  EXPECT_EQ(manager.DestroyProxyGroup(kGroupAName).code(),
            Code::DOES_NOT_EXIST);
  EXPECT_EQ(manager.DestroyProxyGroup(kGroupBName).code(),
            Code::DOES_NOT_EXIST);
  EXPECT_EQ(manager.DestroyProxyGroup(kGroupCName).code(),
            Code::DOES_NOT_EXIST);
}

TEST(GroupManagerTest, SingleGroupInsertion) {
  GroupManager manager;

  // Create group and add some interfaces with an upstream.
  EXPECT_TRUE(manager.CreateProxyGroup(kGroupAName));

  EXPECT_TRUE(manager.AddInterfaceToProxyGroup(kInterfaceAName, kGroupAName));
  EXPECT_TRUE(manager.AddInterfaceToProxyGroup(kInterfaceBName, kGroupAName));
  EXPECT_TRUE(manager.AddInterfaceToProxyGroup(kInterfaceCName, kGroupAName));

  EXPECT_TRUE(manager.SetProxyGroupUpstream(kInterfaceAName, kGroupAName));

  // Verify groups existence.
  EXPECT_TRUE(manager.HasProxyGroup(kGroupAName));
  const auto group_list = manager.GetGroupNames();
  EXPECT_EQ(group_list.size(), 1);
  EXPECT_EQ(group_list.at(0), kGroupAName);

  // Verify memberships.
  vector<string> members;
  EXPECT_TRUE(manager.GetGroupMembers(kGroupAName, &members));

  EXPECT_EQ(members.size(), 3);
  EXPECT_TRUE(base::ContainsValue(members, kInterfaceAName));
  EXPECT_TRUE(base::ContainsValue(members, kInterfaceBName));
  EXPECT_TRUE(base::ContainsValue(members, kInterfaceCName));
  EXPECT_TRUE(manager.IsInterfaceMember(kInterfaceAName));
  EXPECT_TRUE(manager.IsInterfaceMember(kInterfaceBName));
  EXPECT_TRUE(manager.IsInterfaceMember(kInterfaceCName));
  EXPECT_TRUE(
      manager.IsInterfaceMemberOfProxyGroup(kInterfaceAName, kGroupAName));
  EXPECT_TRUE(
      manager.IsInterfaceMemberOfProxyGroup(kInterfaceBName, kGroupAName));
  EXPECT_TRUE(
      manager.IsInterfaceMemberOfProxyGroup(kInterfaceCName, kGroupAName));

  string pg_name;
  EXPECT_TRUE(manager.GetProxyGroupOfInterface(kInterfaceAName, &pg_name));
  EXPECT_EQ(pg_name, kGroupAName);
  EXPECT_TRUE(manager.GetProxyGroupOfInterface(kInterfaceBName, &pg_name));
  EXPECT_EQ(pg_name, kGroupAName);
  EXPECT_TRUE(manager.GetProxyGroupOfInterface(kInterfaceCName, &pg_name));
  EXPECT_EQ(pg_name, kGroupAName);

  // Verify upstream.
  EXPECT_TRUE(manager.IsInterfaceUpstream(kInterfaceAName, kGroupAName));
  EXPECT_FALSE(manager.IsInterfaceUpstream(kInterfaceBName, kGroupAName));
  EXPECT_FALSE(manager.IsInterfaceUpstream(kInterfaceCName, kGroupAName));
  string upstream_name;
  EXPECT_TRUE(manager.GetProxyGroupUpstream(kGroupAName, &upstream_name));
  EXPECT_EQ(upstream_name, kInterfaceAName);
}

TEST(GroupManagerTest, SingleGroupChangeUpsream) {
  GroupManager manager;

  // Create group and add some interfaces with an upstream.
  EXPECT_TRUE(manager.CreateProxyGroup(kGroupAName));
  EXPECT_TRUE(manager.AddInterfaceToProxyGroup(kInterfaceAName, kGroupAName));
  EXPECT_TRUE(manager.AddInterfaceToProxyGroup(kInterfaceBName, kGroupAName));
  EXPECT_TRUE(manager.AddInterfaceToProxyGroup(kInterfaceCName, kGroupAName));
  EXPECT_TRUE(manager.SetProxyGroupUpstream(kInterfaceAName, kGroupAName));

  // Change upstream.
  // Should fail.
  EXPECT_EQ(manager.SetProxyGroupUpstream(kInterfaceBName, kGroupAName).code(),
            Code::ALREADY_EXISTS);
  EXPECT_TRUE(manager.RemoveProxyGroupUpstream(kGroupAName));
  EXPECT_TRUE(manager.SetProxyGroupUpstream(kInterfaceBName, kGroupAName));
  // Verify change.
  EXPECT_FALSE(manager.IsInterfaceUpstream(kInterfaceAName, kGroupAName));
  EXPECT_TRUE(manager.IsInterfaceUpstream(kInterfaceBName, kGroupAName));
  string upstream_name;
  EXPECT_TRUE(manager.GetProxyGroupUpstream(kGroupAName, &upstream_name));
  EXPECT_EQ(upstream_name, kInterfaceBName);
}

TEST(GroupManagerTest, SingleGroupInterfaceRemoval) {
  GroupManager manager;

  // Create group and add some interfaces with an upstream.
  EXPECT_TRUE(manager.CreateProxyGroup(kGroupAName));
  EXPECT_TRUE(manager.AddInterfaceToProxyGroup(kInterfaceAName, kGroupAName));
  EXPECT_TRUE(manager.AddInterfaceToProxyGroup(kInterfaceBName, kGroupAName));
  EXPECT_TRUE(manager.AddInterfaceToProxyGroup(kInterfaceCName, kGroupAName));
  EXPECT_TRUE(manager.SetProxyGroupUpstream(kInterfaceBName, kGroupAName));

  // Remove interface and verify its removal.
  EXPECT_TRUE(
      manager.RemoveInterfaceFromProxyGroup(kInterfaceCName, kGroupAName));
  EXPECT_FALSE(manager.IsInterfaceMember(kInterfaceCName));
  EXPECT_FALSE(
      manager.IsInterfaceMemberOfProxyGroup(kInterfaceCName, kGroupAName));

  // Remove upstream interface and verify.
  EXPECT_TRUE(
      manager.RemoveInterfaceFromProxyGroup(kInterfaceBName, kGroupAName));
  EXPECT_FALSE(manager.IsInterfaceMember(kInterfaceBName));
  EXPECT_FALSE(
      manager.IsInterfaceMemberOfProxyGroup(kInterfaceBName, kGroupAName));
  EXPECT_FALSE(manager.IsInterfaceUpstream(kInterfaceBName, kGroupAName));
  string upstream_name;
  EXPECT_FALSE(manager.GetProxyGroupUpstream(kGroupAName, &upstream_name));
}

TEST(GroupManagerTest, SingleGroupDestroy) {
  GroupManager manager;

  // Create group and add some interfaces with an upstream.
  EXPECT_TRUE(manager.CreateProxyGroup(kGroupAName));
  EXPECT_TRUE(manager.AddInterfaceToProxyGroup(kInterfaceAName, kGroupAName));
  EXPECT_TRUE(manager.AddInterfaceToProxyGroup(kInterfaceBName, kGroupAName));
  EXPECT_TRUE(manager.AddInterfaceToProxyGroup(kInterfaceCName, kGroupAName));
  EXPECT_TRUE(manager.SetProxyGroupUpstream(kInterfaceAName, kGroupAName));

  // Remove group.
  EXPECT_TRUE(manager.DestroyProxyGroup(kGroupAName));
  EXPECT_FALSE(manager.HasProxyGroup(kGroupAName));
  EXPECT_FALSE(
      manager.IsInterfaceMemberOfProxyGroup(kInterfaceAName, kGroupAName));
  EXPECT_FALSE(
      manager.IsInterfaceMemberOfProxyGroup(kInterfaceBName, kGroupAName));
  EXPECT_FALSE(
      manager.IsInterfaceMemberOfProxyGroup(kInterfaceCName, kGroupAName));
  EXPECT_FALSE(manager.IsInterfaceMember(kInterfaceAName));
  EXPECT_FALSE(manager.IsInterfaceMember(kInterfaceBName));
  EXPECT_FALSE(manager.IsInterfaceMember(kInterfaceCName));
  EXPECT_FALSE(manager.IsInterfaceUpstream(kInterfaceAName, kGroupAName));
  string pg_name;
  EXPECT_FALSE(manager.GetProxyGroupOfInterface(kInterfaceAName, &pg_name));
  EXPECT_FALSE(manager.GetProxyGroupOfInterface(kInterfaceBName, &pg_name));
  EXPECT_FALSE(manager.GetProxyGroupOfInterface(kInterfaceCName, &pg_name));
  EXPECT_EQ(pg_name, "");
}

TEST(GroupManagerTest, MultiGroup) {
  GroupManager manager;

  // Create groups.
  EXPECT_TRUE(manager.CreateProxyGroup(kGroupAName));
  EXPECT_TRUE(manager.CreateProxyGroup(kGroupBName));

  // Add members.
  //  Group A = {If A, If B}
  //  Group B = {If C, If D}
  EXPECT_TRUE(manager.AddInterfaceToProxyGroup(kInterfaceAName, kGroupAName));
  EXPECT_TRUE(manager.AddInterfaceToProxyGroup(kInterfaceBName, kGroupAName));
  EXPECT_TRUE(manager.AddInterfaceToProxyGroup(kInterfaceCName, kGroupBName));
  EXPECT_TRUE(manager.AddInterfaceToProxyGroup(kInterfaceDName, kGroupBName));

  // Verify groups.
  const auto group_list = manager.GetGroupNames();
  EXPECT_EQ(group_list.size(), 2);
  EXPECT_TRUE(base::ContainsValue(group_list, kGroupAName));
  EXPECT_TRUE(base::ContainsValue(group_list, kGroupBName));
  EXPECT_TRUE(manager.HasProxyGroup(kGroupAName));
  EXPECT_TRUE(manager.HasProxyGroup(kGroupBName));
  EXPECT_FALSE(manager.HasProxyGroup(kGroupCName));

  // Verify memberships.
  vector<string> members;
  // Group A.
  EXPECT_TRUE(manager.GetGroupMembers(kGroupAName, &members));
  EXPECT_EQ(members.size(), 2);
  EXPECT_TRUE(base::ContainsValue(members, kInterfaceAName));
  EXPECT_TRUE(base::ContainsValue(members, kInterfaceBName));
  EXPECT_TRUE(manager.IsInterfaceMember(kInterfaceAName));
  EXPECT_TRUE(manager.IsInterfaceMember(kInterfaceBName));
  EXPECT_TRUE(
      manager.IsInterfaceMemberOfProxyGroup(kInterfaceAName, kGroupAName));
  EXPECT_TRUE(
      manager.IsInterfaceMemberOfProxyGroup(kInterfaceBName, kGroupAName));
  string pg_name;
  EXPECT_TRUE(manager.GetProxyGroupOfInterface(kInterfaceAName, &pg_name));
  EXPECT_EQ(pg_name, kGroupAName);
  EXPECT_TRUE(manager.GetProxyGroupOfInterface(kInterfaceBName, &pg_name));
  EXPECT_EQ(pg_name, kGroupAName);
  // Group B.
  EXPECT_TRUE(manager.GetGroupMembers(kGroupBName, &members));
  EXPECT_EQ(members.size(), 2);
  EXPECT_TRUE(base::ContainsValue(members, kInterfaceCName));
  EXPECT_TRUE(base::ContainsValue(members, kInterfaceDName));
  EXPECT_TRUE(manager.IsInterfaceMember(kInterfaceCName));
  EXPECT_TRUE(manager.IsInterfaceMember(kInterfaceDName));
  EXPECT_TRUE(
      manager.IsInterfaceMemberOfProxyGroup(kInterfaceCName, kGroupBName));
  EXPECT_TRUE(
      manager.IsInterfaceMemberOfProxyGroup(kInterfaceDName, kGroupBName));
  EXPECT_TRUE(manager.GetProxyGroupOfInterface(kInterfaceCName, &pg_name));
  EXPECT_EQ(pg_name, kGroupBName);
  EXPECT_TRUE(manager.GetProxyGroupOfInterface(kInterfaceDName, &pg_name));
  EXPECT_EQ(pg_name, kGroupBName);

  // Verify no cross-memberships.
  EXPECT_FALSE(
      manager.IsInterfaceMemberOfProxyGroup(kInterfaceAName, kGroupBName));
  EXPECT_FALSE(
      manager.IsInterfaceMemberOfProxyGroup(kInterfaceBName, kGroupBName));
  EXPECT_FALSE(
      manager.IsInterfaceMemberOfProxyGroup(kInterfaceCName, kGroupAName));
  EXPECT_FALSE(
      manager.IsInterfaceMemberOfProxyGroup(kInterfaceDName, kGroupAName));

  // Remove group A and verify removal of members.
  EXPECT_TRUE(manager.DestroyProxyGroup(kGroupAName));
  EXPECT_FALSE(manager.HasProxyGroup(kGroupAName));
  EXPECT_FALSE(manager.IsInterfaceMember(kInterfaceAName));
  EXPECT_FALSE(manager.IsInterfaceMember(kInterfaceBName));
  EXPECT_FALSE(
      manager.IsInterfaceMemberOfProxyGroup(kInterfaceAName, kGroupAName));
  EXPECT_FALSE(
      manager.IsInterfaceMemberOfProxyGroup(kInterfaceBName, kGroupAName));

  // Verify that the other groups are still around.
  EXPECT_TRUE(manager.IsInterfaceMember(kInterfaceCName));
  EXPECT_TRUE(manager.IsInterfaceMember(kInterfaceDName));
  EXPECT_TRUE(
      manager.IsInterfaceMemberOfProxyGroup(kInterfaceCName, kGroupBName));
  EXPECT_TRUE(
      manager.IsInterfaceMemberOfProxyGroup(kInterfaceDName, kGroupBName));

  // Remove all interfaces from B.
  EXPECT_TRUE(
      manager.RemoveInterfaceFromProxyGroup(kInterfaceCName, kGroupBName));
  EXPECT_TRUE(
      manager.RemoveInterfaceFromProxyGroup(kInterfaceDName, kGroupBName));
  EXPECT_FALSE(manager.IsInterfaceMember(kInterfaceCName));
  EXPECT_FALSE(manager.IsInterfaceMember(kInterfaceDName));
  EXPECT_FALSE(
      manager.IsInterfaceMemberOfProxyGroup(kInterfaceCName, kGroupBName));
  EXPECT_FALSE(
      manager.IsInterfaceMemberOfProxyGroup(kInterfaceDName, kGroupBName));

  // Ensure B is still there.
  EXPECT_TRUE(manager.HasProxyGroup(kGroupBName));
}

TEST(GroupManagerTest, BadGroupManipulation) {
  GroupManager manager;

  // Create group twice.
  EXPECT_TRUE(manager.CreateProxyGroup(kGroupAName));
  EXPECT_FALSE(manager.CreateProxyGroup(kGroupAName));

  EXPECT_TRUE(manager.CreateProxyGroup(kGroupBName));
  EXPECT_FALSE(manager.CreateProxyGroup(kGroupBName));

  // Destroy group twice.
  EXPECT_TRUE(manager.DestroyProxyGroup(kGroupAName));
  EXPECT_FALSE(manager.DestroyProxyGroup(kGroupAName));

  EXPECT_TRUE(manager.DestroyProxyGroup(kGroupBName));
  EXPECT_FALSE(manager.DestroyProxyGroup(kGroupBName));

  // Destroy unknown.
  EXPECT_FALSE(manager.DestroyProxyGroup(kGroupCName));
}

TEST(GroupManagerTest, BadMembershipManipulation) {
  GroupManager manager;

  // Create group.
  EXPECT_TRUE(manager.CreateProxyGroup(kGroupAName));
  EXPECT_TRUE(manager.CreateProxyGroup(kGroupBName));

  // Add some interfaces.
  EXPECT_TRUE(manager.AddInterfaceToProxyGroup(kInterfaceAName, kGroupAName));
  EXPECT_TRUE(manager.AddInterfaceToProxyGroup(kInterfaceBName, kGroupBName));

  // Add to non-existing group.
  EXPECT_FALSE(manager.AddInterfaceToProxyGroup(kInterfaceCName, kGroupCName));

  // Double add.
  EXPECT_FALSE(manager.AddInterfaceToProxyGroup(kInterfaceAName, kGroupAName));
  EXPECT_FALSE(manager.AddInterfaceToProxyGroup(kInterfaceBName, kGroupBName));

  // Try to reuse interface in different group.
  EXPECT_FALSE(manager.AddInterfaceToProxyGroup(kInterfaceAName, kGroupBName));
  EXPECT_FALSE(manager.AddInterfaceToProxyGroup(kInterfaceBName, kGroupAName));

  // Set different upstreams.
  EXPECT_FALSE(manager.SetProxyGroupUpstream(kInterfaceBName, kGroupAName));
  EXPECT_FALSE(manager.SetProxyGroupUpstream(kInterfaceAName, kGroupBName));

  // Set upstream to non-existing interface.
  EXPECT_FALSE(manager.SetProxyGroupUpstream(kInterfaceCName, kGroupAName));

  // Set upstream  of non-existing group.
  EXPECT_FALSE(manager.SetProxyGroupUpstream(kInterfaceAName, kGroupCName));

  // Get group name of non-existing interface.
  string pg_name;
  EXPECT_FALSE(manager.GetProxyGroupOfInterface(kInterfaceCName, &pg_name));

  // Remove interface from wrong group.
  EXPECT_FALSE(
      manager.RemoveInterfaceFromProxyGroup(kInterfaceAName, kGroupBName));
  EXPECT_FALSE(
      manager.RemoveInterfaceFromProxyGroup(kInterfaceBName, kGroupAName));

  // Verify they still exists.
  EXPECT_TRUE(
      manager.IsInterfaceMemberOfProxyGroup(kInterfaceAName, kGroupAName));
  EXPECT_TRUE(
      manager.IsInterfaceMemberOfProxyGroup(kInterfaceBName, kGroupBName));

  // Remove from non-existing group.
  EXPECT_FALSE(
      manager.RemoveInterfaceFromProxyGroup(kInterfaceAName, kGroupCName));
  EXPECT_FALSE(
      manager.RemoveInterfaceFromProxyGroup(kInterfaceBName, kGroupCName));

  // Remove non-existing interface.
  EXPECT_FALSE(
      manager.RemoveInterfaceFromProxyGroup(kInterfaceCName, kGroupAName));
  EXPECT_FALSE(
      manager.RemoveInterfaceFromProxyGroup(kInterfaceCName, kGroupBName));

  // Remove interfaces.
  EXPECT_TRUE(
      manager.RemoveInterfaceFromProxyGroup(kInterfaceAName, kGroupAName));
  EXPECT_TRUE(
      manager.RemoveInterfaceFromProxyGroup(kInterfaceBName, kGroupBName));

  // Double remove interfaces.
  EXPECT_FALSE(
      manager.RemoveInterfaceFromProxyGroup(kInterfaceAName, kGroupAName));
  EXPECT_FALSE(
      manager.RemoveInterfaceFromProxyGroup(kInterfaceBName, kGroupBName));
}

}  // namespace portier
