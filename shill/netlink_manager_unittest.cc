// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file provides tests for individual messages.  It tests
// NetlinkMessageFactory's ability to create specific message types and it
// tests the various NetlinkMessage types' ability to parse those
// messages.

// This file tests the public interface to NetlinkManager.
#include "shill/netlink_manager.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "shill/mock_netlink_socket.h"
#include "shill/nl80211_message.h"

using base::Bind;
using base::Unretained;
using testing::_;
using testing::Invoke;
using testing::Return;
using testing::Test;

namespace shill {

namespace {

// These data blocks have been collected by shill using NetlinkManager while,
// simultaneously (and manually) comparing shill output with that of the 'iw'
// code from which it was derived.  The test strings represent the raw packet
// data coming from the kernel.  The comments above each of these strings is
// the markup that "iw" outputs for ech of these packets.

// These constants are consistent throughout the packets, below.

const uint16_t kNl80211FamilyId = 0x13;

// wlan0 (phy #0): disconnected (by AP) reason: 2: Previous authentication no
// longer valid

const unsigned char kNL80211_CMD_DISCONNECT[] = {
  0x30, 0x00, 0x00, 0x00, 0x13, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x30, 0x01, 0x00, 0x00, 0x08, 0x00, 0x01, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x03, 0x00,
  0x04, 0x00, 0x00, 0x00, 0x06, 0x00, 0x36, 0x00,
  0x02, 0x00, 0x00, 0x00, 0x04, 0x00, 0x47, 0x00,
};

const char kGetFamilyCommandString[] = "CTRL_CMD_GETFAMILY";

}  // namespace

bool MockNetlinkSocket::SendMessage(const ByteString &out_string) {
  return true;
}

class NetlinkManagerTest : public Test {
 public:
  NetlinkManagerTest() : netlink_manager_(NetlinkManager::GetInstance()) {
    netlink_manager_->message_types_[Nl80211Message::kMessageTypeString]
        .family_id = kNl80211FamilyId;
    netlink_manager_->message_factory_.AddFactoryMethod(
        kNl80211FamilyId, Bind(&Nl80211Message::CreateMessage));
    Nl80211Message::SetMessageType(kNl80211FamilyId);
  }

  ~NetlinkManagerTest() {
    // NetlinkManager is a singleton, the sock_ field *MUST* be cleared
    // before "NetlinkManagerTest::socket_" gets invalidated, otherwise
    // later tests will refer to a corrupted memory.
    netlink_manager_->sock_ = NULL;
  }

  void SetupNetlinkManagerObject() {
    EXPECT_NE(reinterpret_cast<NetlinkManager *>(NULL), netlink_manager_);
    netlink_manager_->sock_ = &socket_;
    EXPECT_TRUE(netlink_manager_->Init());
  }

 protected:
  class MockHandler80211 {
   public:
    MockHandler80211() :
      on_netlink_message_(base::Bind(&MockHandler80211::OnNetlinkMessage,
                                     base::Unretained(this))) {}
    MOCK_METHOD1(OnNetlinkMessage, void(const NetlinkMessage &msg));
    const NetlinkManager::NetlinkMessageHandler &on_netlink_message() const {
      return on_netlink_message_;
    }
   private:
    NetlinkManager::NetlinkMessageHandler on_netlink_message_;
    DISALLOW_COPY_AND_ASSIGN(MockHandler80211);
  };

  NetlinkManager *netlink_manager_;
  MockNetlinkSocket socket_;
};

// TODO(wdg): Add a test for multi-part messages.  crbug.com/224652
// TODO(wdg): Add a test for GetFaimily.  crbug.com/224649
// TODO(wdg): Add a test for OnNewFamilyMessage.  crbug.com/222486
// TODO(wdg): Add a test for SubscribeToEvents (verify that it handles bad input
// appropriately, and that it calls NetlinkSocket::SubscribeToEvents if input
// is good.)

TEST_F(NetlinkManagerTest, BroadcastHandlerTest) {
  SetupNetlinkManagerObject();

  nlmsghdr *message = const_cast<nlmsghdr *>(
        reinterpret_cast<const nlmsghdr *>(kNL80211_CMD_DISCONNECT));

  MockHandler80211 handler1;
  MockHandler80211 handler2;

  // Simple, 1 handler, case.
  EXPECT_CALL(handler1, OnNetlinkMessage(_)).Times(1);
  EXPECT_FALSE(
      netlink_manager_->FindBroadcastHandler(handler1.on_netlink_message()));
  netlink_manager_->AddBroadcastHandler(handler1.on_netlink_message());
  EXPECT_TRUE(
      netlink_manager_->FindBroadcastHandler(handler1.on_netlink_message()));
  netlink_manager_->OnNlMessageReceived(message);

  // Add a second handler.
  EXPECT_CALL(handler1, OnNetlinkMessage(_)).Times(1);
  EXPECT_CALL(handler2, OnNetlinkMessage(_)).Times(1);
  netlink_manager_->AddBroadcastHandler(handler2.on_netlink_message());
  netlink_manager_->OnNlMessageReceived(message);

  // Verify that a handler can't be added twice.
  EXPECT_CALL(handler1, OnNetlinkMessage(_)).Times(1);
  EXPECT_CALL(handler2, OnNetlinkMessage(_)).Times(1);
  netlink_manager_->AddBroadcastHandler(handler1.on_netlink_message());
  netlink_manager_->OnNlMessageReceived(message);

  // Check that we can remove a handler.
  EXPECT_CALL(handler1, OnNetlinkMessage(_)).Times(0);
  EXPECT_CALL(handler2, OnNetlinkMessage(_)).Times(1);
  EXPECT_TRUE(netlink_manager_->RemoveBroadcastHandler(
      handler1.on_netlink_message()));
  netlink_manager_->OnNlMessageReceived(message);

  // Check that re-adding the handler goes smoothly.
  EXPECT_CALL(handler1, OnNetlinkMessage(_)).Times(1);
  EXPECT_CALL(handler2, OnNetlinkMessage(_)).Times(1);
  netlink_manager_->AddBroadcastHandler(handler1.on_netlink_message());
  netlink_manager_->OnNlMessageReceived(message);

  // Check that ClearBroadcastHandlers works.
  netlink_manager_->ClearBroadcastHandlers();
  EXPECT_CALL(handler1, OnNetlinkMessage(_)).Times(0);
  EXPECT_CALL(handler2, OnNetlinkMessage(_)).Times(0);
  netlink_manager_->OnNlMessageReceived(message);
}

TEST_F(NetlinkManagerTest, MessageHandlerTest) {
  // Setup.
  SetupNetlinkManagerObject();

  MockHandler80211 handler_broadcast;
  EXPECT_TRUE(netlink_manager_->AddBroadcastHandler(
      handler_broadcast.on_netlink_message()));

  Nl80211Message sent_message_1(CTRL_CMD_GETFAMILY, kGetFamilyCommandString);
  MockHandler80211 handler_sent_1;

  Nl80211Message sent_message_2(CTRL_CMD_GETFAMILY, kGetFamilyCommandString);
  MockHandler80211 handler_sent_2;

  // Set up the received message as a response to sent_message_1.
  scoped_array<unsigned char> message_memory(
      new unsigned char[sizeof(kNL80211_CMD_DISCONNECT)]);
  memcpy(message_memory.get(), kNL80211_CMD_DISCONNECT,
         sizeof(kNL80211_CMD_DISCONNECT));
  nlmsghdr *received_message =
        reinterpret_cast<nlmsghdr *>(message_memory.get());

  // Now, we can start the actual test...

  // Verify that generic handler gets called for a message when no
  // message-specific handler has been installed.
  EXPECT_CALL(handler_broadcast, OnNetlinkMessage(_)).Times(1);
  netlink_manager_->OnNlMessageReceived(received_message);

  // Send the message and give our handler.  Verify that we get called back.
  EXPECT_TRUE(netlink_manager_->SendMessage(
      &sent_message_1, handler_sent_1.on_netlink_message()));
  // Make it appear that this message is in response to our sent message.
  received_message->nlmsg_seq = socket_.GetLastSequenceNumber();
  EXPECT_CALL(handler_sent_1, OnNetlinkMessage(_)).Times(1);
  netlink_manager_->OnNlMessageReceived(received_message);

  // Verify that broadcast handler is called for the message after the
  // message-specific handler is called once.
  EXPECT_CALL(handler_broadcast, OnNetlinkMessage(_)).Times(1);
  netlink_manager_->OnNlMessageReceived(received_message);

  // Install and then uninstall message-specific handler; verify broadcast
  // handler is called on message receipt.
  EXPECT_TRUE(netlink_manager_->SendMessage(
      &sent_message_1, handler_sent_1.on_netlink_message()));
  received_message->nlmsg_seq = socket_.GetLastSequenceNumber();
  EXPECT_TRUE(netlink_manager_->RemoveMessageHandler(sent_message_1));
  EXPECT_CALL(handler_broadcast, OnNetlinkMessage(_)).Times(1);
  netlink_manager_->OnNlMessageReceived(received_message);

  // Install handler for different message; verify that broadcast handler is
  // called for _this_ message.
  EXPECT_TRUE(netlink_manager_->SendMessage(
      &sent_message_2, handler_sent_2.on_netlink_message()));
  EXPECT_CALL(handler_broadcast, OnNetlinkMessage(_)).Times(1);
  netlink_manager_->OnNlMessageReceived(received_message);

  // Change the ID for the message to that of the second handler; verify that
  // the appropriate handler is called for _that_ message.
  received_message->nlmsg_seq = socket_.GetLastSequenceNumber();
  EXPECT_CALL(handler_sent_2, OnNetlinkMessage(_)).Times(1);
  netlink_manager_->OnNlMessageReceived(received_message);
}

}  // namespace shill
