// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "firewalld/iptables.h"

#include <gtest/gtest.h>

namespace firewalld {

class IpTablesTest : public testing::Test {
 public:
  IpTablesTest()
      : iptables_succeeds{"/bin/true"}, iptables_fails{"/bin/false"} {}
  ~IpTablesTest() override = default;

 protected:
  IpTables iptables_succeeds;
  IpTables iptables_fails;

 private:
  DISALLOW_COPY_AND_ASSIGN(IpTablesTest);
};

TEST_F(IpTablesTest, Port0Fails) {
  bool success = false;
  // Try to punch hole for port 0.
  ASSERT_TRUE(iptables_succeeds.PunchHole(nullptr, 0, &success));
  // Port 0 is not a valid port.
  ASSERT_FALSE(success);
}

TEST_F(IpTablesTest, PunchHoleSucceeds) {
  bool success = false;
  // Punch hole for port 80, should succeed.
  ASSERT_TRUE(iptables_succeeds.PunchHole(nullptr, 80, &success));
  ASSERT_TRUE(success);
  // Punch again, should still succeed.
  ASSERT_TRUE(iptables_succeeds.PunchHole(nullptr, 80, &success));
  ASSERT_TRUE(success);
  // Plug the hole, should succeed.
  ASSERT_TRUE(iptables_succeeds.PlugHole(nullptr, 80, &success));
  ASSERT_TRUE(success);
}

TEST_F(IpTablesTest, PlugHoleSucceeds) {
  bool success = false;
  // Punch hole for port 80, should succeed.
  ASSERT_TRUE(iptables_succeeds.PunchHole(nullptr, 80, &success));
  ASSERT_TRUE(success);
  // Plug the hole, should succeed.
  ASSERT_TRUE(iptables_succeeds.PlugHole(nullptr, 80, &success));
  ASSERT_TRUE(success);
  // Plug again, should fail.
  ASSERT_TRUE(iptables_succeeds.PlugHole(nullptr, 80, &success));
  ASSERT_FALSE(success);
}

TEST_F(IpTablesTest, PunchHoleFails) {
  bool success = false;
  // Punch hole for port 80, should fail.
  ASSERT_TRUE(iptables_fails.PunchHole(nullptr, 80, &success));
  ASSERT_FALSE(success);
}

}  // namespace firewalld
