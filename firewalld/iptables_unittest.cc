// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "firewalld/iptables.h"

#include <gtest/gtest.h>

#include "firewalld/mock_iptables.h"

namespace firewalld {

using testing::_;
using testing::Return;

class IpTablesTest : public testing::Test {
 public:
  IpTablesTest()
      : iptables_succeeds{"/bin/true", "/bin/true"},
        iptables_fails{"/bin/false", "/bin/false"},
        ip4succeeds_ip6fails{"/bin/true", "/bin/false"} {}
  ~IpTablesTest() override = default;

 protected:
  IpTables iptables_succeeds;
  IpTables iptables_fails;
  IpTables ip4succeeds_ip6fails;

 private:
  DISALLOW_COPY_AND_ASSIGN(IpTablesTest);
};

TEST_F(IpTablesTest, Port0Fails) {
  // Try to punch hole for TCP port 0, port 0 is not a valid port.
  ASSERT_FALSE(iptables_succeeds.PunchTcpHole(0, "iface"));
  // Try to punch hole for UDP port 0, port 0 is not a valid port.
  ASSERT_FALSE(iptables_succeeds.PunchUdpHole(0, "iface"));
}

TEST_F(IpTablesTest, InvalidInterfaceName) {
  ASSERT_FALSE(iptables_succeeds.PunchTcpHole(80, "reallylonginterfacename"));
  ASSERT_FALSE(iptables_succeeds.PunchTcpHole(80, "with spaces"));
  ASSERT_FALSE(iptables_succeeds.PunchTcpHole(80, "with$ymbols"));

  ASSERT_FALSE(iptables_succeeds.PunchUdpHole(53, "reallylonginterfacename"));
  ASSERT_FALSE(iptables_succeeds.PunchUdpHole(53, "with spaces"));
  ASSERT_FALSE(iptables_succeeds.PunchUdpHole(53, "with$ymbols"));
  ASSERT_FALSE(iptables_succeeds.PunchUdpHole(53, "-startdash"));
  ASSERT_FALSE(iptables_succeeds.PunchUdpHole(53, "enddash-"));
}

TEST_F(IpTablesTest, ValidInterfaceName) {
  ASSERT_TRUE(iptables_succeeds.PunchUdpHole(53, "middle-dash"));
}

TEST_F(IpTablesTest, PunchTcpHoleSucceeds) {
  // Punch hole for TCP port 80, should succeed.
  ASSERT_TRUE(iptables_succeeds.PunchTcpHole(80, "iface"));
  // Punch again, should still succeed.
  ASSERT_TRUE(iptables_succeeds.PunchTcpHole(80, "iface"));
  // Plug the hole, should succeed.
  ASSERT_TRUE(iptables_succeeds.PlugTcpHole(80, "iface"));
}

TEST_F(IpTablesTest, PlugTcpHoleSucceeds) {
  // Punch hole for TCP port 80, should succeed.
  ASSERT_TRUE(iptables_succeeds.PunchTcpHole(80, "iface"));
  // Plug the hole, should succeed.
  ASSERT_TRUE(iptables_succeeds.PlugTcpHole(80, "iface"));
  // Plug again, should fail.
  ASSERT_FALSE(iptables_succeeds.PlugTcpHole(80, "iface"));
}

TEST_F(IpTablesTest, PunchUdpHoleSucceeds) {
  // Punch hole for UDP port 53, should succeed.
  ASSERT_TRUE(iptables_succeeds.PunchUdpHole(53, "iface"));
  // Punch again, should still succeed.
  ASSERT_TRUE(iptables_succeeds.PunchUdpHole(53, "iface"));
  // Plug the hole, should succeed.
  ASSERT_TRUE(iptables_succeeds.PlugUdpHole(53, "iface"));
}

TEST_F(IpTablesTest, PlugUdpHoleSucceeds) {
  // Punch hole for UDP port 53, should succeed.
  ASSERT_TRUE(iptables_succeeds.PunchUdpHole(53, "iface"));
  // Plug the hole, should succeed.
  ASSERT_TRUE(iptables_succeeds.PlugUdpHole(53, "iface"));
  // Plug again, should fail.
  ASSERT_FALSE(iptables_succeeds.PlugUdpHole(53, "iface"));
}

TEST_F(IpTablesTest, PunchTcpHoleFails) {
  // Punch hole for TCP port 80, should fail.
  ASSERT_FALSE(iptables_fails.PunchTcpHole(80, "iface"));
}

TEST_F(IpTablesTest, PunchUdpHoleFails) {
  // Punch hole for UDP port 53, should fail.
  ASSERT_FALSE(iptables_fails.PunchUdpHole(53, "iface"));
}

TEST_F(IpTablesTest, PunchTcpHoleIpv6Fails) {
  // Punch hole for TCP port 80, should fail because 'ip6tables' fails.
  ASSERT_FALSE(ip4succeeds_ip6fails.PunchTcpHole(80, "iface"));
}

TEST_F(IpTablesTest, PunchUdpHoleIpv6Fails) {
  // Punch hole for UDP port 53, should fail because 'ip6tables' fails.
  ASSERT_FALSE(ip4succeeds_ip6fails.PunchUdpHole(53, "iface"));
}

TEST_F(IpTablesTest, ApplyVpnSetupAddSuccess) {
  const std::vector<std::string> usernames = {"testuser0", "testuser1"};
  const std::string interface = "ifc0";
  const bool add = true;
  const bool success = true;

  MockIpTables mock_iptables;
  EXPECT_CALL(mock_iptables, ApplyMasquerade(interface, add))
      .WillOnce(Return(success));
  EXPECT_CALL(mock_iptables, ApplyMarkForUserTraffic(usernames[0], add))
      .WillOnce(Return(success));
  EXPECT_CALL(mock_iptables, ApplyMarkForUserTraffic(usernames[1], add))
      .WillOnce(Return(success));
  EXPECT_CALL(mock_iptables, ApplyRuleForUserTraffic(add))
      .WillOnce(Return(success));

  EXPECT_EQ(success, mock_iptables.ApplyVpnSetup(usernames, interface, add));
}

TEST_F(IpTablesTest, ApplyVpnSetupAddFailureInUsername) {
  const std::vector<std::string> usernames = {"testuser0", "testuser1"};
  const std::string interface = "ifc0";
  const bool remove = false;
  const bool add = true;
  const bool failure = false;
  const bool success = true;

  MockIpTables mock_iptables;
  EXPECT_CALL(mock_iptables, ApplyMasquerade(interface, add))
      .Times(1)
      .WillOnce(Return(success));
  EXPECT_CALL(mock_iptables, ApplyMarkForUserTraffic(usernames[0], add))
      .Times(1)
      .WillOnce(Return(success));
  EXPECT_CALL(mock_iptables, ApplyMarkForUserTraffic(usernames[1], add))
      .Times(1)
      .WillOnce(Return(failure));
  EXPECT_CALL(mock_iptables, ApplyRuleForUserTraffic(add))
      .Times(1)
      .WillOnce(Return(success));

  EXPECT_CALL(mock_iptables, ApplyMasquerade(interface, remove))
      .Times(1)
      .WillOnce(Return(success));
  EXPECT_CALL(mock_iptables, ApplyMarkForUserTraffic(usernames[0], remove))
      .Times(1)
      .WillOnce(Return(failure));
  EXPECT_CALL(mock_iptables, ApplyMarkForUserTraffic(usernames[1], remove))
      .Times(0);
  EXPECT_CALL(mock_iptables, ApplyRuleForUserTraffic(remove))
      .Times(1)
      .WillOnce(Return(failure));

  EXPECT_EQ(failure, mock_iptables.ApplyVpnSetup(usernames, interface, add));
}

TEST_F(IpTablesTest, ApplyVpnSetupAddFailureInMasquerade) {
  const std::vector<std::string> usernames = {"testuser0", "testuser1"};
  const std::string interface = "ifc0";
  const bool remove = false;
  const bool add = true;
  const bool failure = false;
  const bool success = true;

  MockIpTables mock_iptables;
  EXPECT_CALL(mock_iptables, ApplyMasquerade(interface, add))
      .Times(1)
      .WillOnce(Return(failure));
  EXPECT_CALL(mock_iptables, ApplyMarkForUserTraffic(_, _)).Times(0);
  EXPECT_CALL(mock_iptables, ApplyRuleForUserTraffic(add))
      .Times(1)
      .WillOnce(Return(success));

  EXPECT_CALL(mock_iptables, ApplyMasquerade(interface, remove)).Times(0);
  EXPECT_CALL(mock_iptables, ApplyRuleForUserTraffic(remove))
      .Times(1)
      .WillOnce(Return(success));

  EXPECT_EQ(failure, mock_iptables.ApplyVpnSetup(usernames, interface, add));
}

TEST_F(IpTablesTest, ApplyVpnSetupAddFailureInRuleForUserTraffic) {
  const std::vector<std::string> usernames = {"testuser0", "testuser1"};
  const std::string interface = "ifc0";
  const bool remove = false;
  const bool add = true;
  const bool failure = false;

  MockIpTables mock_iptables;
  EXPECT_CALL(mock_iptables, ApplyMasquerade(interface, _)).Times(0);
  EXPECT_CALL(mock_iptables, ApplyMarkForUserTraffic(_, _)).Times(0);
  EXPECT_CALL(mock_iptables, ApplyRuleForUserTraffic(add))
      .Times(1)
      .WillOnce(Return(failure));

  EXPECT_CALL(mock_iptables, ApplyRuleForUserTraffic(remove)).Times(0);

  EXPECT_EQ(failure, mock_iptables.ApplyVpnSetup(usernames, interface, add));
}

TEST_F(IpTablesTest, ApplyVpnSetupRemoveSuccess) {
  const std::vector<std::string> usernames = {"testuser0", "testuser1"};
  const std::string interface = "ifc0";
  const bool remove = false;
  const bool add = true;
  const bool success = true;

  MockIpTables mock_iptables;
  EXPECT_CALL(mock_iptables, ApplyMasquerade(interface, remove))
      .Times(1)
      .WillOnce(Return(success));
  EXPECT_CALL(mock_iptables, ApplyMarkForUserTraffic(_, remove))
      .Times(2)
      .WillRepeatedly(Return(success));
  EXPECT_CALL(mock_iptables, ApplyRuleForUserTraffic(remove))
      .Times(1)
      .WillOnce(Return(success));

  EXPECT_CALL(mock_iptables, ApplyMasquerade(interface, add)).Times(0);
  EXPECT_CALL(mock_iptables, ApplyMarkForUserTraffic(_, add)).Times(0);
  EXPECT_CALL(mock_iptables, ApplyRuleForUserTraffic(add)).Times(0);

  EXPECT_EQ(success, mock_iptables.ApplyVpnSetup(usernames, interface, remove));
}

TEST_F(IpTablesTest, ApplyVpnSetupRemoveFailure) {
  const std::vector<std::string> usernames = {"testuser0", "testuser1"};
  const std::string interface = "ifc0";
  const bool remove = false;
  const bool add = true;
  const bool failure = false;

  MockIpTables mock_iptables;
  EXPECT_CALL(mock_iptables, ApplyMasquerade(interface, remove))
      .Times(1)
      .WillOnce(Return(failure));
  EXPECT_CALL(mock_iptables, ApplyMarkForUserTraffic(_, remove))
      .Times(2)
      .WillRepeatedly(Return(failure));
  EXPECT_CALL(mock_iptables, ApplyRuleForUserTraffic(remove))
      .Times(1)
      .WillOnce(Return(failure));

  EXPECT_CALL(mock_iptables, ApplyMasquerade(interface, add)).Times(0);
  EXPECT_CALL(mock_iptables, ApplyMarkForUserTraffic(_, add)).Times(0);
  EXPECT_CALL(mock_iptables, ApplyRuleForUserTraffic(add)).Times(0);

  EXPECT_EQ(failure, mock_iptables.ApplyVpnSetup(usernames, interface, remove));
}

}  // namespace firewalld
