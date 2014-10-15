// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include "permission_broker/allow_group_tty_device_rule.h"
#include "permission_broker/deny_group_tty_device_rule.h"

namespace permission_broker {

class GroupTtyDeviceRuleTest : public testing::Test {
 public:
  GroupTtyDeviceRuleTest()
      : allow_rule_("nonexistent"), deny_rule_("nonexistent") {}
  ~GroupTtyDeviceRuleTest() override = default;

 protected:
  AllowGroupTtyDeviceRule allow_rule_;
  DenyGroupTtyDeviceRule deny_rule_;

 private:
  DISALLOW_COPY_AND_ASSIGN(GroupTtyDeviceRuleTest);
};

TEST_F(GroupTtyDeviceRuleTest, AllowRuleIgnoreNonMatchingGroup) {
  ASSERT_EQ(Rule::IGNORE, allow_rule_.Process("/dev/tty", Rule::ANY_INTERFACE));
}

TEST_F(GroupTtyDeviceRuleTest, DenyRuleIgnoreNonMatchingGroup) {
  ASSERT_EQ(Rule::IGNORE, deny_rule_.Process("/dev/tty", Rule::ANY_INTERFACE));
}

}  // namespace permission_broker
