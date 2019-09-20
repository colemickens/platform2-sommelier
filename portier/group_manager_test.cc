// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "portier/group_manager.h"

#include <memory>

#include <base/stl_util.h>
#include <gtest/gtest.h>
#include <shill/net/ip_address.h>

#include "portier/mock_group.h"

namespace portier {

using std::shared_ptr;

using Code = Status::Code;

using MockGroupManager = GroupManager<MockGroupMember>;

using MockGroupMemberPtr = shared_ptr<MockGroupMember>;

// Testing data.
namespace {

constexpr char kGroupAName[] = "ethernet";
constexpr char kGroupBName[] = "wifi";
constexpr char kGroupCName[] = "lte";

}  // namespace

TEST(GroupManagerTest, GrouplessManager) {
  MockGroupManager manager;

  auto group_list = manager.GetGroupNames();
  EXPECT_EQ(group_list.size(), 0);

  EXPECT_FALSE(manager.HasGroup(kGroupAName));
  EXPECT_FALSE(manager.HasGroup(kGroupBName));
  EXPECT_FALSE(manager.HasGroup(kGroupCName));

  EXPECT_EQ(manager.ReleaseGroup(kGroupAName).code(), Code::DOES_NOT_EXIST);
  EXPECT_EQ(manager.ReleaseGroup(kGroupBName).code(), Code::DOES_NOT_EXIST);
  EXPECT_EQ(manager.ReleaseGroup(kGroupCName).code(), Code::DOES_NOT_EXIST);
}

TEST(GroupManagerTest, SingleGroupInsertion) {
  MockGroupManager manager;

  // Create group and add some interfaces with an upstream.
  EXPECT_TRUE(manager.CreateGroup(kGroupAName));

  // Verify groups existence.
  EXPECT_TRUE(manager.HasGroup(kGroupAName));
  auto group_a = manager.GetGroup(kGroupAName);
  EXPECT_TRUE(group_a);

  const auto group_list = manager.GetGroups();
  EXPECT_EQ(group_list.size(), 1);
  EXPECT_EQ(group_list.at(0), group_a);
  const auto group_name_list = manager.GetGroupNames();
  EXPECT_EQ(group_name_list.size(), 1);
  EXPECT_EQ(group_name_list.at(0), kGroupAName);
}

TEST(GroupManagerTest, SingleGroupDestroy) {
  MockGroupManager manager;

  // Create group and add some interfaces with an upstream.
  EXPECT_TRUE(manager.CreateGroup(kGroupAName));

  auto group_ptr = manager.GetGroup(kGroupAName);

  auto mem_ptr = MockGroupMemberPtr(new MockGroupMember());

  EXPECT_CALL(*mem_ptr, PostJoinGroup()).Times(1);
  EXPECT_CALL(*mem_ptr, PostLeaveGroup()).Times(0);
  EXPECT_TRUE(group_ptr->AddMember(mem_ptr));

  EXPECT_TRUE(mem_ptr->HasGroup());

  // Remove group.
  EXPECT_CALL(*mem_ptr, PostLeaveGroup()).Times(1);
  EXPECT_TRUE(manager.ReleaseGroup(kGroupAName));

  // Verify removal.
  EXPECT_FALSE(manager.HasGroup(kGroupAName));
  EXPECT_FALSE(mem_ptr->HasGroup());
}

TEST(GroupManagerTest, MultiGroup) {
  MockGroupManager manager;

  // Create groups.
  EXPECT_TRUE(manager.CreateGroup(kGroupAName));
  EXPECT_TRUE(manager.CreateGroup(kGroupBName));

  // Verify groups.
  auto group_a_ptr = manager.GetGroup(kGroupAName);
  EXPECT_TRUE(group_a_ptr);
  auto group_b_ptr = manager.GetGroup(kGroupBName);
  EXPECT_TRUE(group_b_ptr);

  const auto group_list = manager.GetGroupNames();
  EXPECT_EQ(group_list.size(), 2);
  EXPECT_TRUE(base::ContainsValue(group_list, kGroupAName));
  EXPECT_TRUE(base::ContainsValue(group_list, kGroupBName));
  EXPECT_TRUE(manager.HasGroup(kGroupAName));
  EXPECT_TRUE(manager.HasGroup(kGroupBName));
  EXPECT_FALSE(manager.HasGroup(kGroupCName));

  // Remove group A and verify removal of members.
  EXPECT_TRUE(manager.ReleaseGroup(kGroupAName));
  EXPECT_FALSE(manager.HasGroup(kGroupAName));

  // Verify that the other groups are still around.
  EXPECT_TRUE(manager.HasGroup(kGroupBName));
}

TEST(GroupManagerTest, BadGroupManipulation) {
  MockGroupManager manager;

  // Create group twice.
  EXPECT_TRUE(manager.CreateGroup(kGroupAName));
  EXPECT_EQ(manager.CreateGroup(kGroupAName).code(), Code::ALREADY_EXISTS);

  EXPECT_TRUE(manager.CreateGroup(kGroupBName));
  EXPECT_EQ(manager.CreateGroup(kGroupBName).code(), Code::ALREADY_EXISTS);

  // Destroy group twice.
  EXPECT_TRUE(manager.ReleaseGroup(kGroupAName));
  EXPECT_EQ(manager.ReleaseGroup(kGroupAName).code(), Code::DOES_NOT_EXIST);

  EXPECT_TRUE(manager.ReleaseGroup(kGroupBName));
  EXPECT_EQ(manager.ReleaseGroup(kGroupBName).code(), Code::DOES_NOT_EXIST);

  // Destroy unknown.
  EXPECT_EQ(manager.ReleaseGroup(kGroupCName).code(), Code::DOES_NOT_EXIST);
}

}  // namespace portier
