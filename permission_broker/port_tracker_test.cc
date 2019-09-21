// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "permission_broker/port_tracker.h"

#include <string>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "permission_broker/mock_firewall.h"

using ::testing::_;
using ::testing::Return;
using ::testing::SetArgPointee;

namespace permission_broker {

class MockPortTracker : public PortTracker {
 public:
  explicit MockPortTracker(Firewall* firewall)
      : PortTracker(nullptr, firewall) {}
  ~MockPortTracker() override = default;

  MOCK_METHOD(int, AddLifelineFd, (int), (override));
  MOCK_METHOD(bool, DeleteLifelineFd, (int), (override));
  MOCK_METHOD(void, CheckLifelineFds, (bool), (override));
  MOCK_METHOD(void, ScheduleLifelineCheck, (), (override));

  MOCK_METHOD(bool, InitializeEpollOnce, (), (override));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockPortTracker);
};

class PortTrackerTest : public testing::Test {
 public:
  PortTrackerTest() : port_tracker_{&mock_firewall_} {}
  ~PortTrackerTest() override = default;

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

  MockFirewall mock_firewall_;
  MockPortTracker port_tracker_;

  uint16_t tcp_port = 8080;
  uint16_t udp_port = 5353;

  std::string interface = "interface";

  int dbus_fd = 3;  // First fd not std{in|out|err}. Doesn't get used at all.
  int tracked_fd = 4;  // Next "available" fd. Used only as a placeholder.

 private:
  DISALLOW_COPY_AND_ASSIGN(PortTrackerTest);
};

TEST_F(PortTrackerTest, AllowTcpPortAccess_Success) {
  SetMockExpectations(&mock_firewall_, true /* success */);
  EXPECT_CALL(port_tracker_, AddLifelineFd(dbus_fd)).WillOnce(Return(0));
  ASSERT_TRUE(port_tracker_.AllowTcpPortAccess(tcp_port, interface, dbus_fd));
}

TEST_F(PortTrackerTest, AllowUdpPortAccess_Success) {
  SetMockExpectations(&mock_firewall_, true /* success */);
  EXPECT_CALL(port_tracker_, AddLifelineFd(dbus_fd)).WillOnce(Return(0));
  ASSERT_TRUE(port_tracker_.AllowUdpPortAccess(udp_port, interface, dbus_fd));
}

TEST_F(PortTrackerTest, AllowTcpPortAccess_Twice) {
  SetMockExpectations(&mock_firewall_, true /* success */);
  EXPECT_CALL(port_tracker_, AddLifelineFd(dbus_fd)).WillOnce(Return(0));
  EXPECT_CALL(port_tracker_, CheckLifelineFds(false));
  ASSERT_TRUE(port_tracker_.AllowTcpPortAccess(tcp_port, interface, dbus_fd));
  ASSERT_FALSE(port_tracker_.AllowTcpPortAccess(tcp_port, interface, dbus_fd));
}

TEST_F(PortTrackerTest, AllowUdpPortAccess_Twice) {
  SetMockExpectations(&mock_firewall_, true /* success */);
  EXPECT_CALL(port_tracker_, AddLifelineFd(dbus_fd)).WillOnce(Return(0));
  EXPECT_CALL(port_tracker_, CheckLifelineFds(false));
  ASSERT_TRUE(port_tracker_.AllowUdpPortAccess(udp_port, interface, dbus_fd));
  ASSERT_FALSE(port_tracker_.AllowUdpPortAccess(udp_port, interface, dbus_fd));
}

TEST_F(PortTrackerTest, AllowTcpPortAccess_FirewallFailure) {
  // Make 'iptables' fail.
  SetMockExpectations(&mock_firewall_, false /* success */);
  EXPECT_CALL(port_tracker_, AddLifelineFd(dbus_fd))
      .WillOnce(Return(tracked_fd));
  EXPECT_CALL(port_tracker_, DeleteLifelineFd(tracked_fd))
      .WillOnce(Return(true));
  ASSERT_FALSE(port_tracker_.AllowTcpPortAccess(tcp_port, interface, dbus_fd));
}

TEST_F(PortTrackerTest, AllowUdpPortAccess_FirewallFailure) {
  // Make 'iptables' fail.
  SetMockExpectations(&mock_firewall_, false /* success */);
  EXPECT_CALL(port_tracker_, AddLifelineFd(dbus_fd))
      .WillOnce(Return(tracked_fd));
  EXPECT_CALL(port_tracker_, DeleteLifelineFd(tracked_fd))
      .WillOnce(Return(true));
  ASSERT_FALSE(port_tracker_.AllowUdpPortAccess(udp_port, interface, dbus_fd));
}

TEST_F(PortTrackerTest, AllowTcpPortAccess_EpollFailure) {
  SetMockExpectations(&mock_firewall_, true /* success */);
  // Make epoll(7) fail.
  EXPECT_CALL(port_tracker_, AddLifelineFd(dbus_fd)).WillOnce(Return(-1));
  ASSERT_FALSE(port_tracker_.AllowTcpPortAccess(tcp_port, interface, dbus_fd));
}

TEST_F(PortTrackerTest, AllowUdpPortAccess_EpollFailure) {
  SetMockExpectations(&mock_firewall_, true /* success */);
  // Make epoll(7) fail.
  EXPECT_CALL(port_tracker_, AddLifelineFd(dbus_fd)).WillOnce(Return(-1));
  ASSERT_FALSE(port_tracker_.AllowUdpPortAccess(udp_port, interface, dbus_fd));
}

TEST_F(PortTrackerTest, RevokeTcpPortAccess_Success) {
  SetMockExpectations(&mock_firewall_, true /* success */);
  EXPECT_CALL(port_tracker_, AddLifelineFd(dbus_fd))
      .WillOnce(Return(tracked_fd));
  ASSERT_TRUE(port_tracker_.AllowTcpPortAccess(tcp_port, interface, dbus_fd));

  EXPECT_CALL(port_tracker_, DeleteLifelineFd(tracked_fd))
      .WillOnce(Return(true));
  ASSERT_TRUE(port_tracker_.RevokeTcpPortAccess(tcp_port, interface));
}

TEST_F(PortTrackerTest, RevokeUdpPortAccess_Success) {
  SetMockExpectations(&mock_firewall_, true /* success */);
  EXPECT_CALL(port_tracker_, AddLifelineFd(dbus_fd))
      .WillOnce(Return(tracked_fd));
  ASSERT_TRUE(port_tracker_.AllowUdpPortAccess(udp_port, interface, dbus_fd));

  EXPECT_CALL(port_tracker_, DeleteLifelineFd(tracked_fd))
      .WillOnce(Return(true));
  ASSERT_TRUE(port_tracker_.RevokeUdpPortAccess(udp_port, interface));
}

TEST_F(PortTrackerTest, RevokeTcpPortAccess_FirewallFailure) {
  // Make plugging the firewall hole fail.
  mock_firewall_.SetRunInMinijailFailCriterion(
      std::vector<std::string>({"-D", "-p", "tcp"}), true /* repeat */,
      false /* omit_failure */);

  EXPECT_CALL(port_tracker_, AddLifelineFd(dbus_fd))
      .WillOnce(Return(tracked_fd));
  ASSERT_TRUE(port_tracker_.AllowTcpPortAccess(tcp_port, interface, dbus_fd));

  EXPECT_CALL(port_tracker_, DeleteLifelineFd(tracked_fd))
      .WillOnce(Return(true));
  ASSERT_FALSE(port_tracker_.RevokeTcpPortAccess(tcp_port, interface));
}

TEST_F(PortTrackerTest, RevokeUdpPortAccess_DbusFailure) {
  // Make plugging the firewall hole fail.
  mock_firewall_.SetRunInMinijailFailCriterion(
      std::vector<std::string>({"-D", "-p", "udp"}), true /* repeat */,
      false /* omit_failure */);

  EXPECT_CALL(port_tracker_, AddLifelineFd(dbus_fd))
      .WillOnce(Return(tracked_fd));
  ASSERT_TRUE(port_tracker_.AllowUdpPortAccess(udp_port, interface, dbus_fd));

  EXPECT_CALL(port_tracker_, DeleteLifelineFd(tracked_fd))
      .WillOnce(Return(true));
  ASSERT_FALSE(port_tracker_.RevokeUdpPortAccess(udp_port, interface));
}

TEST_F(PortTrackerTest, RevokeTcpPortAccess_EpollFailure) {
  SetMockExpectations(&mock_firewall_, true /* success */);

  EXPECT_CALL(port_tracker_, AddLifelineFd(dbus_fd))
      .WillOnce(Return(tracked_fd));
  ASSERT_TRUE(port_tracker_.AllowTcpPortAccess(tcp_port, interface, dbus_fd));

  // Make epoll(7) fail.
  EXPECT_CALL(port_tracker_, DeleteLifelineFd(tracked_fd))
      .WillOnce(Return(false));
  ASSERT_FALSE(port_tracker_.RevokeTcpPortAccess(tcp_port, interface));
}

TEST_F(PortTrackerTest, RevokeUdpPortAccess_EpollFailure) {
  SetMockExpectations(&mock_firewall_, true /* success */);

  EXPECT_CALL(port_tracker_, AddLifelineFd(dbus_fd))
      .WillOnce(Return(tracked_fd));
  ASSERT_TRUE(port_tracker_.AllowUdpPortAccess(udp_port, interface, dbus_fd));

  // Make epoll(7) fail.
  EXPECT_CALL(port_tracker_, DeleteLifelineFd(tracked_fd))
      .WillOnce(Return(false));
  ASSERT_FALSE(port_tracker_.RevokeUdpPortAccess(udp_port, interface));
}

TEST_F(PortTrackerTest, RequestVpnSetupSuccess) {
  const std::string interface = "tun0";
  const std::vector<std::string> usernames(1, "user");
  const int kInvalidHandle = -1;

  SetMockExpectations(&mock_firewall_, true /* success */);
  EXPECT_CALL(port_tracker_, AddLifelineFd(dbus_fd))
      .WillOnce(Return(tracked_fd));
  EXPECT_CALL(port_tracker_, DeleteLifelineFd(tracked_fd))
      .WillOnce(Return(true));

  ASSERT_EQ(port_tracker_.vpn_lifeline_, kInvalidHandle);
  ASSERT_TRUE(port_tracker_.PerformVpnSetup(usernames, interface, dbus_fd));
  ASSERT_EQ(port_tracker_.vpn_usernames_, usernames);
  ASSERT_EQ(port_tracker_.vpn_interface_, interface);
  ASSERT_NE(port_tracker_.vpn_lifeline_, kInvalidHandle);
  // Setup should fail when called after setup.
  ASSERT_FALSE(port_tracker_.PerformVpnSetup(usernames, interface, dbus_fd));

  ASSERT_TRUE(port_tracker_.RemoveVpnSetup());
  ASSERT_EQ(port_tracker_.vpn_usernames_.size(), 0);
  ASSERT_EQ(port_tracker_.vpn_interface_.size(), 0);
  ASSERT_EQ(port_tracker_.vpn_lifeline_, kInvalidHandle);
  // Cleanup should fail when called after cleanup.
  ASSERT_FALSE(port_tracker_.RemoveVpnSetup());
}

TEST_F(PortTrackerTest, RequestVpnSetupFailure) {
  const std::string interface = "tun0";
  const std::vector<std::string> usernames(1, "user");
  const int kInvalidHandle = -1;

  SetMockExpectations(&mock_firewall_, false /* success */);
  EXPECT_CALL(port_tracker_, AddLifelineFd(dbus_fd))
      .WillOnce(Return(tracked_fd));
  EXPECT_CALL(port_tracker_, DeleteLifelineFd(tracked_fd))
      .WillOnce(Return(true));

  ASSERT_EQ(port_tracker_.vpn_lifeline_, kInvalidHandle);
  ASSERT_FALSE(port_tracker_.PerformVpnSetup(usernames, interface, dbus_fd));
  ASSERT_EQ(port_tracker_.vpn_usernames_.size(), 0);
  ASSERT_EQ(port_tracker_.vpn_interface_.size(), 0);
  ASSERT_EQ(port_tracker_.vpn_lifeline_, kInvalidHandle);
}

}  // namespace permission_broker
