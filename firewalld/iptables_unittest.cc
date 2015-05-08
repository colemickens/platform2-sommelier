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
  IpTablesTest() = default;
  ~IpTablesTest() override = default;

 protected:
  void SetMockExpectations(MockIpTables* iptables, bool success) {
    EXPECT_CALL(*iptables, AddAcceptRule(_, _, _, _))
        .WillRepeatedly(Return(success));
    EXPECT_CALL(*iptables, DeleteAcceptRule(_, _, _, _))
        .WillRepeatedly(Return(success));
  }

  void SetMockExpectationsPerExecutable(MockIpTables* iptables,
                                        bool ip4_success,
                                        bool ip6_success) {
    EXPECT_CALL(*iptables, AddAcceptRule("/sbin/iptables", _, _, _))
        .WillRepeatedly(Return(ip4_success));
    EXPECT_CALL(*iptables, AddAcceptRule("/sbin/ip6tables", _, _, _))
        .WillRepeatedly(Return(ip6_success));
    EXPECT_CALL(*iptables, DeleteAcceptRule("/sbin/iptables", _, _, _))
        .WillRepeatedly(Return(ip4_success));
    EXPECT_CALL(*iptables, DeleteAcceptRule("/sbin/ip6tables", _, _, _))
        .WillRepeatedly(Return(ip6_success));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(IpTablesTest);
};

TEST_F(IpTablesTest, Port0Fails) {
  MockIpTables mock_iptables;
  // We should not be adding any rules for port 0.
  EXPECT_CALL(mock_iptables, AddAcceptRule(_, _, _, _)).Times(0);
  EXPECT_CALL(mock_iptables, DeleteAcceptRule(_, _, _, _)).Times(0);
  // Try to punch hole for TCP port 0, port 0 is not a valid port.
  EXPECT_FALSE(mock_iptables.PunchTcpHole(0, "iface"));
  // Try to punch hole for UDP port 0, port 0 is not a valid port.
  EXPECT_FALSE(mock_iptables.PunchUdpHole(0, "iface"));
}

TEST_F(IpTablesTest, ValidInterfaceName) {
  MockIpTables mock_iptables;
  SetMockExpectations(&mock_iptables, true /* success */);

  EXPECT_TRUE(mock_iptables.PunchTcpHole(80, "shortname"));
  EXPECT_TRUE(mock_iptables.PunchUdpHole(53, "shortname"));
  EXPECT_TRUE(mock_iptables.PunchTcpHole(80, "middle-dash"));
  EXPECT_TRUE(mock_iptables.PunchUdpHole(53, "middle-dash"));
}

TEST_F(IpTablesTest, InvalidInterfaceName) {
  MockIpTables mock_iptables;
  // We should not be adding any rules for invalid interface names.
  EXPECT_CALL(mock_iptables, AddAcceptRule(_, _, _, _)).Times(0);
  EXPECT_CALL(mock_iptables, DeleteAcceptRule(_, _, _, _)).Times(0);

  EXPECT_FALSE(mock_iptables.PunchTcpHole(80, "reallylonginterfacename"));
  EXPECT_FALSE(mock_iptables.PunchTcpHole(80, "with spaces"));
  EXPECT_FALSE(mock_iptables.PunchTcpHole(80, "with$ymbols"));
  EXPECT_FALSE(mock_iptables.PunchTcpHole(80, "-startdash"));
  EXPECT_FALSE(mock_iptables.PunchTcpHole(80, "enddash-"));

  EXPECT_FALSE(mock_iptables.PunchUdpHole(53, "reallylonginterfacename"));
  EXPECT_FALSE(mock_iptables.PunchUdpHole(53, "with spaces"));
  EXPECT_FALSE(mock_iptables.PunchUdpHole(53, "with$ymbols"));
  EXPECT_FALSE(mock_iptables.PunchUdpHole(53, "-startdash"));
  EXPECT_FALSE(mock_iptables.PunchUdpHole(53, "enddash-"));
}

TEST_F(IpTablesTest, PunchTcpHoleSucceeds) {
  MockIpTables mock_iptables;
  SetMockExpectations(&mock_iptables, true /* success */);

  // Punch hole for TCP port 80, should succeed.
  EXPECT_TRUE(mock_iptables.PunchTcpHole(80, "iface"));
  // Punch again, should still succeed.
  EXPECT_TRUE(mock_iptables.PunchTcpHole(80, "iface"));
  // Plug the hole, should succeed.
  EXPECT_TRUE(mock_iptables.PlugTcpHole(80, "iface"));
}

TEST_F(IpTablesTest, PlugTcpHoleSucceeds) {
  MockIpTables mock_iptables;
  SetMockExpectations(&mock_iptables, true /* success */);

  // Punch hole for TCP port 80, should succeed.
  EXPECT_TRUE(mock_iptables.PunchTcpHole(80, "iface"));
  // Plug the hole, should succeed.
  EXPECT_TRUE(mock_iptables.PlugTcpHole(80, "iface"));
  // Plug again, should fail.
  EXPECT_FALSE(mock_iptables.PlugTcpHole(80, "iface"));
}

TEST_F(IpTablesTest, PunchUdpHoleSucceeds) {
  MockIpTables mock_iptables;
  SetMockExpectations(&mock_iptables, true /* success */);

  // Punch hole for UDP port 53, should succeed.
  EXPECT_TRUE(mock_iptables.PunchUdpHole(53, "iface"));
  // Punch again, should still succeed.
  EXPECT_TRUE(mock_iptables.PunchUdpHole(53, "iface"));
  // Plug the hole, should succeed.
  EXPECT_TRUE(mock_iptables.PlugUdpHole(53, "iface"));
}

TEST_F(IpTablesTest, PlugUdpHoleSucceeds) {
  MockIpTables mock_iptables;
  SetMockExpectations(&mock_iptables, true /* success */);

  // Punch hole for UDP port 53, should succeed.
  EXPECT_TRUE(mock_iptables.PunchUdpHole(53, "iface"));
  // Plug the hole, should succeed.
  EXPECT_TRUE(mock_iptables.PlugUdpHole(53, "iface"));
  // Plug again, should fail.
  EXPECT_FALSE(mock_iptables.PlugUdpHole(53, "iface"));
}

TEST_F(IpTablesTest, PunchTcpHoleFails) {
  MockIpTables mock_iptables;
  SetMockExpectations(&mock_iptables, false /* success */);
  // Punch hole for TCP port 80, should fail.
  ASSERT_FALSE(mock_iptables.PunchTcpHole(80, "iface"));
}

TEST_F(IpTablesTest, PunchUdpHoleFails) {
  MockIpTables mock_iptables;
  SetMockExpectations(&mock_iptables, false /* success */);
  // Punch hole for UDP port 53, should fail.
  ASSERT_FALSE(mock_iptables.PunchUdpHole(53, "iface"));
}

TEST_F(IpTablesTest, PunchTcpHoleIpv6Fails) {
  MockIpTables mock_iptables;
  SetMockExpectationsPerExecutable(&mock_iptables, true /* ip4_success */,
                                   false /* ip6_success */);
  // Punch hole for TCP port 80, should fail because 'ip6tables' fails.
  ASSERT_FALSE(mock_iptables.PunchTcpHole(80, "iface"));
}

TEST_F(IpTablesTest, PunchUdpHoleIpv6Fails) {
  MockIpTables mock_iptables;
  SetMockExpectationsPerExecutable(&mock_iptables, true /* ip4_success */,
                                   false /* ip6_success */);
  // Punch hole for UDP port 53, should fail because 'ip6tables' fails.
  ASSERT_FALSE(mock_iptables.PunchUdpHole(53, "iface"));
}

TEST_F(IpTablesTest, ApplyVpnSetupAddSuccess) {
  const std::vector<std::string> usernames = {"testuser0", "testuser1"};
  const std::string interface = "ifc0";
  const bool add = true;

  MockIpTables mock_iptables;
  EXPECT_CALL(mock_iptables, ApplyMasquerade(interface, add))
      .WillOnce(Return(true));
  EXPECT_CALL(mock_iptables,
              ApplyMarkForUserTraffic(usernames[0], add))
      .WillOnce(Return(true));
  EXPECT_CALL(mock_iptables,
              ApplyMarkForUserTraffic(usernames[1], add))
      .WillOnce(Return(true));
  EXPECT_CALL(mock_iptables, ApplyRuleForUserTraffic(add))
      .WillOnce(Return(true));

  ASSERT_TRUE(
      mock_iptables.ApplyVpnSetup(usernames, interface, add));
}

TEST_F(IpTablesTest, ApplyVpnSetupAddFailureInUsername) {
  const std::vector<std::string> usernames = {"testuser0", "testuser1"};
  const std::string interface = "ifc0";
  const bool remove = false;
  const bool add = true;

  MockIpTables mock_iptables;
  EXPECT_CALL(mock_iptables, ApplyMasquerade(interface, add))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(mock_iptables,
              ApplyMarkForUserTraffic(usernames[0], add))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(mock_iptables,
              ApplyMarkForUserTraffic(usernames[1], add))
      .Times(1)
      .WillOnce(Return(false));
  EXPECT_CALL(mock_iptables, ApplyRuleForUserTraffic(add))
      .Times(1)
      .WillOnce(Return(true));

  EXPECT_CALL(mock_iptables, ApplyMasquerade(interface, remove))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(mock_iptables,
              ApplyMarkForUserTraffic(usernames[0], remove))
      .Times(1)
      .WillOnce(Return(false));
  EXPECT_CALL(mock_iptables,
              ApplyMarkForUserTraffic(usernames[1], remove)).Times(0);
  EXPECT_CALL(mock_iptables, ApplyRuleForUserTraffic(remove))
      .Times(1)
      .WillOnce(Return(false));

  ASSERT_FALSE(
      mock_iptables.ApplyVpnSetup(usernames, interface, add));
}

TEST_F(IpTablesTest, ApplyVpnSetupAddFailureInMasquerade) {
  const std::vector<std::string> usernames = {"testuser0", "testuser1"};
  const std::string interface = "ifc0";
  const bool remove = false;
  const bool add = true;

  MockIpTables mock_iptables;
  EXPECT_CALL(mock_iptables, ApplyMasquerade(interface, add))
      .Times(1)
      .WillOnce(Return(false));
  EXPECT_CALL(mock_iptables, ApplyMarkForUserTraffic(_, _)).Times(0);
  EXPECT_CALL(mock_iptables, ApplyRuleForUserTraffic(add))
      .Times(1)
      .WillOnce(Return(true));

  EXPECT_CALL(mock_iptables, ApplyMasquerade(interface, remove))
      .Times(0);
  EXPECT_CALL(mock_iptables, ApplyRuleForUserTraffic(remove))
      .Times(1)
      .WillOnce(Return(true));

  ASSERT_FALSE(
      mock_iptables.ApplyVpnSetup(usernames, interface, add));
}

TEST_F(IpTablesTest, ApplyVpnSetupAddFailureInRuleForUserTraffic) {
  const std::vector<std::string> usernames = {"testuser0", "testuser1"};
  const std::string interface = "ifc0";
  const bool remove = false;
  const bool add = true;

  MockIpTables mock_iptables;
  EXPECT_CALL(mock_iptables, ApplyMasquerade(interface, _)).Times(0);
  EXPECT_CALL(mock_iptables, ApplyMarkForUserTraffic(_, _)).Times(0);
  EXPECT_CALL(mock_iptables, ApplyRuleForUserTraffic(add))
      .Times(1)
      .WillOnce(Return(false));

  EXPECT_CALL(mock_iptables, ApplyRuleForUserTraffic(remove)).Times(0);

  ASSERT_FALSE(
      mock_iptables.ApplyVpnSetup(usernames, interface, add));
}

TEST_F(IpTablesTest, ApplyVpnSetupRemoveSuccess) {
  const std::vector<std::string> usernames = {"testuser0", "testuser1"};
  const std::string interface = "ifc0";
  const bool remove = false;
  const bool add = true;

  MockIpTables mock_iptables;
  EXPECT_CALL(mock_iptables, ApplyMasquerade(interface, remove))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(mock_iptables, ApplyMarkForUserTraffic(_, remove))
      .Times(2)
      .WillRepeatedly(Return(true));
  EXPECT_CALL(mock_iptables, ApplyRuleForUserTraffic(remove))
      .Times(1)
      .WillOnce(Return(true));

  EXPECT_CALL(mock_iptables, ApplyMasquerade(interface, add))
      .Times(0);
  EXPECT_CALL(mock_iptables, ApplyMarkForUserTraffic(_, add))
      .Times(0);
  EXPECT_CALL(mock_iptables, ApplyRuleForUserTraffic(add)).Times(0);

  ASSERT_TRUE(
      mock_iptables.ApplyVpnSetup(usernames, interface, remove));
}

TEST_F(IpTablesTest, ApplyVpnSetupRemoveFailure) {
  const std::vector<std::string> usernames = {"testuser0", "testuser1"};
  const std::string interface = "ifc0";
  const bool remove = false;
  const bool add = true;

  MockIpTables mock_iptables;
  EXPECT_CALL(mock_iptables, ApplyMasquerade(interface, remove))
      .Times(1)
      .WillOnce(Return(false));
  EXPECT_CALL(mock_iptables, ApplyMarkForUserTraffic(_, remove))
      .Times(2)
      .WillRepeatedly(Return(false));
  EXPECT_CALL(mock_iptables, ApplyRuleForUserTraffic(remove))
      .Times(1)
      .WillOnce(Return(false));

  EXPECT_CALL(mock_iptables, ApplyMasquerade(interface, add))
      .Times(0);
  EXPECT_CALL(mock_iptables, ApplyMarkForUserTraffic(_, add))
      .Times(0);
  EXPECT_CALL(mock_iptables, ApplyRuleForUserTraffic(add)).Times(0);

  ASSERT_FALSE(
      mock_iptables.ApplyVpnSetup(usernames, interface, remove));
}

}  // namespace firewalld
