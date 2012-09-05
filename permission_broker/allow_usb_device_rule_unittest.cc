// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include "permission_broker/allow_usb_device_rule.h"

namespace permission_broker {

class AllowUsbDeviceRuleTest : public testing::Test {
 public:
  AllowUsbDeviceRuleTest() {}
  virtual ~AllowUsbDeviceRuleTest() {}

 protected:
  AllowUsbDeviceRule rule_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AllowUsbDeviceRuleTest);
};

TEST_F(AllowUsbDeviceRuleTest, IgnoreNonUsbDevice) {
  ASSERT_EQ(Rule::IGNORE, rule_.Process("/dev/loop0"));
}

TEST_F(AllowUsbDeviceRuleTest, AllowUsbDevice) {
  ASSERT_EQ(Rule::ALLOW, rule_.Process("/dev/bus/usb/001/001"));
}

}  // namespace permission_broker
