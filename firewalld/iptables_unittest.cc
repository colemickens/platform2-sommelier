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
  // Try to punch hole for TCP port 0.
  ASSERT_TRUE(iptables_succeeds.PunchTcpHole(nullptr, 0, &success));
  // Port 0 is not a valid port.
  ASSERT_FALSE(success);
  // Try to punch hole for UDP port 0.
  ASSERT_TRUE(iptables_succeeds.PunchUdpHole(nullptr, 0, &success));
  // Port 0 is not a valid port.
  ASSERT_FALSE(success);
}

TEST_F(IpTablesTest, PunchTcpHoleSucceeds) {
  bool success = false;
  // Punch hole for TCP port 80, should succeed.
  ASSERT_TRUE(iptables_succeeds.PunchTcpHole(nullptr, 80, &success));
  ASSERT_TRUE(success);
  // Punch again, should still succeed.
  ASSERT_TRUE(iptables_succeeds.PunchTcpHole(nullptr, 80, &success));
  ASSERT_TRUE(success);
  // Plug the hole, should succeed.
  ASSERT_TRUE(iptables_succeeds.PlugTcpHole(nullptr, 80, &success));
  ASSERT_TRUE(success);
}

TEST_F(IpTablesTest, PlugTcpHoleSucceeds) {
  bool success = false;
  // Punch hole for TCP port 80, should succeed.
  ASSERT_TRUE(iptables_succeeds.PunchTcpHole(nullptr, 80, &success));
  ASSERT_TRUE(success);
  // Plug the hole, should succeed.
  ASSERT_TRUE(iptables_succeeds.PlugTcpHole(nullptr, 80, &success));
  ASSERT_TRUE(success);
  // Plug again, should fail.
  ASSERT_TRUE(iptables_succeeds.PlugTcpHole(nullptr, 80, &success));
  ASSERT_FALSE(success);
}

TEST_F(IpTablesTest, PunchUdpHoleSucceeds) {
  bool success = false;
  // Punch hole for UDP port 53, should succeed.
  ASSERT_TRUE(iptables_succeeds.PunchUdpHole(nullptr, 53, &success));
  ASSERT_TRUE(success);
  // Punch again, should still succeed.
  ASSERT_TRUE(iptables_succeeds.PunchUdpHole(nullptr, 53, &success));
  ASSERT_TRUE(success);
  // Plug the hole, should succeed.
  ASSERT_TRUE(iptables_succeeds.PlugUdpHole(nullptr, 53, &success));
  ASSERT_TRUE(success);
}

TEST_F(IpTablesTest, PlugUdpHoleSucceeds) {
  bool success = false;
  // Punch hole for UDP port 53, should succeed.
  ASSERT_TRUE(iptables_succeeds.PunchUdpHole(nullptr, 53, &success));
  ASSERT_TRUE(success);
  // Plug the hole, should succeed.
  ASSERT_TRUE(iptables_succeeds.PlugUdpHole(nullptr, 53, &success));
  ASSERT_TRUE(success);
  // Plug again, should fail.
  ASSERT_TRUE(iptables_succeeds.PlugUdpHole(nullptr, 53, &success));
  ASSERT_FALSE(success);
}

TEST_F(IpTablesTest, PunchTcpHoleFails) {
  bool success = false;
  // Punch hole for TCP port 80, should fail.
  ASSERT_TRUE(iptables_fails.PunchTcpHole(nullptr, 80, &success));
  ASSERT_FALSE(success);
}

TEST_F(IpTablesTest, PunchUdpHoleFails) {
  bool success = false;
  // Punch hole for UDP port 53, should fail.
  ASSERT_TRUE(iptables_fails.PunchUdpHole(nullptr, 53, &success));
  ASSERT_FALSE(success);
}

}  // namespace firewalld
