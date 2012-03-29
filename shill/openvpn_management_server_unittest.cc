// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/openvpn_management_server.h"

#include <netinet/in.h>

#include <chromeos/dbus/service_constants.h>
#include <gtest/gtest.h>

#include "shill/glib.h"
#include "shill/key_value_store.h"
#include "shill/mock_event_dispatcher.h"
#include "shill/mock_openvpn_driver.h"
#include "shill/mock_sockets.h"

using std::string;
using std::vector;
using testing::_;
using testing::Return;
using testing::ReturnNew;

namespace shill {

namespace {
MATCHER_P(CallbackEq, callback, "") {
  return arg.Equals(callback);
}

MATCHER_P(VoidStringEq, value, "") {
  return value == reinterpret_cast<const char *>(arg);
}
}  // namespace {}

class OpenVPNManagementServerTest : public testing::Test {
 public:
  OpenVPNManagementServerTest()
      : driver_(args_),
        server_(&driver_, &glib_) {}

  virtual ~OpenVPNManagementServerTest() {}

  void SetSockets() { server_.sockets_ = &sockets_; }
  void SetDispatcher() { server_.dispatcher_ = &dispatcher_; }
  void ExpectNotStarted() { EXPECT_TRUE(server_.sockets_ == NULL); }

  void SetConnectedSocket() {
    server_.connected_socket_ = kConnectedSocket;
    SetSockets();
  }

  void ExpectSend(const string &value) {
    EXPECT_CALL(sockets_,
                Send(kConnectedSocket, VoidStringEq(value), value.size(), 0))
        .WillOnce(Return(value.size()));
  }

  void ExpectStaticChallengeResponse() {
    driver_.args()->SetString(flimflam::kOpenVPNUserProperty, "jojo");
    driver_.args()->SetString(flimflam::kOpenVPNPasswordProperty, "yoyo");
    driver_.args()->SetString(flimflam::kOpenVPNOTPProperty, "123456");
    SetConnectedSocket();
    ExpectSend("username \"Auth\" jojo\n");
    ExpectSend("password \"Auth\" \"SCRV1:eW95bw==:MTIzNDU2\"\n");
  }

  InputData CreateInputDataFromString(const string &str) {
    InputData data(
        reinterpret_cast<unsigned char *>(const_cast<char *>(str.data())),
        str.size());
    return data;
  }

 protected:
  static const int kConnectedSocket;

  GLib glib_;
  KeyValueStore args_;
  MockOpenVPNDriver driver_;
  OpenVPNManagementServer server_;
  MockSockets sockets_;
  MockEventDispatcher dispatcher_;
};

// static
const int OpenVPNManagementServerTest::kConnectedSocket = 555;

TEST_F(OpenVPNManagementServerTest, StartStarted) {
  SetSockets();
  EXPECT_TRUE(server_.Start(NULL, NULL, NULL));
}

TEST_F(OpenVPNManagementServerTest, StartSocketFail) {
  EXPECT_CALL(sockets_, Socket(AF_INET, SOCK_STREAM, IPPROTO_TCP))
      .WillOnce(Return(-1));
  EXPECT_FALSE(server_.Start(NULL, &sockets_, NULL));
  ExpectNotStarted();
}

TEST_F(OpenVPNManagementServerTest, StartGetSockNameFail) {
  const int kSocket = 123;
  EXPECT_CALL(sockets_, Socket(AF_INET, SOCK_STREAM, IPPROTO_TCP))
      .WillOnce(Return(kSocket));
  EXPECT_CALL(sockets_, Bind(kSocket, _, _)).WillOnce(Return(0));
  EXPECT_CALL(sockets_, Listen(kSocket, 1)).WillOnce(Return(0));
  EXPECT_CALL(sockets_, GetSockName(kSocket, _, _)).WillOnce(Return(-1));
  EXPECT_CALL(sockets_, Close(kSocket)).WillOnce(Return(0));
  EXPECT_FALSE(server_.Start(NULL, &sockets_, NULL));
  ExpectNotStarted();
}

TEST_F(OpenVPNManagementServerTest, Start) {
  const int kSocket = 123;
  EXPECT_CALL(sockets_, Socket(AF_INET, SOCK_STREAM, IPPROTO_TCP))
      .WillOnce(Return(kSocket));
  EXPECT_CALL(sockets_, Bind(kSocket, _, _)).WillOnce(Return(0));
  EXPECT_CALL(sockets_, Listen(kSocket, 1)).WillOnce(Return(0));
  EXPECT_CALL(sockets_, GetSockName(kSocket, _, _)).WillOnce(Return(0));
  EXPECT_CALL(dispatcher_,
              CreateReadyHandler(kSocket, IOHandler::kModeInput,
                                 CallbackEq(server_.ready_callback_)))
      .WillOnce(ReturnNew<IOHandler>());
  vector<string> options;
  EXPECT_TRUE(server_.Start(&dispatcher_, &sockets_, &options));
  EXPECT_EQ(&sockets_, server_.sockets_);
  EXPECT_EQ(kSocket, server_.socket_);
  EXPECT_TRUE(server_.ready_handler_.get());
  EXPECT_EQ(&dispatcher_, server_.dispatcher_);
  EXPECT_FALSE(options.empty());
}

TEST_F(OpenVPNManagementServerTest, Stop) {
  SetSockets();
  server_.input_handler_.reset(new IOHandler());
  const int kConnectedSocket = 234;
  server_.connected_socket_ = kConnectedSocket;
  EXPECT_CALL(sockets_, Close(kConnectedSocket)).WillOnce(Return(0));
  SetDispatcher();
  server_.ready_handler_.reset(new IOHandler());
  const int kSocket = 345;
  server_.socket_ = kSocket;
  EXPECT_CALL(sockets_, Close(kSocket)).WillOnce(Return(0));
  server_.Stop();
  EXPECT_FALSE(server_.input_handler_.get());
  EXPECT_EQ(-1, server_.connected_socket_);
  EXPECT_FALSE(server_.dispatcher_);
  EXPECT_FALSE(server_.ready_handler_.get());
  EXPECT_EQ(-1, server_.socket_);
  ExpectNotStarted();
}

TEST_F(OpenVPNManagementServerTest, OnReadyAcceptFail) {
  const int kSocket = 333;
  SetSockets();
  EXPECT_CALL(sockets_, Accept(kSocket, NULL, NULL)).WillOnce(Return(-1));
  server_.OnReady(kSocket);
  EXPECT_EQ(-1, server_.connected_socket_);
}

TEST_F(OpenVPNManagementServerTest, OnReady) {
  const int kSocket = 111;
  SetConnectedSocket();
  SetDispatcher();
  EXPECT_CALL(sockets_, Accept(kSocket, NULL, NULL))
      .WillOnce(Return(kConnectedSocket));
  server_.ready_handler_.reset(new IOHandler());
  EXPECT_CALL(dispatcher_,
              CreateInputHandler(kConnectedSocket,
                                 CallbackEq(server_.input_callback_)))
      .WillOnce(ReturnNew<IOHandler>());
  ExpectSend("state on\n");
  server_.OnReady(kSocket);
  EXPECT_EQ(kConnectedSocket, server_.connected_socket_);
  EXPECT_FALSE(server_.ready_handler_.get());
  EXPECT_TRUE(server_.input_handler_.get());
}

TEST_F(OpenVPNManagementServerTest, OnInput) {
  {
    string s;
    InputData data = CreateInputDataFromString(s);
    server_.OnInput(&data);
  }
  {
    string s = "foo\n"
        ">INFO:...\n"
        ">PASSWORD:Need 'Auth' SC:user/password/otp\n"
        ">STATE:123,RECONNECTING,detail,...,...";
    InputData data = CreateInputDataFromString(s);
    ExpectStaticChallengeResponse();
    EXPECT_CALL(driver_, OnReconnecting());
    server_.OnInput(&data);
  }
}

TEST_F(OpenVPNManagementServerTest, ProcessMessage) {
  server_.ProcessMessage("foo");
  server_.ProcessMessage(">INFO:");

  EXPECT_CALL(driver_, OnReconnecting());
  server_.ProcessMessage(">STATE:123,RECONNECTING,detail,...,...");
}

TEST_F(OpenVPNManagementServerTest, ProcessInfoMessage) {
  EXPECT_FALSE(server_.ProcessInfoMessage("foo"));
  EXPECT_TRUE(server_.ProcessInfoMessage(">INFO:"));
}

TEST_F(OpenVPNManagementServerTest, ProcessStateMessage) {
  EXPECT_FALSE(server_.ProcessStateMessage("foo"));
  EXPECT_TRUE(server_.ProcessStateMessage(">STATE:123,WAIT,detail,...,..."));
  EXPECT_CALL(driver_, OnReconnecting());
  EXPECT_TRUE(
      server_.ProcessStateMessage(">STATE:123,RECONNECTING,detail,...,..."));
}

TEST_F(OpenVPNManagementServerTest, ProcessNeedPasswordMessageAuthSC) {
  EXPECT_FALSE(server_.ProcessNeedPasswordMessage("foo"));
  ExpectStaticChallengeResponse();
  EXPECT_TRUE(
      server_.ProcessNeedPasswordMessage(
          ">PASSWORD:Need 'Auth' SC:user/password/otp"));
  EXPECT_FALSE(driver_.args()->ContainsString(flimflam::kOpenVPNOTPProperty));
}

TEST_F(OpenVPNManagementServerTest, PerformStaticChallengeNoCreds) {
  // Expect no crash due to null sockets_.
  server_.PerformStaticChallenge();
  driver_.args()->SetString(flimflam::kOpenVPNUserProperty, "jojo");
  server_.PerformStaticChallenge();
  driver_.args()->SetString(flimflam::kOpenVPNPasswordProperty, "yoyo");
  server_.PerformStaticChallenge();
}

TEST_F(OpenVPNManagementServerTest, PerformStaticChallenge) {
  ExpectStaticChallengeResponse();
  server_.PerformStaticChallenge();
  EXPECT_FALSE(driver_.args()->ContainsString(flimflam::kOpenVPNOTPProperty));
}

TEST_F(OpenVPNManagementServerTest, Send) {
  const char kMessage[] = "foo\n";
  SetConnectedSocket();
  ExpectSend(kMessage);
  server_.Send(kMessage);
}

TEST_F(OpenVPNManagementServerTest, SendState) {
  SetConnectedSocket();
  ExpectSend("state off\n");
  server_.SendState("off");
}

TEST_F(OpenVPNManagementServerTest, SendUsername) {
  SetConnectedSocket();
  ExpectSend("username \"Auth\" joesmith\n");
  server_.SendUsername("Auth", "joesmith");
}

TEST_F(OpenVPNManagementServerTest, SendPassword) {
  SetConnectedSocket();
  ExpectSend("password \"Auth\" \"foobar\"\n");
  server_.SendPassword("Auth", "foobar");
}

}  // namespace shill
