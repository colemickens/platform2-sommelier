// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include "permission_broker/only_allow_group_tty_device_rule.h"

namespace permission_broker {

class OnlyAllowGroupTtyDeviceRuleTest : public testing::Test {
 public:
  OnlyAllowGroupTtyDeviceRuleTest() : rule_("nonexistent") {}
  ~OnlyAllowGroupTtyDeviceRuleTest() override = default;

 protected:
  OnlyAllowGroupTtyDeviceRule rule_;

 private:
  DISALLOW_COPY_AND_ASSIGN(OnlyAllowGroupTtyDeviceRuleTest);
};

TEST_F(OnlyAllowGroupTtyDeviceRuleTest, DenyNonMatchingGroup) {
  ASSERT_EQ(Rule::DENY, rule_.Process("/dev/tty",
                                        Rule::ANY_INTERFACE));
}

}  // namespace permission_broker
