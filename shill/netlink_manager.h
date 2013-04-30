// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This software provides an abstracted interface to the netlink socket
// interface.  In its current implementation it is used, primarily, to
// communicate with the cfg80211 kernel module and mac80211 drivers:
//
//         [shill]--[nl80211 library, libnl_genl/libnl libraries]
//            |
//     (netlink socket)
//            |
// [cfg80211 kernel module]
//            |
//    [mac80211 drivers]
//
// In order to send a message and handle it's response, do the following:
// - Create a handler (it'll want to verify that it's the kind of message you
//   want, cast it to the appropriate type, and get attributes from the cast
//   message):
//
//    #include "nl80211_message.h"
//    class SomeClass {
//      static void MyMessageHandler(const NetlinkMessage &raw) {
//        if (raw.message_type() != ControlNetlinkMessage::kMessageType)
//          return;
//        const ControlNetlinkMessage *message =
//          reinterpret_cast<const ControlNetlinkMessage *>(&raw);
//        if (message.command() != NewFamilyMessage::kCommand)
//          return;
//        uint16_t my_attribute;
//        message->const_attributes()->GetU16AttributeValue(
//          CTRL_ATTR_FAMILY_ID, &my_attribute);
//      }  // MyMessageHandler.
//    }  // class SomeClass.
//
// - Instantiate a message:
//
//    #include "nl80211_message.h"
//    GetFamilyMessage msg;
//
// - And add attributes:
//
//    if (msg.attributes()->CreateStringAttribute(CTRL_ATTR_FAMILY_NAME,
//                                                "CTRL_ATTR_FAMILY_NAME")) {
//      msg.attributes()->SetStringAttributeValue(CTRL_ATTR_FAMILY_NAME,
//                                                "foo");
//    }
//
// - Then send the message, passing-in a closure to the handler you created:
//
//    NetlinkManager *netlink_manager = NetlinkManager::GetInstance();
//    netlink_manager->SendMessage(&msg, Bind(&SomeClass::MyMessageHandler));
//
// NetlinkManager will then save your handler and send your message.  When a
// response to your message arrives, it'll call your handler.
//

#ifndef SHILL_NETLINK_MANAGER_H_
#define SHILL_NETLINK_MANAGER_H_

#include <list>
#include <map>
#include <set>
#include <string>

#include <base/basictypes.h>
#include <base/bind.h>
#include <base/lazy_instance.h>
#include <base/memory/scoped_ptr.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST

#include "shill/io_handler.h"
#include "shill/netlink_message.h"

struct nlmsghdr;

namespace shill {

class Error;
class EventDispatcher;
struct InputData;
class NetlinkSocket;

// NetlinkManager is a singleton that coordinates sending netlink messages to,
// and receiving netlink messages from, the kernel.  The first use of this is
// to communicate between user-space and the cfg80211 module that manages wifi
// drivers.  Bring NetlinkManager up as follows:
//  NetlinkManager *netlink_manager_ = NetlinkManager::GetInstance();
//  EventDispatcher dispatcher_;
//  netlink_manager_->Init();  // Initialize the socket.
//  // Get message types for all dynamic message types.
//  Nl80211Message::SetMessageType(
//      netlink_manager_->GetFamily(Nl80211Message::kMessageTypeString,
//                              Bind(&Nl80211Message::CreateMessage)));
//  netlink_manager_->Start(&dispatcher_);
class NetlinkManager {
 public:
  typedef base::Callback<void(const NetlinkMessage &)> NetlinkMessageHandler;

  // Encapsulates all the different things we know about a specific message
  // type like its name, and its id.
  struct MessageType {
    MessageType();

    uint16_t family_id;

    // Multicast groups supported by the family.  The string and mapping to
    // a group id are extracted from the CTRL_CMD_NEWFAMILY message.
    std::map<std::string, uint32_t> groups;
  };

  // Various kinds of events to which we can subscribe (and receive) from
  // cfg80211.
  static const char kEventTypeConfig[];
  static const char kEventTypeScan[];
  static const char kEventTypeRegulatory[];
  static const char kEventTypeMlme[];

  // NetlinkManager is a singleton and this is the way to access it.
  static NetlinkManager *GetInstance();

  // Performs non-trivial object initialization of the NetlinkManager singleton.
  bool Init();

  // Passes the job of waiting for, and the subsequent reading from, the
  // netlink socket to |dispatcher|.
  void Start(EventDispatcher *dispatcher);

  // The following methods deal with the network family table.  This table
  // associates netlink family names with family_ids (also called message
  // types).  Note that some families have static ids assigned to them but
  // others require the kernel to resolve a string describing the family into
  // a dynamically-determined id.

  // Returns the family_id (message type) associated with |family_name|,
  // calling the kernel if needed.  Returns
  // |NetlinkMessage::kIllegalMessageType| if the message type could not be
  // determined.  May block so |GetFamily| should be called before entering the
  // event loop.
  uint16_t GetFamily(std::string family_name,
      const NetlinkMessageFactory::FactoryMethod &message_factory);

  // Retrieves a family id (message type) given the |name| string describing
  // the message family.
  virtual uint16_t GetMessageType(const std::string &name) const;

  // Install a NetlinkManager NetlinkMessageHandler.  The handler is a
  // user-supplied object to be called by the system for user-bound messages
  // that do not have a corresponding messaage-specific callback.
  // |AddBroadcastHandler| should be called before |SubscribeToEvents| since
  // the result of this call are used for that call.
  virtual bool AddBroadcastHandler(
      const NetlinkMessageHandler &message_handler);

  // Uninstall a NetlinkMessage Handler.
  virtual bool RemoveBroadcastHandler(
      const NetlinkMessageHandler &message_handler);

  // Determines whether a handler is in the list of broadcast handlers.
  bool FindBroadcastHandler(const NetlinkMessageHandler &message_handler) const;

  // Uninstall all broadcast netlink message handlers.
  void ClearBroadcastHandlers();

  // Sends a netlink message to the kernel using the NetlinkManager socket after
  // installing a handler to deal with the kernel's response to the message.
  // TODO(wdg): Eventually, this should also include a timeout and a callback
  // to call in case of timeout.
  virtual bool SendMessage(NetlinkMessage *message,
                           const NetlinkMessageHandler &message_handler);

  // Uninstall the handler for a specific netlink message.
  bool RemoveMessageHandler(const NetlinkMessage &message);

  // Sign-up to receive and log multicast events of a specific type (once wifi
  // is up).
  virtual bool SubscribeToEvents(const std::string &family,
                                 const std::string &group);

  // Gets the next sequence number for a NetlinkMessage to be sent over
  // NetlinkManager's netlink socket.
  uint32_t GetSequenceNumber();

 protected:
  friend struct base::DefaultLazyInstanceTraits<NetlinkManager>;

  explicit NetlinkManager();

 private:
  friend class NetlinkManagerTest;
  friend class NetlinkMessageTest;
  friend class ShillDaemonTest;
  FRIEND_TEST(NetlinkManagerTest, AddLinkTest);
  FRIEND_TEST(NetlinkManagerTest, BroadcastHandlerTest);
  FRIEND_TEST(NetlinkManagerTest, MessageHandlerTest);
  FRIEND_TEST(NetlinkMessageTest, Parse_NL80211_CMD_TRIGGER_SCAN);
  FRIEND_TEST(NetlinkMessageTest, Parse_NL80211_CMD_NEW_SCAN_RESULTS);
  FRIEND_TEST(NetlinkMessageTest, Parse_NL80211_CMD_NEW_STATION);
  FRIEND_TEST(NetlinkMessageTest, Parse_NL80211_CMD_AUTHENTICATE);
  FRIEND_TEST(NetlinkMessageTest, Parse_NL80211_CMD_ASSOCIATE);
  FRIEND_TEST(NetlinkMessageTest, Parse_NL80211_CMD_CONNECT);
  FRIEND_TEST(NetlinkMessageTest, Parse_NL80211_CMD_DEAUTHENTICATE);
  FRIEND_TEST(NetlinkMessageTest, Parse_NL80211_CMD_DISCONNECT);
  FRIEND_TEST(NetlinkMessageTest, Parse_NL80211_CMD_NOTIFY_CQM);
  FRIEND_TEST(NetlinkMessageTest, Parse_NL80211_CMD_DISASSOCIATE);

  typedef std::map<uint32_t, NetlinkMessageHandler> MessageHandlers;

  static const long kMaximumNewFamilyWaitSeconds;
  static const long kMaximumNewFamilyWaitMicroSeconds;

  // Returns the file descriptor of socket used to read wifi data.
  int file_descriptor() const;

  // EventDispatcher calls this when data is available on our socket.  This
  // method passes each, individual, message in the input to
  // |OnNlMessageReceived|.  Each part of a multipart message gets handled,
  // individually, by this method.
  void OnRawNlMessageReceived(InputData *data);

  // This method processes a message from |OnRawNlMessageReceived| by passing
  // the message to either the NetlinkManager callback that matches the sequence
  // number of the message or, if there isn't one, to all of the default
  // NetlinkManager callbacks in |broadcast_handlers_|.
  void OnNlMessageReceived(nlmsghdr *msg);

  // Called by InputHandler on exceptional events.
  void OnReadError(const Error &error);

  // Just for tests, this method turns off WiFi and clears the subscribed
  // events list. If |full| is true, also clears state set by Init.
  void Reset(bool full);

  // Handles a CTRL_CMD_NEWFAMILY message from the kernel.
  void OnNewFamilyMessage(const NetlinkMessage &message);

  // NetlinkManager Handlers, OnRawNlMessageReceived invokes each of these
  // User-supplied callback object when _it_ gets called to read libnl data.
  std::list<NetlinkMessageHandler> broadcast_handlers_;

  // Message-specific callbacks, mapped by message ID.
  MessageHandlers message_handlers_;

  // Hooks needed to be called by shill's EventDispatcher.
  EventDispatcher *dispatcher_;
  base::WeakPtrFactory<NetlinkManager> weak_ptr_factory_;
  base::Callback<void(InputData *)> dispatcher_callback_;
  scoped_ptr<IOHandler> dispatcher_handler_;

  NetlinkSocket *sock_;
  std::map<const std::string, MessageType> message_types_;
  NetlinkMessageFactory message_factory_;

  DISALLOW_COPY_AND_ASSIGN(NetlinkManager);
};

}  // namespace shill

#endif  // SHILL_NETLINK_MANAGER_H_
