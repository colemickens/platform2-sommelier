// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file provides tests for individual messages.  It tests
// NetlinkMessageFactory's ability to create specific message types and it
// tests the various NetlinkMessage types' ability to parse those
// messages.

// This file tests the public interface to NetlinkManager.
#include "shill/netlink_manager.h"

#include <map>
#include <string>

#include <base/stringprintf.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "shill/mock_event_dispatcher.h"
#include "shill/mock_log.h"
#include "shill/mock_netlink_socket.h"
#include "shill/mock_sockets.h"
#include "shill/mock_time.h"
#include "shill/netlink_attribute.h"
#include "shill/nl80211_message.h"
#include "shill/scope_logger.h"

using base::Bind;
using base::StringPrintf;
using base::Unretained;
using std::map;
using std::string;
using testing::_;
using testing::AnyNumber;
using testing::EndsWith;
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

// Family and group Ids.
const char kFamilyStoogesString[] = "stooges";  // Not saved as a legal family.
const char kGroupMoeString[] = "moe";  // Not saved as a legal group.
const char kFamilyMarxString[] = "marx";
const uint16_t kFamilyMarxNumber = 20;
const char kGroupGrouchoString[] = "groucho";
const uint32_t kGroupGrouchoNumber = 21;
const char kGroupHarpoString[] = "harpo";
const uint32_t kGroupHarpoNumber = 22;
const char kGroupChicoString[] = "chico";
const uint32_t kGroupChicoNumber = 23;
const char kGroupZeppoString[] = "zeppo";
const uint32_t kGroupZeppoNumber = 24;
const char kGroupGummoString[] = "gummo";
const uint32_t kGroupGummoNumber = 25;

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

class NetlinkManagerTest : public Test {
 public:
  NetlinkManagerTest()
      : netlink_manager_(NetlinkManager::GetInstance()),
        sockets_(new MockSockets),
        saved_sequence_number_(0) {
    netlink_manager_->message_types_[Nl80211Message::kMessageTypeString].
       family_id = kNl80211FamilyId;
    netlink_manager_->message_types_[kFamilyMarxString].family_id =
        kFamilyMarxNumber;
    netlink_manager_->message_types_[kFamilyMarxString].groups =
      map<string, uint32_t> {{kGroupGrouchoString, kGroupGrouchoNumber},
                             {kGroupHarpoString, kGroupHarpoNumber},
                             {kGroupChicoString, kGroupChicoNumber},
                             {kGroupZeppoString, kGroupZeppoNumber},
                             {kGroupGummoString, kGroupGummoNumber}};
    netlink_manager_->message_factory_.AddFactoryMethod(
        kNl80211FamilyId, Bind(&Nl80211Message::CreateMessage));
    Nl80211Message::SetMessageType(kNl80211FamilyId);
    // Passes ownership.
    netlink_socket_.sockets_.reset(sockets_);

    EXPECT_NE(reinterpret_cast<NetlinkManager *>(NULL), netlink_manager_);
    netlink_manager_->sock_ = &netlink_socket_;
    EXPECT_TRUE(netlink_manager_->Init());
  }

  ~NetlinkManagerTest() {
    // NetlinkManager is a singleton, the sock_ field *MUST* be cleared
    // before "NetlinkManagerTest::socket_" gets invalidated, otherwise
    // later tests will refer to a corrupted memory.
    netlink_manager_->sock_ = NULL;
  }

  // |SaveReply|, |SendMessage|, and |ReplyToSentMessage| work together to
  // enable a test to get a response to a sent message.  They must be called
  // in the order, above, so that a) a reply message is available to b) have
  // its sequence number replaced, and then c) sent back to the code.
  void SaveReply(const ByteString &message) {
    saved_message_ = message;
  }

  // Replaces the |saved_message_|'s sequence number with the sent value.
  bool SendMessage(const ByteString &outgoing_message) {
    if (outgoing_message.GetLength() < sizeof(nlmsghdr)) {
      LOG(ERROR) << "Outgoing message is too short";
      return false;
    }
    const nlmsghdr *outgoing_header =
        reinterpret_cast<const nlmsghdr *>(outgoing_message.GetConstData());

    if (saved_message_.GetLength() < sizeof(nlmsghdr)) {
      LOG(ERROR) << "Saved message is too short; have you called |SaveReply|?";
      return false;
    }
    nlmsghdr *reply_header =
        reinterpret_cast<nlmsghdr *>(saved_message_.GetData());

    reply_header->nlmsg_seq = outgoing_header->nlmsg_seq;
    saved_sequence_number_ = reply_header->nlmsg_seq;
    return true;
  }

  bool ReplyToSentMessage(ByteString *message) {
    if (!message) {
      return false;
    }
    *message = saved_message_;
    return true;
  }

  bool ReplyWithRandomMessage(ByteString *message) {
    GetFamilyMessage get_family_message;
    // Any number that's not 0 or 1 is acceptable, here.  Zero is bad because
    // we want to make sure that this message is different than the main
    // send/receive pair.  One is bad because the default for
    // |saved_sequence_number_| is zero and the likely default value for the
    // first sequence number generated from the code is 1.
    const uint32_t kRandomOffset = 1003;
    if (!message) {
      return false;
    }
    *message = get_family_message.Encode(saved_sequence_number_ +
                                         kRandomOffset);
    return true;
  }

 protected:
  class MockHandlerNetlink {
   public:
    MockHandlerNetlink() :
      on_netlink_message_(base::Bind(&MockHandlerNetlink::OnNetlinkMessage,
                                     base::Unretained(this))) {}
    MOCK_METHOD1(OnNetlinkMessage, void(const NetlinkMessage &msg));
    const NetlinkManager::NetlinkMessageHandler &on_netlink_message() const {
      return on_netlink_message_;
    }
   private:
    NetlinkManager::NetlinkMessageHandler on_netlink_message_;
    DISALLOW_COPY_AND_ASSIGN(MockHandlerNetlink);
  };

  class MockHandlerNetlinkAuxilliary {
   public:
    MockHandlerNetlinkAuxilliary() :
      on_netlink_message_(
          base::Bind(&MockHandlerNetlinkAuxilliary::OnErrorHandler,
                     base::Unretained(this))) {}
    MOCK_METHOD2(OnErrorHandler,
                 void(NetlinkManager::AuxilliaryMessageType type,
                      const NetlinkMessage *msg));
    const NetlinkManager::NetlinkAuxilliaryMessageHandler &on_netlink_message()
        const {
      return on_netlink_message_;
    }
   private:
    NetlinkManager::NetlinkAuxilliaryMessageHandler on_netlink_message_;
    DISALLOW_COPY_AND_ASSIGN(MockHandlerNetlinkAuxilliary);
  };

  class MockHandler80211 {
   public:
    MockHandler80211() :
      on_netlink_message_(base::Bind(&MockHandler80211::OnNetlinkMessage,
                                     base::Unretained(this))) {}
    MOCK_METHOD1(OnNetlinkMessage, void(const Nl80211Message &msg));
    const NetlinkManager::Nl80211MessageHandler &on_netlink_message() const {
      return on_netlink_message_;
    }
   private:
    NetlinkManager::Nl80211MessageHandler on_netlink_message_;
    DISALLOW_COPY_AND_ASSIGN(MockHandler80211);
  };

  void Reset() {
    netlink_manager_->Reset(false);
  }

  NetlinkManager *netlink_manager_;
  MockNetlinkSocket netlink_socket_;
  MockSockets *sockets_;  // Owned by |netlink_socket_|.
  ByteString saved_message_;
  uint32_t saved_sequence_number_;
};

namespace {

class TimeFunctor {
 public:
  TimeFunctor(time_t tv_sec, suseconds_t tv_usec) {
    return_value_.tv_sec = tv_sec;
    return_value_.tv_usec = tv_usec;
  }

  TimeFunctor() {
    return_value_.tv_sec = 0;
    return_value_.tv_usec = 0;
  }

  TimeFunctor(const TimeFunctor &other) {
    return_value_.tv_sec = other.return_value_.tv_sec;
    return_value_.tv_usec = other.return_value_.tv_usec;
  }

  TimeFunctor &operator=(const TimeFunctor &rhs) {
    return_value_.tv_sec = rhs.return_value_.tv_sec;
    return_value_.tv_usec = rhs.return_value_.tv_usec;
    return *this;
  }

  // Replaces GetTimeMonotonic.
  int operator()(struct timeval *answer) {
    if (answer) {
      *answer = return_value_;
    }
    return 0;
  }

 private:
  struct timeval return_value_;

  // No DISALLOW_COPY_AND_ASSIGN since testing::Invoke uses copy.
};

}  // namespace

TEST_F(NetlinkManagerTest, Start) {
  MockEventDispatcher dispatcher;

  EXPECT_CALL(dispatcher, CreateInputHandler(_, _, _));
  netlink_manager_->Start(&dispatcher);
}

TEST_F(NetlinkManagerTest, SubscribeToEvents) {
  ScopedMockLog log;
  EXPECT_CALL(log, Log(_, _, _)).Times(AnyNumber());

  // Family not registered.
  EXPECT_CALL(log, Log(logging::LOG_ERROR, _,
                       EndsWith("doesn't exist")));
  EXPECT_CALL(netlink_socket_, SubscribeToEvents(_)).Times(0);
  EXPECT_FALSE(netlink_manager_->SubscribeToEvents(kFamilyStoogesString,
                                                   kGroupMoeString));

  // Group not part of family
  string in_family = StringPrintf("doesn't exist in family '%s'",
                                  kFamilyMarxString);
  EXPECT_CALL(log, Log(logging::LOG_ERROR, _, EndsWith(in_family)));
  EXPECT_CALL(netlink_socket_, SubscribeToEvents(_)).Times(0);
  EXPECT_FALSE(netlink_manager_->SubscribeToEvents(kFamilyMarxString,
                                                   kGroupMoeString));

  // Family registered and group part of family.
  EXPECT_CALL(netlink_socket_, SubscribeToEvents(kGroupHarpoNumber)).
      WillOnce(Return(true));
  EXPECT_TRUE(netlink_manager_->SubscribeToEvents(kFamilyMarxString,
                                                  kGroupHarpoString));
}

TEST_F(NetlinkManagerTest, GetFamily) {
  const uint16_t kSampleMessageType = 42;
  const string kSampleMessageName("SampleMessageName");
  const uint32_t kRandomSequenceNumber = 3;

  NewFamilyMessage new_family_message;
  new_family_message.attributes()->CreateAttribute(
      CTRL_ATTR_FAMILY_ID,
      base::Bind(&NetlinkAttribute::NewControlAttributeFromId));
  new_family_message.attributes()->SetU16AttributeValue(
      CTRL_ATTR_FAMILY_ID, kSampleMessageType);
  new_family_message.attributes()->CreateAttribute(
      CTRL_ATTR_FAMILY_NAME,
      base::Bind(&NetlinkAttribute::NewControlAttributeFromId));
  new_family_message.attributes()->SetStringAttributeValue(
      CTRL_ATTR_FAMILY_NAME, kSampleMessageName);

  // The sequence number is immaterial since it'll be overwritten.
  SaveReply(new_family_message.Encode(kRandomSequenceNumber));
  EXPECT_CALL(netlink_socket_, SendMessage(_)).
      WillOnce(Invoke(this, &NetlinkManagerTest::SendMessage));
  EXPECT_CALL(netlink_socket_, file_descriptor()).WillRepeatedly(Return(0));
  EXPECT_CALL(*sockets_, Select(_, _, _, _, _)).WillOnce(Return(1));
  EXPECT_CALL(netlink_socket_, RecvMessage(_)).
      WillOnce(Invoke(this, &NetlinkManagerTest::ReplyToSentMessage));
  NetlinkMessageFactory::FactoryMethod null_factory;
  EXPECT_EQ(kSampleMessageType, netlink_manager_->GetFamily(kSampleMessageName,
                                                            null_factory));
}

TEST_F(NetlinkManagerTest, GetFamilyOneInterstitialMessage) {
  Reset();

  const uint16_t kSampleMessageType = 42;
  const string kSampleMessageName("SampleMessageName");
  const uint32_t kRandomSequenceNumber = 3;

  NewFamilyMessage new_family_message;
  new_family_message.attributes()->CreateAttribute(
      CTRL_ATTR_FAMILY_ID,
      base::Bind(&NetlinkAttribute::NewControlAttributeFromId));
  new_family_message.attributes()->SetU16AttributeValue(
      CTRL_ATTR_FAMILY_ID, kSampleMessageType);
  new_family_message.attributes()->CreateAttribute(
      CTRL_ATTR_FAMILY_NAME,
      base::Bind(&NetlinkAttribute::NewControlAttributeFromId));
  new_family_message.attributes()->SetStringAttributeValue(
      CTRL_ATTR_FAMILY_NAME, kSampleMessageName);

  // The sequence number is immaterial since it'll be overwritten.
  SaveReply(new_family_message.Encode(kRandomSequenceNumber));
  EXPECT_CALL(netlink_socket_, SendMessage(_)).
      WillOnce(Invoke(this, &NetlinkManagerTest::SendMessage));
  EXPECT_CALL(netlink_socket_, file_descriptor()).WillRepeatedly(Return(0));
  EXPECT_CALL(*sockets_, Select(_, _, _, _, _)).WillRepeatedly(Return(1));
  EXPECT_CALL(netlink_socket_, RecvMessage(_)).
      WillOnce(Invoke(this, &NetlinkManagerTest::ReplyWithRandomMessage)).
      WillOnce(Invoke(this, &NetlinkManagerTest::ReplyToSentMessage));
  NetlinkMessageFactory::FactoryMethod null_factory;
  EXPECT_EQ(kSampleMessageType, netlink_manager_->GetFamily(kSampleMessageName,
                                                            null_factory));
}

TEST_F(NetlinkManagerTest, GetFamilyTimeout) {
  Reset();
  MockTime time;
  Time *old_time = netlink_manager_->time_;
  netlink_manager_->time_ = &time;

  EXPECT_CALL(netlink_socket_, SendMessage(_)).WillOnce(Return(true));
  time_t kStartSeconds = 1234;  // Arbitrary.
  suseconds_t kSmallUsec = 100;
  EXPECT_CALL(time, GetTimeMonotonic(_)).
      WillOnce(Invoke(TimeFunctor(kStartSeconds, 0))).  // Initial time.
      WillOnce(Invoke(TimeFunctor(kStartSeconds, kSmallUsec))).
      WillOnce(Invoke(TimeFunctor(kStartSeconds, 2 * kSmallUsec))).
      WillOnce(Invoke(TimeFunctor(
          kStartSeconds + NetlinkManager::kMaximumNewFamilyWaitSeconds + 1,
          NetlinkManager::kMaximumNewFamilyWaitMicroSeconds)));
  EXPECT_CALL(netlink_socket_, file_descriptor()).WillRepeatedly(Return(0));
  EXPECT_CALL(*sockets_, Select(_, _, _, _, _)).WillRepeatedly(Return(1));
  EXPECT_CALL(netlink_socket_, RecvMessage(_)).
      WillRepeatedly(Invoke(this, &NetlinkManagerTest::ReplyWithRandomMessage));
  NetlinkMessageFactory::FactoryMethod null_factory;

  const string kSampleMessageName("SampleMessageName");
  EXPECT_EQ(NetlinkMessage::kIllegalMessageType,
            netlink_manager_->GetFamily(kSampleMessageName, null_factory));
  netlink_manager_->time_ = old_time;
}

TEST_F(NetlinkManagerTest, BroadcastHandler) {
  Reset();
  nlmsghdr *message = const_cast<nlmsghdr *>(
        reinterpret_cast<const nlmsghdr *>(kNL80211_CMD_DISCONNECT));

  MockHandlerNetlink handler1;
  MockHandlerNetlink handler2;

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

TEST_F(NetlinkManagerTest, MessageHandler) {
  Reset();
  MockHandlerNetlink handler_broadcast;
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
  NetlinkManager::NetlinkAuxilliaryMessageHandler null_error_handler;
  EXPECT_CALL(netlink_socket_, SendMessage(_)).WillRepeatedly(Return(true));
  EXPECT_TRUE(netlink_manager_->SendNl80211Message(
      &sent_message_1, handler_sent_1.on_netlink_message(),
      null_error_handler));
  // Make it appear that this message is in response to our sent message.
  received_message->nlmsg_seq = netlink_socket_.GetLastSequenceNumber();
  EXPECT_CALL(handler_sent_1, OnNetlinkMessage(_)).Times(1);
  netlink_manager_->OnNlMessageReceived(received_message);

  // Verify that broadcast handler is called for the message after the
  // message-specific handler is called once.
  EXPECT_CALL(handler_broadcast, OnNetlinkMessage(_)).Times(1);
  netlink_manager_->OnNlMessageReceived(received_message);

  // Install and then uninstall message-specific handler; verify broadcast
  // handler is called on message receipt.
  EXPECT_TRUE(netlink_manager_->SendNl80211Message(
      &sent_message_1, handler_sent_1.on_netlink_message(),
      null_error_handler));
  received_message->nlmsg_seq = netlink_socket_.GetLastSequenceNumber();
  EXPECT_TRUE(netlink_manager_->RemoveMessageHandler(sent_message_1));
  EXPECT_CALL(handler_broadcast, OnNetlinkMessage(_)).Times(1);
  netlink_manager_->OnNlMessageReceived(received_message);

  // Install handler for different message; verify that broadcast handler is
  // called for _this_ message.
  EXPECT_TRUE(netlink_manager_->SendNl80211Message(
      &sent_message_2, handler_sent_2.on_netlink_message(),
      null_error_handler));
  EXPECT_CALL(handler_broadcast, OnNetlinkMessage(_)).Times(1);
  netlink_manager_->OnNlMessageReceived(received_message);

  // Change the ID for the message to that of the second handler; verify that
  // the appropriate handler is called for _that_ message.
  received_message->nlmsg_seq = netlink_socket_.GetLastSequenceNumber();
  EXPECT_CALL(handler_sent_2, OnNetlinkMessage(_)).Times(1);
  netlink_manager_->OnNlMessageReceived(received_message);
}

TEST_F(NetlinkManagerTest, MultipartMessageHandler) {
  Reset();

  // Install a broadcast handler.
  MockHandlerNetlink broadcast_handler;
  EXPECT_TRUE(netlink_manager_->AddBroadcastHandler(
      broadcast_handler.on_netlink_message()));

  // Build a message and send it in order to install a response handler.
  TriggerScanMessage trigger_scan_message;
  MockHandler80211 response_handler;
  MockHandlerNetlinkAuxilliary auxilliary_handler;
  EXPECT_CALL(netlink_socket_, SendMessage(_)).WillOnce(Return(true));
  NetlinkManager::NetlinkAuxilliaryMessageHandler null_error_handler;
  EXPECT_TRUE(netlink_manager_->SendNl80211Message(
      &trigger_scan_message, response_handler.on_netlink_message(),
      auxilliary_handler.on_netlink_message()));

  // Build a multi-part response (well, it's just one message but it'll be
  // received multiple times).
  const uint32_t kSequenceNumber = 32;  // Arbitrary (replaced, later).
  NewScanResultsMessage new_scan_results;
  new_scan_results.AddFlag(NLM_F_MULTI);
  ByteString new_scan_results_bytes(new_scan_results.Encode(kSequenceNumber));
  nlmsghdr *received_message =
        reinterpret_cast<nlmsghdr *>(new_scan_results_bytes.GetData());
  received_message->nlmsg_seq = netlink_socket_.GetLastSequenceNumber();

  // Verify that the message-specific handler is called.
  EXPECT_CALL(response_handler, OnNetlinkMessage(_));
  netlink_manager_->OnNlMessageReceived(received_message);

  // Verify that the message-specific handler is still called.
  EXPECT_CALL(response_handler, OnNetlinkMessage(_));
  netlink_manager_->OnNlMessageReceived(received_message);

  // Build a Done message with the sent-message sequence number.
  DoneMessage done_message;
  ByteString done_message_bytes(
      done_message.Encode(netlink_socket_.GetLastSequenceNumber()));

  // Verify that the message-specific handler is called for the done message.
  EXPECT_CALL(auxilliary_handler, OnErrorHandler(_, _));
  netlink_manager_->OnNlMessageReceived(
      reinterpret_cast<nlmsghdr *>(done_message_bytes.GetData()));

  // Verify that broadcast handler is called now that the done message has
  // been seen.
  EXPECT_CALL(response_handler, OnNetlinkMessage(_)).Times(0);
  EXPECT_CALL(auxilliary_handler, OnErrorHandler(_, _)).Times(0);
  EXPECT_CALL(broadcast_handler, OnNetlinkMessage(_)).Times(1);
  netlink_manager_->OnNlMessageReceived(received_message);
}

TEST_F(NetlinkManagerTest, TimeoutResponseHandlers) {
  Reset();
  MockHandlerNetlink broadcast_handler;
  EXPECT_TRUE(netlink_manager_->AddBroadcastHandler(
      broadcast_handler.on_netlink_message()));

  // Set up the received message as a response to the get_wiphi_message
  // we're going to send.
  NewWiphyMessage new_wiphy_message;
  const uint32_t kRandomSequenceNumber = 3;
  ByteString new_wiphy_message_bytes =
      new_wiphy_message.Encode(kRandomSequenceNumber);
  nlmsghdr *received_message =
      reinterpret_cast<nlmsghdr *>(new_wiphy_message_bytes.GetData());

  // Setup a random received message to trigger wiping out old messages.
  NewScanResultsMessage new_scan_results;
  ByteString new_scan_results_bytes =
      new_scan_results.Encode(kRandomSequenceNumber);

  // Setup the timestamps of various messages
  MockTime time;
  Time *old_time = netlink_manager_->time_;
  netlink_manager_->time_ = &time;

  time_t kStartSeconds = 1234;  // Arbitrary.
  suseconds_t kSmallUsec = 100;
  EXPECT_CALL(time, GetTimeMonotonic(_)).
      WillOnce(Invoke(TimeFunctor(kStartSeconds, 0))).  // Initial time.
      WillOnce(Invoke(TimeFunctor(kStartSeconds, kSmallUsec))).

      WillOnce(Invoke(TimeFunctor(kStartSeconds, 0))).  // Initial time.
      WillOnce(Invoke(TimeFunctor(
          kStartSeconds + NetlinkManager::kResponseTimeoutSeconds + 1,
          NetlinkManager::kResponseTimeoutMicroSeconds)));
  EXPECT_CALL(netlink_socket_, SendMessage(_)).WillRepeatedly(Return(true));

  GetWiphyMessage get_wiphi_message;
  MockHandler80211 response_handler;
  MockHandlerNetlinkAuxilliary auxilliary_handler;

  GetRegMessage get_reg_message;  // Just a message to trigger timeout.
  NetlinkManager::Nl80211MessageHandler null_message_handler;
  NetlinkManager::NetlinkAuxilliaryMessageHandler null_error_handler;

  // Send two messages within the message handler timeout; verify that we
  // get called back (i.e., that the first handler isn't discarded).
  EXPECT_TRUE(netlink_manager_->SendNl80211Message(
      &get_wiphi_message, response_handler.on_netlink_message(),
      auxilliary_handler.on_netlink_message()));
  received_message->nlmsg_seq = netlink_socket_.GetLastSequenceNumber();
  EXPECT_TRUE(netlink_manager_->SendNl80211Message(
      &get_reg_message, null_message_handler, null_error_handler));
  EXPECT_CALL(response_handler, OnNetlinkMessage(_));
  netlink_manager_->OnNlMessageReceived(received_message);

  // Send two messages at an interval greater than the message handler timeout
  // before the response to the first arrives.  Verify that the error handler
  // for the first message is called (with a timeout flag) and that the
  // broadcast handler gets called, instead of the message's handler.
  EXPECT_TRUE(netlink_manager_->SendNl80211Message(
      &get_wiphi_message, response_handler.on_netlink_message(),
      auxilliary_handler.on_netlink_message()));
  received_message->nlmsg_seq = netlink_socket_.GetLastSequenceNumber();
  EXPECT_CALL(auxilliary_handler,
              OnErrorHandler(NetlinkManager::kTimeoutWaitingForResponse, NULL));
  EXPECT_TRUE(netlink_manager_->SendNl80211Message(&get_reg_message,
                                                   null_message_handler,
                                                   null_error_handler));
  EXPECT_CALL(response_handler, OnNetlinkMessage(_)).Times(0);
  EXPECT_CALL(broadcast_handler, OnNetlinkMessage(_));
  netlink_manager_->OnNlMessageReceived(received_message);

  // Put the state of the singleton back where it was.
  netlink_manager_->time_ = old_time;
}

}  // namespace shill
