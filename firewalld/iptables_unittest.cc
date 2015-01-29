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
  // Try to punch hole for TCP port 0, port 0 is not a valid port.
  ASSERT_FALSE(iptables_succeeds.PunchTcpHole(0));
  // Try to punch hole for UDP port 0, port 0 is not a valid port.
  ASSERT_FALSE(iptables_succeeds.PunchUdpHole(0));
}

TEST_F(IpTablesTest, PunchTcpHoleSucceeds) {
  // Punch hole for TCP port 80, should succeed.
  ASSERT_TRUE(iptables_succeeds.PunchTcpHole(80));
  // Punch again, should still succeed.
  ASSERT_TRUE(iptables_succeeds.PunchTcpHole(80));
  // Plug the hole, should succeed.
  ASSERT_TRUE(iptables_succeeds.PlugTcpHole(80));
}

TEST_F(IpTablesTest, PlugTcpHoleSucceeds) {
  // Punch hole for TCP port 80, should succeed.
  ASSERT_TRUE(iptables_succeeds.PunchTcpHole(80));
  // Plug the hole, should succeed.
  ASSERT_TRUE(iptables_succeeds.PlugTcpHole(80));
  // Plug again, should fail.
  ASSERT_FALSE(iptables_succeeds.PlugTcpHole(80));
}

TEST_F(IpTablesTest, PunchUdpHoleSucceeds) {
  // Punch hole for UDP port 53, should succeed.
  ASSERT_TRUE(iptables_succeeds.PunchUdpHole(53));
  // Punch again, should still succeed.
  ASSERT_TRUE(iptables_succeeds.PunchUdpHole(53));
  // Plug the hole, should succeed.
  ASSERT_TRUE(iptables_succeeds.PlugUdpHole(53));
}

TEST_F(IpTablesTest, PlugUdpHoleSucceeds) {
  // Punch hole for UDP port 53, should succeed.
  ASSERT_TRUE(iptables_succeeds.PunchUdpHole(53));
  // Plug the hole, should succeed.
  ASSERT_TRUE(iptables_succeeds.PlugUdpHole(53));
  // Plug again, should fail.
  ASSERT_FALSE(iptables_succeeds.PlugUdpHole(53));
}

TEST_F(IpTablesTest, PunchTcpHoleFails) {
  // Punch hole for TCP port 80, should fail.
  ASSERT_FALSE(iptables_fails.PunchTcpHole(80));
}

TEST_F(IpTablesTest, PunchUdpHoleFails) {
  // Punch hole for UDP port 53, should fail.
  ASSERT_FALSE(iptables_fails.PunchUdpHole(53));
}

}  // namespace firewalld
