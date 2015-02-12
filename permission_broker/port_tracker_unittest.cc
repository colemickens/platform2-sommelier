// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "permission_broker/port_tracker.h"

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
  MOCK_METHOD0(CheckLifelineFds, void(void));
  MOCK_METHOD0(ScheduleLifelineCheck, void(void));

  MOCK_METHOD0(InitializeEpollOnce, bool(void));

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

  int dbus_fd = 3;  // First fd not std{in|out|err}. Doesn't get used at all.

 private:
  DISALLOW_COPY_AND_ASSIGN(PortTrackerTest);
};

TEST_F(PortTrackerTest, ProcessTcpPortSuccess) {
  EXPECT_CALL(port_tracker, AddLifelineFd(dbus_fd)).WillOnce(Return(0));
  EXPECT_CALL(firewalld, PunchTcpHole(tcp_port, _, _, _, _))
      .WillOnce(DoAll(SetArgumentPointee<2>(true), Return(true)));
  ASSERT_TRUE(port_tracker.ProcessTcpPort(tcp_port, dbus_fd));
}

TEST_F(PortTrackerTest, ProcessUdpPortSuccess) {
  EXPECT_CALL(port_tracker, AddLifelineFd(dbus_fd)).WillOnce(Return(0));
  EXPECT_CALL(firewalld, PunchUdpHole(udp_port, _, _, _, _))
      .WillOnce(DoAll(SetArgumentPointee<2>(true), Return(true)));
  ASSERT_TRUE(port_tracker.ProcessUdpPort(udp_port, dbus_fd));
}

TEST_F(PortTrackerTest, ProcessTcpPortDBusFailure) {
  EXPECT_CALL(port_tracker, AddLifelineFd(dbus_fd)).WillOnce(Return(0));
  // Make D-Bus fail.
  EXPECT_CALL(firewalld, PunchTcpHole(tcp_port, _, _, _, _))
      .WillOnce(DoAll(SetArgumentPointee<2>(false), Return(false)));
  ASSERT_FALSE(port_tracker.ProcessTcpPort(tcp_port, dbus_fd));
}

TEST_F(PortTrackerTest, ProcessUdpPortDBusFailure) {
  EXPECT_CALL(port_tracker, AddLifelineFd(dbus_fd)).WillOnce(Return(0));
  // Make D-Bus fail.
  EXPECT_CALL(firewalld, PunchUdpHole(udp_port, _, _, _, _))
      .WillOnce(DoAll(SetArgumentPointee<2>(false), Return(false)));
  ASSERT_FALSE(port_tracker.ProcessUdpPort(udp_port, dbus_fd));
}

TEST_F(PortTrackerTest, ProcessTcpPortEpollFailure) {
  // Make epoll(7) fail.
  EXPECT_CALL(port_tracker, AddLifelineFd(dbus_fd)).WillOnce(Return(-1));
  ON_CALL(firewalld, PunchTcpHole(tcp_port, _, _, _, _))
      .WillByDefault(DoAll(SetArgumentPointee<2>(true), Return(true)));
  ASSERT_FALSE(port_tracker.ProcessTcpPort(tcp_port, dbus_fd));
}

TEST_F(PortTrackerTest, ProcessUdpPortEpollFailure) {
  // Make epoll(7) fail.
  EXPECT_CALL(port_tracker, AddLifelineFd(dbus_fd)).WillOnce(Return(-1));
  ON_CALL(firewalld, PunchUdpHole(udp_port, _, _, _, _))
      .WillByDefault(DoAll(SetArgumentPointee<2>(true), Return(true)));
  ASSERT_FALSE(port_tracker.ProcessUdpPort(udp_port, dbus_fd));
}

}  // namespace permission_broker
