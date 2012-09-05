// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include "permission_broker/deny_usb_vendor_id_rule.h"

static const uint16_t kLinuxFoundationUsbVendorId = 0x1d6b;

namespace permission_broker {

class DenyUsbVendorIdRuleTest : public testing::Test {
 public:
  DenyUsbVendorIdRuleTest() : rule_(kLinuxFoundationUsbVendorId) {}
  virtual ~DenyUsbVendorIdRuleTest() {}

 protected:
  DenyUsbVendorIdRule rule_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DenyUsbVendorIdRuleTest);
};

TEST_F(DenyUsbVendorIdRuleTest, IgnoreNonUsbDevice) {
  ASSERT_EQ(Rule::IGNORE, rule_.Process("/dev/loop0"));
}

TEST_F(DenyUsbVendorIdRuleTest, DenyMatchingUsbDevice) {
  ASSERT_EQ(Rule::DENY, rule_.Process("/dev/bus/usb/001/001"));
}

}  // namespace permission_broker
