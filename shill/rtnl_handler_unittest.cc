// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include <gtest/gtest.h>
#include <net/if.h>
#include <sys/ioctl.h>

#include "shill/mock_sockets.h"
#include "shill/rtnl_handler.h"

using std::string;
using testing::_;
using testing::DoAll;
using testing::Return;
using testing::StrictMock;
using testing::Test;

namespace shill {

namespace {

const int kTestInterfaceIndex = 4;

ACTION(SetInterfaceIndex) {
  if (arg2) {
    reinterpret_cast<struct ifreq *>(arg2)->ifr_ifindex = kTestInterfaceIndex;
  }
}

}  // namespace

class RTNLHandlerTest : public Test {
 protected:
  void SetSockets(Sockets *sockets) {
    RTNLHandler::GetInstance()->sockets_ = sockets;
  }

  virtual void SetUp() {
    SetSockets(&sockets_);
  }

  virtual void TearDown() {
    SetSockets(NULL);
  }

  StrictMock<MockSockets> sockets_;
};

TEST_F(RTNLHandlerTest, GetInterfaceName) {
  EXPECT_EQ(-1, RTNLHandler::GetInstance()->GetInterfaceIndex(""));
  {
    struct ifreq ifr;
    string name(sizeof(ifr.ifr_name), 'x');
    EXPECT_EQ(-1, RTNLHandler::GetInstance()->GetInterfaceIndex(name));
  }

  const int kTestSocket = 123;
  EXPECT_CALL(sockets_, Socket(PF_INET, SOCK_DGRAM, 0))
      .Times(3)
      .WillOnce(Return(-1))
      .WillRepeatedly(Return(kTestSocket));
  EXPECT_CALL(sockets_, Ioctl(kTestSocket, SIOCGIFINDEX, _))
      .WillOnce(Return(-1))
      .WillOnce(DoAll(SetInterfaceIndex(), Return(0)));
  EXPECT_CALL(sockets_, Close(kTestSocket))
      .Times(2)
      .WillRepeatedly(Return(0));
  EXPECT_EQ(-1, RTNLHandler::GetInstance()->GetInterfaceIndex("eth0"));
  EXPECT_EQ(-1, RTNLHandler::GetInstance()->GetInterfaceIndex("wlan0"));
  EXPECT_EQ(kTestInterfaceIndex,
            RTNLHandler::GetInstance()->GetInterfaceIndex("usb0"));
}

}  // namespace shill
