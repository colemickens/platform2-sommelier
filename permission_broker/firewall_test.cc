// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "permission_broker/firewall.h"

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
  SetMockExpectationsPerExecutable(
      &mock_firewall, true /* ip4_success */, false /* ip6_success */);

  // Punch hole for TCP port 80, should fail because 'ip6tables' fails.
  ASSERT_FALSE(mock_firewall.AddAcceptRules(kProtocolTcp, 80, "iface"));
  // Punch hole for UDP port 53, should fail because 'ip6tables' fails.
  ASSERT_FALSE(mock_firewall.AddAcceptRules(kProtocolUdp, 53, "iface"));
}

}  // namespace permission_broker
