// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include "permission_broker/allow_tty_device_rule.h"

namespace permission_broker {

class AllowTtyDeviceRuleTest : public testing::Test {
 public:
  AllowTtyDeviceRuleTest() = default;
  ~AllowTtyDeviceRuleTest() override = default;

 protected:
  AllowTtyDeviceRule rule_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AllowTtyDeviceRuleTest);
};

TEST_F(AllowTtyDeviceRuleTest, IgnoreNonTtyDevice) {
  ASSERT_EQ(Rule::IGNORE, rule_.Process("/dev/loop0",
                                        Rule::ANY_INTERFACE));
}

TEST_F(AllowTtyDeviceRuleTest, AllowTtyDevice) {
  ASSERT_EQ(Rule::ALLOW, rule_.Process("/dev/tty",
                                       Rule::ANY_INTERFACE));
}

}  // namespace permission_broker
