// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include "usb_bouncer/util.h"

namespace usb_bouncer {

TEST(UtilTest, IncludeRuleAtLockscreen) {
  EXPECT_FALSE(IncludeRuleAtLockscreen(""));

  const std::string blocked_device =
      "allow id 0781:5588 serial \"12345678BF05\" name \"USB Extreme Pro\" "
      "hash \"9hMkYEMPjuNegGmzLIKwUp2MPctSL0tCWk7ruWGuOzc=\" with-interface "
      "08:06:50 with-connect-type \"unknown\"";
  EXPECT_FALSE(IncludeRuleAtLockscreen(blocked_device))
      << "Failed to filter: " << blocked_device;

  const std::string allowed_device =
      "allow id 0bda:8153 serial \"000001000000\" name \"USB 10/100/1000 LAN\" "
      "hash \"dljXy8thtljhoJo+O+hfhSlp1J89rz0Z4404iqKzakI=\" with-interface "
      "{ ff:ff:00 02:06:00 0a:00:00 0a:00:00 }";
  EXPECT_TRUE(IncludeRuleAtLockscreen(allowed_device))
      << "Failed to allow:" << allowed_device;
}

}  // namespace usb_bouncer
