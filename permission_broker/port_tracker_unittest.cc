// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "permission_broker/port_tracker.h"

#include <string>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "firewalld/dbus-mocks.h"

using ::testing::_;
using ::testing::Return;
using ::testing::SetArgumentPointee;

namespace permission_broker {

class MockPortTracker : public PortTracker {
 public:
  explicit MockPortTracker(org::chromium::FirewalldProxyInterface* firewalld)
      : PortTracker(nullptr, firewalld) {}
  ~MockPortTracker() override = default;

  MOCK_METHOD1(AddLifelineFd, int(int));
  MOCK_METHOD1(DeleteLifelineFd, bool(int));
  MOCK_METHOD1(CheckLifelineFds, void(bool));
  MOCK_METHOD0(ScheduleLifelineCheck, void());

  MOCK_METHOD0(InitializeEpollOnce, bool());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockPortTracker);
};

class PortTrackerTest : public testing::Test {
 public:
  PortTrackerTest() : port_tracker{&firewalld} {}
  ~PortTrackerTest() override = default;

 protected:
  org::chromium::FirewalldProxyMock firewalld;
  MockPortTracker port_tracker;

  uint16_t tcp_port = 8080;
  uint16_t udp_port = 5353;

  std::string interface = "interface";

  int dbus_fd = 3;  // First fd not std{in|out|err}. Doesn't get used at all.
  int tracked_fd = 4;  // Next "available" fd. Used only as a placeholder.

 private:
  DISALLOW_COPY_AND_ASSIGN(PortTrackerTest);
};

TEST_F(PortTrackerTest, ProcessTcpPortSuccess) {
  EXPECT_CALL(port_tracker, AddLifelineFd(dbus_fd)).WillOnce(Return(0));
  EXPECT_CALL(firewalld, PunchTcpHole(tcp_port, interface, _, _, _))
      .WillOnce(DoAll(SetArgumentPointee<2>(true), Return(true)));
  ASSERT_TRUE(port_tracker.ProcessTcpPort(tcp_port, interface, dbus_fd));
}

TEST_F(PortTrackerTest, ProcessUdpPortSuccess) {
  EXPECT_CALL(port_tracker, AddLifelineFd(dbus_fd)).WillOnce(Return(0));
  EXPECT_CALL(firewalld, PunchUdpHole(udp_port, interface, _, _, _))
      .WillOnce(DoAll(SetArgumentPointee<2>(true), Return(true)));
  ASSERT_TRUE(port_tracker.ProcessUdpPort(udp_port, interface, dbus_fd));
}

TEST_F(PortTrackerTest, ProcessTcpPortTwice) {
  EXPECT_CALL(port_tracker, AddLifelineFd(dbus_fd)).WillOnce(Return(0));
  EXPECT_CALL(firewalld, PunchTcpHole(tcp_port, interface, _, _, _))
      .WillOnce(DoAll(SetArgumentPointee<2>(true), Return(true)));
  ASSERT_TRUE(port_tracker.ProcessTcpPort(tcp_port, interface, dbus_fd));
  ASSERT_FALSE(port_tracker.ProcessTcpPort(tcp_port, interface, dbus_fd));
}

TEST_F(PortTrackerTest, ProcessUdpPortTwice) {
  EXPECT_CALL(port_tracker, AddLifelineFd(dbus_fd)).WillOnce(Return(0));
  EXPECT_CALL(firewalld, PunchUdpHole(udp_port, interface, _, _, _))
      .WillOnce(DoAll(SetArgumentPointee<2>(true), Return(true)));
  ASSERT_TRUE(port_tracker.ProcessUdpPort(udp_port, interface, dbus_fd));
  ASSERT_FALSE(port_tracker.ProcessUdpPort(udp_port, interface, dbus_fd));
}

TEST_F(PortTrackerTest, ProcessTcpPortDBusFailure) {
  EXPECT_CALL(port_tracker, AddLifelineFd(dbus_fd)).WillOnce(Return(0));
  // Make D-Bus fail.
  EXPECT_CALL(firewalld, PunchTcpHole(tcp_port, interface, _, _, _))
      .WillOnce(DoAll(SetArgumentPointee<2>(false), Return(false)));
  ASSERT_FALSE(port_tracker.ProcessTcpPort(tcp_port, interface, dbus_fd));
}

TEST_F(PortTrackerTest, ProcessUdpPortDBusFailure) {
  EXPECT_CALL(port_tracker, AddLifelineFd(dbus_fd)).WillOnce(Return(0));
  // Make D-Bus fail.
  EXPECT_CALL(firewalld, PunchUdpHole(udp_port, interface, _, _, _))
      .WillOnce(DoAll(SetArgumentPointee<2>(false), Return(false)));
  ASSERT_FALSE(port_tracker.ProcessUdpPort(udp_port, interface, dbus_fd));
}

TEST_F(PortTrackerTest, ProcessTcpPortEpollFailure) {
  // Make epoll(7) fail.
  EXPECT_CALL(port_tracker, AddLifelineFd(dbus_fd)).WillOnce(Return(-1));
  ON_CALL(firewalld, PunchTcpHole(tcp_port, _, _, _, _))
      .WillByDefault(DoAll(SetArgumentPointee<2>(true), Return(true)));
  ASSERT_FALSE(port_tracker.ProcessTcpPort(tcp_port, interface, dbus_fd));
}

TEST_F(PortTrackerTest, ProcessUdpPortEpollFailure) {
  // Make epoll(7) fail.
  EXPECT_CALL(port_tracker, AddLifelineFd(dbus_fd)).WillOnce(Return(-1));
  ON_CALL(firewalld, PunchUdpHole(udp_port, _, _, _, _))
      .WillByDefault(DoAll(SetArgumentPointee<2>(true), Return(true)));
  ASSERT_FALSE(port_tracker.ProcessUdpPort(udp_port, interface, dbus_fd));
}

TEST_F(PortTrackerTest, ReleaseTcpPortSuccess) {
  EXPECT_CALL(port_tracker, AddLifelineFd(dbus_fd))
      .WillOnce(Return(tracked_fd));
  EXPECT_CALL(firewalld, PunchTcpHole(tcp_port, interface, _, _, _))
      .WillOnce(DoAll(SetArgumentPointee<2>(true), Return(true)));
  ASSERT_TRUE(port_tracker.ProcessTcpPort(tcp_port, interface, dbus_fd));

  EXPECT_CALL(port_tracker, DeleteLifelineFd(tracked_fd))
      .WillOnce(Return(true));
  EXPECT_CALL(firewalld, PlugTcpHole(tcp_port, interface, _, _, _))
      .WillOnce(DoAll(SetArgumentPointee<2>(true), Return(true)));
  ASSERT_TRUE(port_tracker.ReleaseTcpPort(tcp_port, interface));
}

TEST_F(PortTrackerTest, ReleaseUdpPortSuccess) {
  EXPECT_CALL(port_tracker, AddLifelineFd(dbus_fd))
      .WillOnce(Return(tracked_fd));
  EXPECT_CALL(firewalld, PunchUdpHole(udp_port, interface, _, _, _))
      .WillOnce(DoAll(SetArgumentPointee<2>(true), Return(true)));
  ASSERT_TRUE(port_tracker.ProcessUdpPort(udp_port, interface, dbus_fd));

  EXPECT_CALL(port_tracker, DeleteLifelineFd(tracked_fd))
      .WillOnce(Return(true));
  EXPECT_CALL(firewalld, PlugUdpHole(udp_port, interface, _, _, _))
      .WillOnce(DoAll(SetArgumentPointee<2>(true), Return(true)));
  ASSERT_TRUE(port_tracker.ReleaseUdpPort(udp_port, interface));
}

TEST_F(PortTrackerTest, ReleaseTcpPortDbusFailure) {
  EXPECT_CALL(port_tracker, AddLifelineFd(dbus_fd))
      .WillOnce(Return(tracked_fd));
  EXPECT_CALL(firewalld, PunchTcpHole(tcp_port, interface, _, _, _))
      .WillOnce(DoAll(SetArgumentPointee<2>(true), Return(true)));
  ASSERT_TRUE(port_tracker.ProcessTcpPort(tcp_port, interface, dbus_fd));

  EXPECT_CALL(port_tracker, DeleteLifelineFd(tracked_fd))
      .WillOnce(Return(true));
  // Make D-Bus fail.
  EXPECT_CALL(firewalld, PlugTcpHole(tcp_port, interface, _, _, _))
      .WillOnce(DoAll(SetArgumentPointee<2>(false), Return(false)));
  ASSERT_FALSE(port_tracker.ReleaseTcpPort(tcp_port, interface));
}

TEST_F(PortTrackerTest, ReleaseUdpPortDbusFailure) {
  EXPECT_CALL(port_tracker, AddLifelineFd(dbus_fd))
      .WillOnce(Return(tracked_fd));
  EXPECT_CALL(firewalld, PunchUdpHole(udp_port, interface, _, _, _))
      .WillOnce(DoAll(SetArgumentPointee<2>(true), Return(true)));
  ASSERT_TRUE(port_tracker.ProcessUdpPort(udp_port, interface, dbus_fd));

  EXPECT_CALL(port_tracker, DeleteLifelineFd(tracked_fd))
      .WillOnce(Return(true));
  // Make D-Bus fail.
  EXPECT_CALL(firewalld, PlugUdpHole(udp_port, interface, _, _, _))
      .WillOnce(DoAll(SetArgumentPointee<2>(false), Return(false)));
  ASSERT_FALSE(port_tracker.ReleaseUdpPort(udp_port, interface));
}

TEST_F(PortTrackerTest, ReleaseTcpPortEpollFailure) {
  EXPECT_CALL(port_tracker, AddLifelineFd(dbus_fd))
      .WillOnce(Return(tracked_fd));
  EXPECT_CALL(firewalld, PunchTcpHole(tcp_port, interface, _, _, _))
      .WillOnce(DoAll(SetArgumentPointee<2>(true), Return(true)));
  ASSERT_TRUE(port_tracker.ProcessTcpPort(tcp_port, interface, dbus_fd));

  // Make epoll(7) fail.
  EXPECT_CALL(port_tracker, DeleteLifelineFd(tracked_fd))
      .WillOnce(Return(false));
  EXPECT_CALL(firewalld, PlugTcpHole(tcp_port, interface, _, _, _))
      .WillOnce(DoAll(SetArgumentPointee<2>(true), Return(true)));
  ASSERT_FALSE(port_tracker.ReleaseTcpPort(tcp_port, interface));
}

TEST_F(PortTrackerTest, ReleaseUdpPortEpollFailure) {
  EXPECT_CALL(port_tracker, AddLifelineFd(dbus_fd))
      .WillOnce(Return(tracked_fd));
  EXPECT_CALL(firewalld, PunchUdpHole(udp_port, interface, _, _, _))
      .WillOnce(DoAll(SetArgumentPointee<2>(true), Return(true)));
  ASSERT_TRUE(port_tracker.ProcessUdpPort(udp_port, interface, dbus_fd));

  // Make epoll(7) fail.
  EXPECT_CALL(port_tracker, DeleteLifelineFd(tracked_fd))
      .WillOnce(Return(false));
  EXPECT_CALL(firewalld, PlugUdpHole(udp_port, interface, _, _, _))
      .WillOnce(DoAll(SetArgumentPointee<2>(true), Return(true)));
  ASSERT_FALSE(port_tracker.ReleaseUdpPort(udp_port, interface));
}

}  // namespace permission_broker
