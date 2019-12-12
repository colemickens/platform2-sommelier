// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "permission_broker/firewall.h"

#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "permission_broker/mock_firewall.h"

namespace permission_broker {

using testing::_;
using testing::Return;

class FirewallTest : public testing::Test {
 public:
  FirewallTest() = default;
  ~FirewallTest() override = default;

 protected:
  void SetMockExpectations(MockFirewall* firewall, bool success) {
    // Empty criterion matches all commands.
    firewall->SetRunInMinijailFailCriterion(std::vector<std::string>(), true,
                                            success);
  }

  void SetMockExpectationsPerExecutable(MockFirewall* firewall,
                                        bool ip4_success,
                                        bool ip6_success) {
    if (!ip4_success)
      firewall->SetRunInMinijailFailCriterion(
          std::vector<std::string>({kIpTablesPath}), true /* repeat */,
          false /* omit_failure */);
    if (!ip6_success)
      firewall->SetRunInMinijailFailCriterion(
          std::vector<std::string>({kIp6TablesPath}), true, false);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FirewallTest);
};

TEST_F(FirewallTest, Port0Fails) {
  MockFirewall mock_firewall;
  // Don't fail on anything.
  int id = mock_firewall.SetRunInMinijailFailCriterion(
      std::vector<std::string>(), true, true);

  // Try to punch hole for TCP port 0, port 0 is not a valid port.
  EXPECT_FALSE(mock_firewall.AddAcceptRules(kProtocolTcp, 0, "iface"));
  // Try to punch hole for UDP port 0, port 0 is not a valid port.
  EXPECT_FALSE(mock_firewall.AddAcceptRules(kProtocolUdp, 0, "iface"));

  // Try to plug hole for TCP port 0, port 0 is not a valid port.
  EXPECT_FALSE(mock_firewall.DeleteAcceptRules(kProtocolTcp, 0, "iface"));
  // Try to plug hole for UDP port 0, port 0 is not a valid port.
  EXPECT_FALSE(mock_firewall.DeleteAcceptRules(kProtocolUdp, 0, "iface"));

  // We should not be adding/removing any rules for port 0.
  EXPECT_EQ(mock_firewall.GetRunInMinijailCriterionMatchCount(id), 0);
}

TEST_F(FirewallTest, ValidInterfaceName) {
  MockFirewall mock_firewall;
  SetMockExpectations(&mock_firewall, true /* success */);

  EXPECT_TRUE(mock_firewall.AddAcceptRules(kProtocolTcp, 80, "shortname"));
  EXPECT_TRUE(mock_firewall.AddAcceptRules(kProtocolUdp, 53, "shortname"));
  EXPECT_TRUE(mock_firewall.AddAcceptRules(kProtocolTcp, 80, "middle-dash"));
  EXPECT_TRUE(mock_firewall.AddAcceptRules(kProtocolUdp, 53, "middle-dash"));
  EXPECT_TRUE(mock_firewall.AddAcceptRules(kProtocolTcp, 80, "middle.dot"));
  EXPECT_TRUE(mock_firewall.AddAcceptRules(kProtocolUdp, 53, "middle.dot"));
}

TEST_F(FirewallTest, InvalidInterfaceName) {
  MockFirewall mock_firewall;
  int id = mock_firewall.SetRunInMinijailFailCriterion(
      std::vector<std::string>(), true, true);

  EXPECT_FALSE(mock_firewall.AddAcceptRules(kProtocolTcp, 80,
                                            "reallylonginterfacename"));
  EXPECT_FALSE(mock_firewall.AddAcceptRules(kProtocolTcp, 80, "with spaces"));
  EXPECT_FALSE(mock_firewall.AddAcceptRules(kProtocolTcp, 80, "with$ymbols"));
  EXPECT_FALSE(mock_firewall.AddAcceptRules(kProtocolTcp, 80, "-startdash"));
  EXPECT_FALSE(mock_firewall.AddAcceptRules(kProtocolTcp, 80, "enddash-"));
  EXPECT_FALSE(mock_firewall.AddAcceptRules(kProtocolTcp, 80, ".startdot"));
  EXPECT_FALSE(mock_firewall.AddAcceptRules(kProtocolTcp, 80, "enddot."));

  EXPECT_FALSE(mock_firewall.AddAcceptRules(kProtocolUdp, 53,
                                            "reallylonginterfacename"));
  EXPECT_FALSE(mock_firewall.AddAcceptRules(kProtocolUdp, 53, "with spaces"));
  EXPECT_FALSE(mock_firewall.AddAcceptRules(kProtocolUdp, 53, "with$ymbols"));
  EXPECT_FALSE(mock_firewall.AddAcceptRules(kProtocolUdp, 53, "-startdash"));
  EXPECT_FALSE(mock_firewall.AddAcceptRules(kProtocolUdp, 53, "enddash-"));
  EXPECT_FALSE(mock_firewall.AddAcceptRules(kProtocolUdp, 53, ".startdot"));
  EXPECT_FALSE(mock_firewall.AddAcceptRules(kProtocolUdp, 53, "enddot."));

  EXPECT_FALSE(mock_firewall.DeleteAcceptRules(kProtocolTcp, 80,
                                               "reallylonginterfacename"));
  EXPECT_FALSE(
      mock_firewall.DeleteAcceptRules(kProtocolTcp, 80, "with spaces"));
  EXPECT_FALSE(
      mock_firewall.DeleteAcceptRules(kProtocolTcp, 80, "with$ymbols"));
  EXPECT_FALSE(mock_firewall.DeleteAcceptRules(kProtocolTcp, 80, "-startdash"));
  EXPECT_FALSE(mock_firewall.DeleteAcceptRules(kProtocolTcp, 80, "enddash-"));
  EXPECT_FALSE(mock_firewall.DeleteAcceptRules(kProtocolTcp, 80, ".startdot"));
  EXPECT_FALSE(mock_firewall.DeleteAcceptRules(kProtocolTcp, 80, "enddot."));

  EXPECT_FALSE(mock_firewall.DeleteAcceptRules(kProtocolUdp, 53,
                                               "reallylonginterfacename"));
  EXPECT_FALSE(
      mock_firewall.DeleteAcceptRules(kProtocolUdp, 53, "with spaces"));
  EXPECT_FALSE(
      mock_firewall.DeleteAcceptRules(kProtocolUdp, 53, "with$ymbols"));
  EXPECT_FALSE(mock_firewall.DeleteAcceptRules(kProtocolUdp, 53, "-startdash"));
  EXPECT_FALSE(mock_firewall.DeleteAcceptRules(kProtocolUdp, 53, "enddash-"));
  EXPECT_FALSE(mock_firewall.DeleteAcceptRules(kProtocolUdp, 53, ".startdot"));
  EXPECT_FALSE(mock_firewall.DeleteAcceptRules(kProtocolUdp, 53, "enddot."));

  // We should not be adding/removing any rules for invalid interface names.
  EXPECT_EQ(mock_firewall.GetRunInMinijailCriterionMatchCount(id), 0);
}

TEST_F(FirewallTest, AddAcceptRules_Fails) {
  MockFirewall mock_firewall;
  SetMockExpectations(&mock_firewall, false /* success */);

  // Punch hole for TCP port 80, should fail.
  ASSERT_FALSE(mock_firewall.AddAcceptRules(kProtocolTcp, 80, "iface"));
  // Punch hole for UDP port 53, should fail.
  ASSERT_FALSE(mock_firewall.AddAcceptRules(kProtocolUdp, 53, "iface"));
}

TEST_F(FirewallTest, AddAcceptRules_Ipv6Fails) {
  MockFirewall mock_firewall;
  SetMockExpectationsPerExecutable(&mock_firewall, true /* ip4_success */,
                                   false /* ip6_success */);

  // Punch hole for TCP port 80, should fail because 'ip6tables' fails.
  ASSERT_FALSE(mock_firewall.AddAcceptRules(kProtocolTcp, 80, "iface"));
  // Punch hole for UDP port 53, should fail because 'ip6tables' fails.
  ASSERT_FALSE(mock_firewall.AddAcceptRules(kProtocolUdp, 53, "iface"));
}

TEST_F(FirewallTest, Port0LockdownFails) {
  MockFirewall mock_firewall;
  // Don't fail on anything.
  int id = mock_firewall.SetRunInMinijailFailCriterion(
      std::vector<std::string>(), true, true);

  // Try to lock down TCP port 0, port 0 is not a valid port.
  EXPECT_FALSE(mock_firewall.AddLoopbackLockdownRules(kProtocolTcp, 0));
  // Try to lock down UDP port 0, port 0 is not a valid port.
  EXPECT_FALSE(mock_firewall.AddLoopbackLockdownRules(kProtocolUdp, 0));

  // We should not be adding/removing any rules for port 0.
  EXPECT_EQ(mock_firewall.GetRunInMinijailCriterionMatchCount(id), 0);
}

TEST_F(FirewallTest, AddLoopbackLockdownRules_Success) {
  MockFirewall mock_firewall;
  SetMockExpectations(&mock_firewall, true /* success */);
  ASSERT_TRUE(mock_firewall.AddLoopbackLockdownRules(kProtocolTcp, 80));
  ASSERT_TRUE(mock_firewall.AddLoopbackLockdownRules(kProtocolUdp, 53));
  ASSERT_TRUE(mock_firewall.AddLoopbackLockdownRules(kProtocolTcp, 1234));
  ASSERT_TRUE(mock_firewall.AddLoopbackLockdownRules(kProtocolTcp, 8080));
}

TEST_F(FirewallTest, AddLoopbackLockdownRules_Fails) {
  MockFirewall mock_firewall;
  SetMockExpectations(&mock_firewall, false /* success */);

  // Lock down TCP port 80, should fail.
  ASSERT_FALSE(mock_firewall.AddLoopbackLockdownRules(kProtocolTcp, 80));
  // Lock down UDP port 53, should fail.
  ASSERT_FALSE(mock_firewall.AddLoopbackLockdownRules(kProtocolUdp, 53));
}

TEST_F(FirewallTest, AddLoopbackLockdownRules_Ipv6Fails) {
  MockFirewall mock_firewall;
  SetMockExpectationsPerExecutable(&mock_firewall, true /* ip4_success */,
                                   false /* ip6_success */);

  // Lock down TCP port 80, should fail because 'ip6tables' fails.
  ASSERT_FALSE(mock_firewall.AddLoopbackLockdownRules(kProtocolTcp, 80));
  // Lock down UDP port 53, should fail because 'ip6tables' fails.
  ASSERT_FALSE(mock_firewall.AddLoopbackLockdownRules(kProtocolUdp, 53));
}

TEST_F(FirewallTest, AddAcceptRules_InvalidArguments) {
  MockFirewall mock_firewall;
  // Don't fail on anything.
  int id = mock_firewall.SetRunInMinijailFailCriterion({}, true, true);

  // Invalid input interface
  ASSERT_FALSE(mock_firewall.AddIpv4ForwardRule(
      kProtocolTcp, "", 80, "-startdash", "100.115.92.5", 8080));
  ASSERT_FALSE(mock_firewall.AddIpv4ForwardRule(
      kProtocolUdp, "", 80, "enddash-", "100.115.92.5", 8080));
  ASSERT_FALSE(mock_firewall.DeleteIpv4ForwardRule(
      kProtocolTcp, "", 80, ".startdot", "100.115.92.5", 8080));
  ASSERT_FALSE(mock_firewall.DeleteIpv4ForwardRule(
      kProtocolUdp, "", 80, "enddot.", "100.115.92.5", 8080));
  // Empty interface
  ASSERT_FALSE(mock_firewall.AddIpv4ForwardRule(kProtocolTcp, "", 80, "",
                                                "100.115.92.5", 8080));
  ASSERT_FALSE(mock_firewall.AddIpv4ForwardRule(kProtocolUdp, "", 80, "",
                                                "100.115.92.5", 8080));
  ASSERT_FALSE(mock_firewall.DeleteIpv4ForwardRule(kProtocolTcp, "", 80, "",
                                                   "100.115.92.5", 8080));
  ASSERT_FALSE(mock_firewall.DeleteIpv4ForwardRule(kProtocolUdp, "", 80, "",
                                                   "100.115.92.5", 8080));
  // Invalid input dst address
  ASSERT_FALSE(mock_firewall.AddIpv4ForwardRule(
      kProtocolTcp, "256.256.256.256", 80, "iface", "100.115.92.5", 8080));
  ASSERT_FALSE(mock_firewall.AddIpv4ForwardRule(kProtocolUdp, "qodpjqwpod", 80,
                                                "iface", "100.115.92.5", 8080));
  ASSERT_FALSE(mock_firewall.DeleteIpv4ForwardRule(
      kProtocolTcp, "1.1", 80, "iface", "100.115.92.5", 8080));
  ASSERT_FALSE(mock_firewall.DeleteIpv4ForwardRule(
      kProtocolUdp, "2001:db8::1", 80, "iface", "100.115.92.5", 8080));
  // Invalid input dst port
  ASSERT_FALSE(mock_firewall.AddIpv4ForwardRule(kProtocolTcp, "", 0, "iface",
                                                "100.115.92.5", 8080));
  ASSERT_FALSE(mock_firewall.AddIpv4ForwardRule(kProtocolTcp, "", 0, "iface",
                                                "100.115.92.5", 8080));
  ASSERT_FALSE(mock_firewall.DeleteIpv4ForwardRule(kProtocolUdp, "", 0, "iface",
                                                   "100.115.92.5", 8080));
  ASSERT_FALSE(mock_firewall.DeleteIpv4ForwardRule(kProtocolUdp, "", 0, "iface",
                                                   "100.115.92.5", 8080));
  // Invalid output dst address
  ASSERT_FALSE(mock_firewall.AddIpv4ForwardRule(kProtocolTcp, "", 80, "iface",
                                                "", 8080));
  ASSERT_FALSE(mock_firewall.AddIpv4ForwardRule(kProtocolUdp, "", 80, "iface",
                                                "qodpjqwpod", 8080));
  ASSERT_FALSE(mock_firewall.DeleteIpv4ForwardRule(kProtocolTcp, "", 80,
                                                   "iface", "1.1", 8080));
  ASSERT_FALSE(mock_firewall.DeleteIpv4ForwardRule(
      kProtocolUdp, "", 80, "iface", "2001:db8::1", 8080));
  // Invalid output dst port
  ASSERT_FALSE(mock_firewall.AddIpv4ForwardRule(kProtocolTcp, "", 80, "iface",
                                                "100.115.92.5", 0));
  ASSERT_FALSE(mock_firewall.AddIpv4ForwardRule(kProtocolUdp, "", 80, "iface",
                                                "100.115.92.5", 0));
  ASSERT_FALSE(mock_firewall.DeleteIpv4ForwardRule(kProtocolTcp, "", 80,
                                                   "iface", "100.115.92.5", 0));
  ASSERT_FALSE(mock_firewall.DeleteIpv4ForwardRule(kProtocolUdp, "", 80,
                                                   "iface", "100.115.92.5", 0));

  EXPECT_EQ(mock_firewall.GetRunInMinijailCriterionMatchCount(id), 0);
}

TEST_F(FirewallTest, AddAcceptRules_IptablesFails) {
  MockFirewall mock_firewall;
  SetMockExpectations(&mock_firewall, false /* success */);

  ASSERT_FALSE(mock_firewall.AddIpv4ForwardRule(kProtocolTcp, "", 80, "iface",
                                                "100.115.92.6", 8080));
  ASSERT_FALSE(mock_firewall.AddIpv4ForwardRule(kProtocolUdp, "", 80, "iface",
                                                "100.115.92.6", 8080));
  ASSERT_FALSE(mock_firewall.DeleteIpv4ForwardRule(
      kProtocolTcp, "", 80, "iface", "100.115.92.6", 8080));
  ASSERT_FALSE(mock_firewall.DeleteIpv4ForwardRule(
      kProtocolUdp, "", 80, "iface", "100.115.92.6", 8080));
}

TEST_F(FirewallTest, AddAcceptRules_ValidRules) {
  MockFirewall mock_firewall;
  int id = mock_firewall.SetRunInMinijailFailCriterion({}, true, true);

  // Invalid input interface
  ASSERT_TRUE(mock_firewall.AddIpv4ForwardRule(kProtocolTcp, "", 80, "wlan0",
                                               "100.115.92.2", 8080));
  ASSERT_TRUE(mock_firewall.AddIpv4ForwardRule(
      kProtocolTcp, "100.115.92.2", 5555, "vmtap0", "127.0.0.1", 5550));
  ASSERT_TRUE(mock_firewall.AddIpv4ForwardRule(kProtocolUdp, "", 5353, "eth0",
                                               "192.168.1.1", 5353));
  ASSERT_TRUE(mock_firewall.DeleteIpv4ForwardRule(kProtocolTcp, "", 5000,
                                                  "mlan0", "10.0.0.24", 5001));
  ASSERT_TRUE(mock_firewall.DeleteIpv4ForwardRule(
      kProtocolTcp, "100.115.92.2", 5555, "vmtap0", "127.0.0.1", 5550));
  ASSERT_TRUE(mock_firewall.DeleteIpv4ForwardRule(kProtocolUdp, "", 443, "eth1",
                                                  "1.2.3.4", 443));

  EXPECT_EQ(mock_firewall.GetRunInMinijailCriterionMatchCount(id), 6);

  std::vector<std::string> expected_commands = {
      "/sbin/iptables -t nat -I PREROUTING -i wlan0 -p tcp --dport 80 -j DNAT "
      "--to-destination 100.115.92.2:8080 -w",
      "/sbin/iptables -t nat -I PREROUTING -i vmtap0 -p tcp -d 100.115.92.2 "
      "--dport 5555 -j DNAT --to-destination 127.0.0.1:5550 -w",
      "/sbin/iptables -t nat -I PREROUTING -i eth0 -p udp --dport 5353 -j DNAT "
      "--to-destination 192.168.1.1:5353 -w",
      "/sbin/iptables -t nat -D PREROUTING -i mlan0 -p tcp --dport 5000 -j "
      "DNAT --to-destination 10.0.0.24:5001 -w",
      "/sbin/iptables -t nat -D PREROUTING -i vmtap0 -p tcp -d 100.115.92.2 "
      "--dport 5555 -j DNAT --to-destination 127.0.0.1:5550 -w",
      "/sbin/iptables -t nat -D PREROUTING -i eth1 -p udp --dport 443 -j DNAT "
      "--to-destination 1.2.3.4:443 -w"};
  std::vector<std::string> issued_commands = mock_firewall.GetAllCommands();
  ASSERT_EQ(expected_commands.size(), issued_commands.size());
  for (int i = 0; i < expected_commands.size(); i++) {
    ASSERT_EQ(expected_commands[i], issued_commands[i]);
  }
}
}  // namespace permission_broker
