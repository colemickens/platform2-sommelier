// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "portier/group.h"

#include <memory>
#include <string>
#include <vector>

#include <base/stl_util.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "portier/mock_group.h"

namespace portier {

using std::shared_ptr;
using std::string;
using std::vector;

namespace {

// Test data.

using MockGroup = MockGroupMember::GroupType;
using MockGroupMemberPtr = shared_ptr<MockGroupMember>;

const vector<string> kInvalidGroupNames = {"",
                                           "not one word",
                                           "   leadingspaces",
                                           "trailingspaces   ",
                                           "i||eg@|ch@r$",
                                           "contains^badcharacter",
                                           "\n0\np\ri\n\tabl\e",
                                           "nonasci\xf0\x9f\xa4\xa1"};

const vector<string> kValidGroupNames = {"eth0-group", "test_group", "lanparty",
                                         "net0", "othername"};

constexpr char kGroupName1[] = "group1";
constexpr char kGroupName2[] = "group2";

constexpr size_t kManyInterfaceCount = 30;

}  // namespace

// Parameterized test for invalid group names.
class GroupInvalidNameTest : public testing::TestWithParam<string> {};

TEST_P(GroupInvalidNameTest, CreationFails) {
  const string pg_name = GetParam();
  auto pg_ptr = MockGroup::Create(pg_name);
  EXPECT_FALSE(pg_ptr);
}

INSTANTIATE_TEST_CASE_P(GroupTest,
                        GroupInvalidNameTest,
                        testing::ValuesIn(kInvalidGroupNames));

class GroupValidNameTest : public testing::TestWithParam<string> {};

TEST_P(GroupValidNameTest, CreationSucceeds) {
  const string pg_name = GetParam();
  auto pg_ptr = MockGroup::Create(pg_name);
  EXPECT_TRUE(pg_ptr);
}

INSTANTIATE_TEST_CASE_P(GroupTest,
                        GroupValidNameTest,
                        testing::ValuesIn(kValidGroupNames));

TEST(GroupTest, MemberLess) {
  auto pg_ptr = MockGroup::Create(kGroupName1);
  EXPECT_TRUE(pg_ptr);

  EXPECT_EQ(pg_ptr->name(), kGroupName1);
  EXPECT_EQ(pg_ptr->size(), 0);

  auto members = pg_ptr->GetMembers();
  EXPECT_TRUE(members.empty());
}

TEST(GroupTest, SingleMember) {
  auto pg_ptr = MockGroup::Create(kGroupName1);
  auto mem_ptr = MockGroupMemberPtr(new MockGroupMember());

  // Add member.
  EXPECT_CALL(*mem_ptr, PostJoinGroup()).Times(1);
  EXPECT_CALL(*mem_ptr, PostLeaveGroup()).Times(0);
  EXPECT_TRUE(pg_ptr->AddMember(mem_ptr));

  // Verify membership.
  EXPECT_EQ(pg_ptr->size(), 1);
  EXPECT_TRUE(mem_ptr->HasGroup());

  auto mem_pg_ptr = mem_ptr->GetGroup();
  EXPECT_EQ(pg_ptr.get(), mem_pg_ptr);

  auto members = pg_ptr->GetMembers();
  EXPECT_EQ(members.size(), 1);
  auto list_mem_ptr = members.at(0);
  EXPECT_EQ(list_mem_ptr, mem_ptr);

  // Add a second time.
  EXPECT_TRUE(pg_ptr->AddMember(mem_ptr));
  EXPECT_EQ(pg_ptr->size(), 1);
  EXPECT_TRUE(mem_ptr->HasGroup());

  // Remove
  EXPECT_CALL(*mem_ptr, PostJoinGroup()).Times(0);
  EXPECT_CALL(*mem_ptr, PostLeaveGroup()).Times(1);
  EXPECT_TRUE(pg_ptr->RemoveMember(mem_ptr));

  // Verify removal.
  EXPECT_EQ(pg_ptr->size(), 0);
  EXPECT_FALSE(mem_ptr->HasGroup());

  mem_pg_ptr = mem_ptr->GetGroup();
  EXPECT_FALSE(mem_pg_ptr);

  // Try removing again (should fail).
  EXPECT_FALSE(pg_ptr->RemoveMember(mem_ptr));
}

TEST(GroupTest, OutOfScopeGroup) {
  auto pg_ptr = MockGroup::Create(kGroupName1);
  auto mem_ptr = MockGroupMemberPtr(new MockGroupMember());

  // Add member.
  EXPECT_CALL(*mem_ptr, PostJoinGroup()).Times(1);
  EXPECT_CALL(*mem_ptr, PostLeaveGroup()).Times(0);
  EXPECT_TRUE(pg_ptr->AddMember(mem_ptr));
  EXPECT_TRUE(mem_ptr->HasGroup());

  // Deleting group should automatically remove member.
  EXPECT_CALL(*mem_ptr, PostJoinGroup()).Times(0);
  EXPECT_CALL(*mem_ptr, PostLeaveGroup()).Times(1);
  pg_ptr.reset();
  EXPECT_FALSE(mem_ptr->HasGroup());
}

TEST(GroupTest, MultipleMembers) {
  auto pg_ptr = MockGroup::Create(kGroupName1);
  auto mem1_ptr = MockGroupMemberPtr(new MockGroupMember());
  auto mem2_ptr = MockGroupMemberPtr(new MockGroupMember());

  EXPECT_CALL(*mem1_ptr, PostJoinGroup()).Times(1);
  EXPECT_CALL(*mem1_ptr, PostLeaveGroup()).Times(0);
  EXPECT_CALL(*mem2_ptr, PostJoinGroup()).Times(1);
  EXPECT_CALL(*mem2_ptr, PostLeaveGroup()).Times(0);
  EXPECT_TRUE(pg_ptr->AddMember(mem1_ptr));
  EXPECT_TRUE(pg_ptr->AddMember(mem2_ptr));

  // Verify members.
  EXPECT_EQ(pg_ptr->size(), 2);
  EXPECT_TRUE(mem1_ptr->HasGroup());
  EXPECT_TRUE(mem2_ptr->HasGroup());

  // Remove first.
  EXPECT_CALL(*mem1_ptr, PostJoinGroup()).Times(0);
  EXPECT_CALL(*mem1_ptr, PostLeaveGroup()).Times(1);
  EXPECT_TRUE(pg_ptr->RemoveMember(mem1_ptr));

  // Verify.
  EXPECT_EQ(pg_ptr->size(), 1);
  EXPECT_FALSE(mem1_ptr->HasGroup());
  EXPECT_TRUE(mem2_ptr->HasGroup());

  // Add it back.
  EXPECT_CALL(*mem1_ptr, PostJoinGroup()).Times(1);
  EXPECT_CALL(*mem1_ptr, PostLeaveGroup()).Times(0);
  EXPECT_TRUE(pg_ptr->AddMember(mem1_ptr));

  // Verify members.
  EXPECT_EQ(pg_ptr->size(), 2);
  EXPECT_TRUE(mem1_ptr->HasGroup());
  EXPECT_TRUE(mem2_ptr->HasGroup());

  // Remove all.
  EXPECT_CALL(*mem1_ptr, PostJoinGroup()).Times(0);
  EXPECT_CALL(*mem1_ptr, PostLeaveGroup()).Times(1);
  EXPECT_CALL(*mem2_ptr, PostJoinGroup()).Times(0);
  EXPECT_CALL(*mem2_ptr, PostLeaveGroup()).Times(1);
  pg_ptr->RemoveAllMembers();

  // Verify
  EXPECT_EQ(pg_ptr->size(), 0);
  EXPECT_FALSE(mem1_ptr->HasGroup());
  EXPECT_FALSE(mem2_ptr->HasGroup());
}

TEST(GroupTest, ManyMembers) {
  auto pg_ptr = MockGroup::Create(kGroupName1);

  for (size_t i = 0; i < kManyInterfaceCount; i++) {
    auto mem_ptr = MockGroupMemberPtr(new MockGroupMember());
    EXPECT_CALL(*mem_ptr, PostJoinGroup()).Times(1);
    EXPECT_CALL(*mem_ptr, PostLeaveGroup()).Times(1);
    EXPECT_TRUE(pg_ptr->AddMember(mem_ptr));
  }

  EXPECT_EQ(pg_ptr->size(), kManyInterfaceCount);
  pg_ptr->RemoveAllMembers();
  EXPECT_EQ(pg_ptr->size(), 0);
}

TEST(GroupTest, Upstream) {
  auto pg_ptr = MockGroup::Create(kGroupName1);
  auto mem1_ptr = MockGroupMemberPtr(new MockGroupMember());
  auto mem2_ptr = MockGroupMemberPtr(new MockGroupMember());

  // For this test, we dont really care about the calls.
  EXPECT_CALL(*mem1_ptr, PostJoinGroup()).Times(1);
  EXPECT_CALL(*mem1_ptr, PostLeaveGroup()).Times(1);
  EXPECT_CALL(*mem2_ptr, PostJoinGroup()).Times(1);
  EXPECT_CALL(*mem2_ptr, PostLeaveGroup()).Times(1);

  EXPECT_FALSE(pg_ptr->GetUpstream());

  // Add member.
  EXPECT_TRUE(pg_ptr->AddMember(mem1_ptr));

  // Verify nothing funky is going on.
  EXPECT_FALSE(mem1_ptr->IsUpstream());
  EXPECT_FALSE(pg_ptr->GetUpstream());

  // Try and fail at setting non-member as upstream.
  EXPECT_FALSE(pg_ptr->SetUpstream(mem2_ptr));
  EXPECT_FALSE(pg_ptr->GetUpstream());
  EXPECT_FALSE(mem2_ptr->HasGroup());

  // Set the current member as upstream.
  EXPECT_TRUE(pg_ptr->SetUpstream(mem1_ptr));
  EXPECT_EQ(pg_ptr->GetUpstream(), mem1_ptr);
  EXPECT_TRUE(mem1_ptr->IsUpstream());

  // Add another interface.
  EXPECT_TRUE(pg_ptr->AddMember(mem2_ptr));

  // Nothing else should change.
  EXPECT_EQ(pg_ptr->GetUpstream(), mem1_ptr);
  EXPECT_TRUE(mem1_ptr->IsUpstream());
  EXPECT_FALSE(mem2_ptr->IsUpstream());

  // Remove upstream member normally.
  pg_ptr->UnsetUpstream();

  // Should still be member, but not upsteam.
  EXPECT_FALSE(mem1_ptr->IsUpstream());
  EXPECT_TRUE(mem1_ptr->HasGroup());
  EXPECT_EQ(pg_ptr->size(), 2);

  // Set second member as upstream.
  EXPECT_TRUE(pg_ptr->SetUpstream(mem2_ptr));
  EXPECT_EQ(pg_ptr->GetUpstream(), mem2_ptr);
  EXPECT_TRUE(mem2_ptr->IsUpstream());

  // Remove second member from group.
  EXPECT_TRUE(pg_ptr->RemoveMember(mem2_ptr));

  // Verify that is no longer upstream.
  EXPECT_FALSE(pg_ptr->GetUpstream());
  EXPECT_FALSE(mem2_ptr->HasGroup());
  EXPECT_FALSE(mem2_ptr->IsUpstream());
  EXPECT_EQ(pg_ptr->size(), 1);
}

TEST(GroupTest, MultipleGroups) {
  auto pg1_ptr = MockGroup::Create(kGroupName1);
  auto pg2_ptr = MockGroup::Create(kGroupName2);
  auto mem1_ptr = MockGroupMemberPtr(new MockGroupMember());
  auto mem2_ptr = MockGroupMemberPtr(new MockGroupMember());

  EXPECT_CALL(*mem1_ptr, PostJoinGroup()).Times(1);
  EXPECT_CALL(*mem1_ptr, PostLeaveGroup()).Times(0);
  EXPECT_CALL(*mem2_ptr, PostJoinGroup()).Times(1);
  EXPECT_CALL(*mem2_ptr, PostLeaveGroup()).Times(0);
  EXPECT_TRUE(pg1_ptr->AddMember(mem1_ptr));
  EXPECT_TRUE(pg2_ptr->AddMember(mem2_ptr));

  // Verify membership.
  EXPECT_TRUE(mem1_ptr->HasGroup());
  EXPECT_TRUE(mem2_ptr->HasGroup());

  EXPECT_EQ(pg1_ptr->size(), 1);
  EXPECT_EQ(pg2_ptr->size(), 1);

  // Should not be able members to other groups.
  EXPECT_FALSE(pg1_ptr->AddMember(mem2_ptr));
  EXPECT_FALSE(pg2_ptr->AddMember(mem1_ptr));

  // Should not be able to remove members from other groups
  EXPECT_FALSE(pg1_ptr->RemoveMember(mem2_ptr));
  EXPECT_FALSE(pg2_ptr->RemoveMember(mem1_ptr));

  // Remove member 2 from group 2.
  EXPECT_CALL(*mem2_ptr, PostJoinGroup()).Times(0);
  EXPECT_CALL(*mem2_ptr, PostLeaveGroup()).Times(1);
  EXPECT_TRUE(pg2_ptr->RemoveMember(mem2_ptr));

  // Add member 2 to group 1.
  EXPECT_CALL(*mem2_ptr, PostJoinGroup()).Times(1);
  EXPECT_CALL(*mem2_ptr, PostLeaveGroup()).Times(0);
  EXPECT_TRUE(pg1_ptr->AddMember(mem2_ptr));

  EXPECT_EQ(pg1_ptr->size(), 2);
  EXPECT_EQ(pg2_ptr->size(), 0);

  EXPECT_CALL(*mem1_ptr, PostLeaveGroup()).Times(1);
  EXPECT_CALL(*mem2_ptr, PostLeaveGroup()).Times(1);
}

}  // namespace portier
