// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PORTIER_MOCK_GROUP_H_
#define PORTIER_MOCK_GROUP_H_

#include <gmock/gmock.h>

#include "portier/group.h"

namespace portier {

// A mock implementation of a Group Member.  Used for testing.
class MockGroupMember : public GroupMemberInterface<MockGroupMember> {
 public:
  MockGroupMember() {}

  MOCK_METHOD(void, PostJoinGroup, (), (override));
  MOCK_METHOD(void, PostLeaveGroup, (), (override));
};

using MockGroup = MockGroupMember::GroupType;

}  // namespace portier

#endif  // PORTIER_MOCK_GROUP_H_
