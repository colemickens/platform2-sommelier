// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include <glib.h>
#include <gtest/gtest.h>
#include <net/if.h>
#include <sys/socket.h>
#include <linux/netlink.h>  // Needs typedefs from sys/socket.h.
#include <linux/rtnetlink.h>
#include <sys/ioctl.h>

#include <base/bind.h>

#include "shill/manager.h"
#include "shill/mock_control.h"
#include "shill/mock_glib.h"
#include "shill/mock_manager.h"
#include "shill/mock_metrics.h"
#include "shill/mock_sockets.h"
#include "shill/rtnl_handler.h"
#include "shill/rtnl_message.h"

using base::Bind;
using base::Callback;
using base::Unretained;
using std::string;
using testing::_;
using testing::A;
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

class TestEventDispatcher : public EventDispatcher {
 public:
  virtual IOHandler *CreateInputHandler(
      int /*fd*/,
      const Callback<void(InputData*)> &/*callback*/) {
    return NULL;
  }
};

}  // namespace

class RTNLHandlerTest : public Test {
 public:
  RTNLHandlerTest()
      : manager_(&control_interface_, &dispatcher_, &metrics_, &glib_),
        callback_(Bind(&RTNLHandlerTest::HandlerCallback, Unretained(this))) {
  }

  virtual void TearDown() {
    RTNLHandler::GetInstance()->Stop();
    SetSockets(NULL);
  }

  MOCK_METHOD1(HandlerCallback, void(const RTNLMessage &));

 protected:
  static const int kTestSocket;
  static const int kTestDeviceIndex;
  static const char kTestDeviceName[];

  void AddLink();
  void StartRTNLHandler();
  void StopRTNLHandler();

  void SetSockets(Sockets *sockets) {
    RTNLHandler::GetInstance()->sockets_ = sockets;
  }

  StrictMock<MockSockets> sockets_;
  MockGLib glib_;
  MockControl control_interface_;
  MockMetrics metrics_;
  MockManager manager_;
  TestEventDispatcher dispatcher_;
  Callback<void(const RTNLMessage &)> callback_;
};

const int RTNLHandlerTest::kTestSocket = 123;
const int RTNLHandlerTest::kTestDeviceIndex = 123456;
const char RTNLHandlerTest::kTestDeviceName[] = "test-device";

void RTNLHandlerTest::StartRTNLHandler() {
  EXPECT_CALL(sockets_, Socket(PF_NETLINK, SOCK_DGRAM, NETLINK_ROUTE))
      .WillOnce(Return(kTestSocket));
  EXPECT_CALL(sockets_, Bind(kTestSocket, _, sizeof(sockaddr_nl)))
      .WillOnce(Return(0));
  RTNLHandler::GetInstance()->Start(&dispatcher_, &sockets_);
}

void RTNLHandlerTest::StopRTNLHandler() {
  EXPECT_CALL(sockets_, Close(kTestSocket)).WillOnce(Return(0));
  RTNLHandler::GetInstance()->Stop();
}

void RTNLHandlerTest::AddLink() {
  RTNLMessage message(RTNLMessage::kTypeLink,
                      RTNLMessage::kModeAdd,
                      0,
                      0,
                      0,
                      kTestDeviceIndex,
                      IPAddress::kFamilyIPv4);
  message.SetAttribute(static_cast<uint16>(IFLA_IFNAME),
                       ByteString(string(kTestDeviceName), true));
  ByteString b(message.Encode());
  InputData data(b.GetData(), b.GetLength());
  RTNLHandler::GetInstance()->ParseRTNL(&data);
}

TEST_F(RTNLHandlerTest, AddLinkTest) {
  StartRTNLHandler();
  scoped_ptr<RTNLListener> link_listener(
      new RTNLListener(RTNLHandler::kRequestLink, callback_));

  EXPECT_CALL(*this, HandlerCallback(A<const RTNLMessage &>())).Times(1);

  AddLink();

  StopRTNLHandler();
}


TEST_F(RTNLHandlerTest, GetInterfaceName) {
  SetSockets(&sockets_);
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
      .Times(3)
      .WillRepeatedly(Return(0));
  EXPECT_EQ(-1, RTNLHandler::GetInstance()->GetInterfaceIndex("eth0"));
  EXPECT_EQ(-1, RTNLHandler::GetInstance()->GetInterfaceIndex("wlan0"));
  EXPECT_EQ(kTestInterfaceIndex,
            RTNLHandler::GetInstance()->GetInterfaceIndex("usb0"));
}

}  // namespace shill
