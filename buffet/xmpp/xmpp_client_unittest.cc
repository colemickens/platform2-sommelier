// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "buffet/device_registration_info.h"

#include <base/values.h>
#include <chromeos/key_value_store.h>
#include <chromeos/http/curl_api.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "buffet/xmpp/xmpp_client.h"
#include "buffet/xmpp/xmpp_connection.h"

using ::testing::DoAll;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::_;

namespace buffet {

namespace {

constexpr char kAccountName[] = "Account@Name";
constexpr char kAccessToken[] = "AccessToken";

constexpr char kStartStreamResponse[] =
    "<stream:stream from=\"clouddevices.gserviceaccount.com\" "
    "id=\"0CCF520913ABA04B\" version=\"1.0\" "
    "xmlns:stream=\"http://etherx.jabber.org/streams\" "
    "xmlns=\"jabber:client\">"
    "<stream:features><starttls xmlns=\"urn:ietf:params:xml:ns:xmpp-tls\">"
    "<required/></starttls><mechanisms "
    "xmlns=\"urn:ietf:params:xml:ns:xmpp-sasl\"><mechanism>X-OAUTH2</mechanism>"
    "<mechanism>X-GOOGLE-TOKEN</mechanism></mechanisms></stream:features>";
constexpr char kAuthenticationSucceededResponse[] =
    "<success xmlns=\"urn:ietf:params:xml:ns:xmpp-sasl\"/>";
constexpr char kAuthenticationFailedResponse[] =
    "<failure xmlns=\"urn:ietf:params:xml:ns:xmpp-sasl\"><not-authorized/>"
    "</failure></stream:stream>";
constexpr char kRestartStreamResponse[] =
    "<stream:stream from=\"clouddevices.gserviceaccount.com\" "
    "id=\"BE7D34E0B7589E2A\" version=\"1.0\" "
    "xmlns:stream=\"http://etherx.jabber.org/streams\" "
    "xmlns=\"jabber:client\">"
    "<stream:features><bind xmlns=\"urn:ietf:params:xml:ns:xmpp-bind\"/>"
    "<session xmlns=\"urn:ietf:params:xml:ns:xmpp-session\"/>"
    "</stream:features>";
constexpr char kBindResponse[] =
    "<iq id=\"0\" type=\"result\">"
    "<bind xmlns=\"urn:ietf:params:xml:ns:xmpp-bind\">"
    "<jid>110cc78f78d7032cc7bf2c6e14c1fa7d@clouddevices.gserviceaccount.com"
    "/19853128</jid></bind></iq>";
constexpr char kSessionResponse[] =
    "<iq type=\"result\" id=\"1\"/>";
constexpr char kSubscribedResponse[] =
    "<iq to=\""
    "110cc78f78d7032cc7bf2c6e14c1fa7d@clouddevices.gserviceaccount.com/"
    "19853128\" from=\""
    "110cc78f78d7032cc7bf2c6e14c1fa7d@clouddevices.gserviceaccount.com\" "
    "id=\"pushsubscribe1\" type=\"result\"/>";

constexpr char kStartStreamMessage[] =
    "<stream:stream to='clouddevices.gserviceaccount.com' "
    "xmlns:stream='http://etherx.jabber.org/streams' xml:lang='*' "
    "version='1.0' xmlns='jabber:client'>";
constexpr char kAuthenticationMessage[] =
    "<auth xmlns='urn:ietf:params:xml:ns:xmpp-sasl' mechanism='X-OAUTH2' "
    "auth:service='oauth2' auth:allow-non-google-login='true' "
    "auth:client-uses-full-bind-result='true' "
    "xmlns:auth='http://www.google.com/talk/protocol/auth'>"
    "AEFjY291bnRATmFtZQBBY2Nlc3NUb2tlbg==</auth>";
constexpr char kBindMessage[] =
    "<iq type='set' id='0'><bind "
    "xmlns='urn:ietf:params:xml:ns:xmpp-bind'/></iq>";
constexpr char kSessionMessage[] =
    "<iq type='set' id='1'><session "
    "xmlns='urn:ietf:params:xml:ns:xmpp-session'/></iq>";
constexpr char kSubscribeMessage[] =
    "<iq type='set' to='Account@Name' id='pushsubscribe1'>"
    "<subscribe xmlns='google:push'><item channel='cloud_devices' from=''/>"
    "</subscribe></iq>";

class MockXmppConnection : public XmppConnection {
 public:
  MockXmppConnection() = default;

  MOCK_CONST_METHOD1(Read, bool(std::string* msg));
  MOCK_CONST_METHOD1(Write, bool(const std::string& msg));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockXmppConnection);
};

}  // namespace

class TestHelper {
 public:
  static void SetState(XmppClient* client, XmppClient::XmppState state) {
    client->state_ = state;
  }

  static XmppClient::XmppState GetState(const XmppClient& client) {
    return client.state_;
  }

  static XmppConnection* GetConnection(const XmppClient& client) {
    return client.connection_.get();
  }
};

class XmppClientTest : public ::testing::Test {
 protected:
  void SetUp() override {
    std::unique_ptr<XmppConnection> connection(new MockXmppConnection());
    xmpp_client_.reset(new XmppClient(kAccountName, kAccessToken,
                                      std::move(connection)));
    connection_ = static_cast<MockXmppConnection*>(
        TestHelper::GetConnection(*xmpp_client_));
  }

  std::unique_ptr<XmppClient> xmpp_client_;
  MockXmppConnection* connection_;  // xmpp_client_ owns this.
};

TEST_F(XmppClientTest, StartStream) {
  EXPECT_EQ(TestHelper::GetState(*xmpp_client_),
            XmppClient::XmppState::kNotStarted);
  EXPECT_CALL(*connection_, Write(kStartStreamMessage))
      .WillOnce(Return(true));
  xmpp_client_->StartStream();
  EXPECT_EQ(TestHelper::GetState(*xmpp_client_),
            XmppClient::XmppState::kStarted);
}

TEST_F(XmppClientTest, HandleStartedResponse) {
  TestHelper::SetState(xmpp_client_.get(),
                       XmppClient::XmppState::kStarted);
  EXPECT_CALL(*connection_, Read(_))
      .WillOnce(DoAll(SetArgPointee<0>(kStartStreamResponse), Return(true)));
  EXPECT_CALL(*connection_, Write(kAuthenticationMessage))
      .WillOnce(Return(true));
  xmpp_client_->Read();
  EXPECT_EQ(TestHelper::GetState(*xmpp_client_),
            XmppClient::XmppState::kAuthenticationStarted);
}

TEST_F(XmppClientTest, HandleAuthenticationSucceededResponse) {
  TestHelper::SetState(
      xmpp_client_.get(),
      XmppClient::XmppState::kAuthenticationStarted);
  EXPECT_CALL(*connection_, Read(_))
      .WillOnce(DoAll(SetArgPointee<0>(kAuthenticationSucceededResponse),
                      Return(true)));
  EXPECT_CALL(*connection_, Write(kStartStreamMessage))
      .WillOnce(Return(true));
  xmpp_client_->Read();
  EXPECT_EQ(TestHelper::GetState(*xmpp_client_),
            XmppClient::XmppState::kStreamRestartedPostAuthentication);
}

TEST_F(XmppClientTest, HandleAuthenticationFailedResponse) {
  TestHelper::SetState(
      xmpp_client_.get(),
      XmppClient::XmppState::kAuthenticationStarted);
  EXPECT_CALL(*connection_, Read(_))
      .WillOnce(DoAll(SetArgPointee<0>(kAuthenticationFailedResponse),
                      Return(true)));
  EXPECT_CALL(*connection_, Write(_))
      .Times(0);
  xmpp_client_->Read();
  EXPECT_EQ(TestHelper::GetState(*xmpp_client_),
            XmppClient::XmppState::kAuthenticationFailed);
}

TEST_F(XmppClientTest, HandleStreamRestartedResponse) {
  TestHelper::SetState(
      xmpp_client_.get(),
      XmppClient::XmppState::kStreamRestartedPostAuthentication);
  EXPECT_CALL(*connection_, Read(_))
      .WillOnce(DoAll(SetArgPointee<0>(kRestartStreamResponse), Return(true)));
  EXPECT_CALL(*connection_, Write(kBindMessage))
      .WillOnce(Return(true));
  xmpp_client_->Read();
  EXPECT_EQ(TestHelper::GetState(*xmpp_client_),
            XmppClient::XmppState::kBindSent);
}

TEST_F(XmppClientTest, HandleBindResponse) {
  TestHelper::SetState(xmpp_client_.get(),
                       XmppClient::XmppState::kBindSent);
  EXPECT_CALL(*connection_, Read(_))
      .WillOnce(DoAll(SetArgPointee<0>(kBindResponse), Return(true)));
  EXPECT_CALL(*connection_, Write(kSessionMessage))
      .WillOnce(Return(true));
  xmpp_client_->Read();
  EXPECT_EQ(TestHelper::GetState(*xmpp_client_),
            XmppClient::XmppState::kSessionStarted);
}

TEST_F(XmppClientTest, HandleSessionResponse) {
  TestHelper::SetState(xmpp_client_.get(),
                       XmppClient::XmppState::kSessionStarted);
  EXPECT_CALL(*connection_, Read(_))
      .WillOnce(DoAll(SetArgPointee<0>(kSessionResponse), Return(true)));
  EXPECT_CALL(*connection_, Write(kSubscribeMessage))
      .WillOnce(Return(true));
  xmpp_client_->Read();
  EXPECT_EQ(TestHelper::GetState(*xmpp_client_),
            XmppClient::XmppState::kSubscribeStarted);
}

TEST_F(XmppClientTest, HandleSubscribeResponse) {
  TestHelper::SetState(xmpp_client_.get(),
                       XmppClient::XmppState::kSubscribeStarted);
  EXPECT_CALL(*connection_, Read(_))
      .WillOnce(DoAll(SetArgPointee<0>(kSubscribedResponse), Return(true)));
  EXPECT_CALL(*connection_, Write(_))
      .Times(0);
  xmpp_client_->Read();
  EXPECT_EQ(TestHelper::GetState(*xmpp_client_),
            XmppClient::XmppState::kSubscribed);
}

}  // namespace buffet
