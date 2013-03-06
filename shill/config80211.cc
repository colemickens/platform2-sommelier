// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/config80211.h"

#include <ctype.h>
#include <netlink/msg.h>
#include <sys/select.h>
#include <sys/time.h>

#include <map>
#include <sstream>
#include <string>

#include <base/memory/weak_ptr.h>
#include <base/stl_util.h>

#include "shill/attribute_list.h"
#include "shill/error.h"
#include "shill/io_handler.h"
#include "shill/logging.h"
#include "shill/netlink_socket.h"
#include "shill/nl80211_message.h"
#include "shill/scope_logger.h"
#include "shill/shill_time.h"

using base::Bind;
using base::LazyInstance;
using std::list;
using std::map;
using std::string;

namespace shill {

namespace {
LazyInstance<Config80211> g_config80211 = LAZY_INSTANCE_INITIALIZER;
}  // namespace

const char Config80211::kEventTypeConfig[] = "config";
const char Config80211::kEventTypeScan[] = "scan";
const char Config80211::kEventTypeRegulatory[] = "regulatory";
const char Config80211::kEventTypeMlme[] = "mlme";
const long Config80211::kMaximumNewFamilyWaitSeconds = 1;
const long Config80211::kMaximumNewFamilyWaitMicroSeconds = 0;

Config80211::MessageType::MessageType() :
  family_id(NetlinkMessage::kIllegalMessageType) {}

Config80211::Config80211()
    : dispatcher_(NULL),
      weak_ptr_factory_(this),
      dispatcher_callback_(Bind(&Config80211::OnRawNlMessageReceived,
                                weak_ptr_factory_.GetWeakPtr())),
      sock_(NULL) {}

Config80211 *Config80211::GetInstance() {
  return g_config80211.Pointer();
}

void Config80211::Reset(bool full) {
  ClearBroadcastHandlers();
  message_types_.clear();
  if (full) {
    dispatcher_ = NULL;
    delete sock_;
    sock_ = NULL;
  }
}

void Config80211::OnNewFamilyMessage(const NetlinkMessage &raw_message) {
  uint16_t family_id;
  string family_name;

  if (raw_message.message_type() == ErrorAckMessage::kMessageType) {
    const ErrorAckMessage *error_ack_message =
        reinterpret_cast<const ErrorAckMessage *>(&raw_message);
    if (error_ack_message->error()) {
      LOG(ERROR) << __func__ << ": Message (seq: "
                 << raw_message.sequence_number() << ") failed: "
                 << error_ack_message->ToString();
    } else {
      SLOG(WiFi, 6) << __func__ << ": Message (seq: "
                 << raw_message.sequence_number() << ") ACKed";
    }
    return;
  }

  if (raw_message.message_type() != ControlNetlinkMessage::kMessageType) {
    LOG(ERROR) << "Received unexpected message type: "
               << raw_message.message_type();
    return;
  }

  const ControlNetlinkMessage *message =
      reinterpret_cast<const ControlNetlinkMessage *>(&raw_message);

  if (!message->const_attributes()->GetU16AttributeValue(CTRL_ATTR_FAMILY_ID,
                                                         &family_id)) {
    LOG(ERROR) << __func__ << ": Couldn't get family_id attribute";
    return;
  }

  if (!message->const_attributes()->GetStringAttributeValue(
      CTRL_ATTR_FAMILY_NAME, &family_name)) {
    LOG(ERROR) << __func__ << ": Couldn't get family_name attribute";
    return;
  }

  SLOG(WiFi, 3) << "Socket family '" << family_name << "' has id=" << family_id;

  // Extract the available multicast groups from the message.
  AttributeListConstRefPtr multicast_groups;
  if (message->const_attributes()->ConstGetNestedAttributeList(
      CTRL_ATTR_MCAST_GROUPS, &multicast_groups)) {
    AttributeListConstRefPtr current_group;

    for (int i = 1;
         multicast_groups->ConstGetNestedAttributeList(i, &current_group);
         ++i) {
      string group_name;
      uint32_t group_id;
      if (!current_group->GetStringAttributeValue(CTRL_ATTR_MCAST_GRP_NAME,
                                                  &group_name)) {
        LOG(WARNING) << "Expected CTRL_ATTR_MCAST_GRP_NAME, found none";
        continue;
      }
      if (!current_group->GetU32AttributeValue(CTRL_ATTR_MCAST_GRP_ID,
                                               &group_id)) {
        LOG(WARNING) << "Expected CTRL_ATTR_MCAST_GRP_ID, found none";
        continue;
      }
      SLOG(WiFi, 3) << "  Adding group '" << group_name << "' = " << group_id;
      message_types_[family_name].groups[group_name] = group_id;
    }
  }

  message_types_[family_name].family_id = family_id;
}

bool Config80211::Init() {
  if (!sock_) {
    sock_ = new NetlinkSocket;
    if (!sock_) {
      LOG(ERROR) << "No memory";
      return false;
    }

    if (!sock_->Init()) {
      return false;
    }
  }
  return true;
}

void Config80211::Start(EventDispatcher *dispatcher) {
  dispatcher_ = dispatcher;
  CHECK(dispatcher_);
  // Install ourselves in the shill mainloop so we receive messages on the
  // netlink socket.
  dispatcher_handler_.reset(dispatcher_->CreateInputHandler(
      file_descriptor(),
      dispatcher_callback_,
      Bind(&Config80211::OnReadError, weak_ptr_factory_.GetWeakPtr())));
}

uint16_t Config80211::GetFamily(string name) {
  MessageType &message_type = message_types_[name];
  if (message_type.family_id != NetlinkMessage::kIllegalMessageType) {
    return message_type.family_id;
  }
  if (!sock_) {
    LOG(FATAL) << "Must call |Init| before this method.";
    return false;
  }

  GetFamilyMessage msg;
  if (!msg.attributes()->CreateStringAttribute(CTRL_ATTR_FAMILY_NAME,
                                               "CTRL_ATTR_FAMILY_NAME")) {
    LOG(ERROR) << "Couldn't create string attribute";
    return false;
  }
  if (!msg.attributes()->SetStringAttributeValue(CTRL_ATTR_FAMILY_NAME, name)) {
    LOG(ERROR) << "Couldn't set string attribute";
    return false;
  }
  SendMessage(&msg, Bind(&Config80211::OnNewFamilyMessage,
                         weak_ptr_factory_.GetWeakPtr()));

  // Wait for a response.  The code absolutely needs family_ids for its
  // message types so we do a synchronous wait.  It's OK to do this because
  // a) libnl does a synchronous wait (so there's prior art), b) waiting
  // asynchronously would add significant and unnecessary complexity to the
  // code that deals with pending messages that could, potentially, be waiting
  // for a message type, and c) it really doesn't take very long for the
  // GETFAMILY / NEWFAMILY transaction to transpire (this transaction was timed
  // over 20 times and found a maximum duration of 11.1 microseconds and an
  // average of 4.0 microseconds).
  struct timeval start_time, now, end_time;
  struct timeval maximum_wait_duration = {kMaximumNewFamilyWaitSeconds,
                                          kMaximumNewFamilyWaitMicroSeconds};
  Time *time = Time::GetInstance();
  time->GetTimeMonotonic(&start_time);
  now = start_time;
  timeradd(&start_time, &maximum_wait_duration, &end_time);

  do {
    // Wait with timeout for a message from the netlink socket.
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(file_descriptor(), &read_fds);
    struct timeval wait_duration;
    timersub(&end_time, &now, &wait_duration);
    int result = select(file_descriptor() + 1, &read_fds, NULL, NULL,
                        &wait_duration);
    if (result < 0) {
      PLOG(ERROR) << "Select failed";
      return NetlinkMessage::kIllegalMessageType;
    }
    if (result == 0) {
      LOG(WARNING) << "Timed out waiting for family_id for family '"
                   << name << "'.";
      return NetlinkMessage::kIllegalMessageType;
    }

    // Read and process any messages.
    ByteString received;
    sock_->RecvMessage(&received);
    InputData input_data(received.GetData(), received.GetLength());
    OnRawNlMessageReceived(&input_data);
    if (message_type.family_id != NetlinkMessage::kIllegalMessageType) {
      time->GetTimeMonotonic(&now);
      timersub(&now, &start_time, &wait_duration);
      SLOG(WiFi, 5) << "Found id " << message_type.family_id
                    << " for name '" << name << "' in "
                    << wait_duration.tv_sec << " sec, "
                    << wait_duration.tv_usec << " usec.";
      return message_type.family_id;
    }
    time->GetTimeMonotonic(&now);
  } while (timercmp(&now, &end_time, <));

  LOG(ERROR) << "Timed out waiting for family_id for family '" << name << "'.";
  return NetlinkMessage::kIllegalMessageType;
}

uint16_t Config80211::GetMessageType(string name) const {
  map<const string, MessageType>::const_iterator family =
      message_types_.find(name);
  if (family == message_types_.end()) {
    LOG(WARNING) << "Family '" << name << "' is not in list.";
    return NetlinkMessage::kIllegalMessageType;
  }
  return family->second.family_id;
}


bool Config80211::AddBroadcastHandler(const NetlinkMessageHandler &handler) {
  list<NetlinkMessageHandler>::iterator i;
  if (FindBroadcastHandler(handler)) {
    LOG(WARNING) << "Trying to re-add a handler";
    return false;  // Should only be one copy in the list.
  }
  if (handler.is_null()) {
    LOG(WARNING) << "Trying to add a NULL handler";
    return false;
  }
  // And add the handler to the list.
  SLOG(WiFi, 3) << "Config80211::" << __func__ << " - adding handler";
  broadcast_handlers_.push_back(handler);
  return true;
}

bool Config80211::RemoveBroadcastHandler(
    const NetlinkMessageHandler &handler) {
  list<NetlinkMessageHandler>::iterator i;
  for (i = broadcast_handlers_.begin(); i != broadcast_handlers_.end(); ++i) {
    if ((*i).Equals(handler)) {
      broadcast_handlers_.erase(i);
      // Should only be one copy in the list so we don't have to continue
      // looking for another one.
      return true;
    }
  }
  LOG(WARNING) << "NetlinkMessageHandler not found.";
  return false;
}

bool Config80211::FindBroadcastHandler(const NetlinkMessageHandler &handler)
    const {
  list<NetlinkMessageHandler>::const_iterator i;
  for (i = broadcast_handlers_.begin(); i != broadcast_handlers_.end(); ++i) {
    if ((*i).Equals(handler)) {
      return true;
    }
  }
  return false;
}

void Config80211::ClearBroadcastHandlers() {
  broadcast_handlers_.clear();
}

bool Config80211::SendMessage(NetlinkMessage *message,
                              const NetlinkMessageHandler &handler) {
  if (!message) {
    LOG(ERROR) << "Message is NULL.";
    return false;
  }

  ByteString message_string = message->Encode(this->GetSequenceNumber());

  if (handler.is_null()) {
    SLOG(WiFi, 3) << "Handler for message was null.";
  } else if (ContainsKey(message_handlers_, message->sequence_number())) {
    LOG(ERROR) << "A handler already existed for sequence: "
               << message->sequence_number();
    return false;
  } else {
    message_handlers_[message->sequence_number()] = handler;
  }

  SLOG(WiFi, 6) << "NL Message " << message->sequence_number()
                << " Sending (" << message_string.GetLength()
                << " bytes) ===>";
  message->Print(6);
  NetlinkMessage::PrintBytes(6, message_string.GetConstData(),
                             message_string.GetLength());

  if (!sock_->SendMessage(message_string)) {
    LOG(ERROR) << "Failed to send Netlink message.";
    return false;
  }
  return true;
}

bool Config80211::RemoveMessageHandler(const NetlinkMessage &message) {
  if (!ContainsKey(message_handlers_, message.sequence_number())) {
    return false;
  }
  message_handlers_.erase(message.sequence_number());
  return true;
}

uint32_t Config80211::GetSequenceNumber() {
  return sock_ ?
      sock_->GetSequenceNumber() : NetlinkMessage::kBroadcastSequenceNumber;
}

bool Config80211::SubscribeToEvents(const string &family_id,
                                    const string &group_name) {
  if (!ContainsKey(message_types_, family_id)) {
    LOG(ERROR) << "Family '" << family_id << "' doesn't exist";
    return false;
  }

  if (!ContainsKey(message_types_[family_id].groups, group_name)) {
    LOG(ERROR) << "Group '" << group_name << "' doesn't exist in family '"
               << family_id << "'";
    return false;
  }

  uint32_t group_id = message_types_[family_id].groups[group_name];
  if (!sock_) {
    LOG(FATAL) << "Need to call |Init| first.";
  }
  return sock_->SubscribeToEvents(group_id);
}

void Config80211::OnRawNlMessageReceived(InputData *data) {
  if (!data) {
    LOG(ERROR) << __func__ << "() called with null header.";
    return;
  }
  unsigned char *buf = data->buf;
  unsigned char *end = buf + data->len;
  while (buf < end) {
    nlmsghdr *msg = reinterpret_cast<nlmsghdr *>(buf);
    // Discard the message if there're not enough bytes to a) tell the code how
    // much space is in the message (i.e., to access nlmsg_len) or b) to hold
    // the entire message.  The odd calculation is there to keep the code from
    // potentially calculating an illegal address (causes a segfault on some
    // architectures).
    size_t bytes_left = end - buf;
    if (((bytes_left < (offsetof(nlmsghdr, nlmsg_len) +
                        sizeof(msg->nlmsg_len))) ||
         (bytes_left < msg->nlmsg_len))) {
      LOG(ERROR) << "Discarding incomplete message.";
      return;
    }
    OnNlMessageReceived(msg);
    buf += msg->nlmsg_len;
  }
}

void Config80211::OnNlMessageReceived(nlmsghdr *msg) {
  if (!msg) {
    LOG(ERROR) << __func__ << "() called with null header.";
    return;
  }
  const uint32 sequence_number = msg->nlmsg_seq;
  scoped_ptr<NetlinkMessage> message(NetlinkMessageFactory::CreateMessage(msg));
  if (message == NULL) {
    SLOG(WiFi, 3) << "NL Message " << sequence_number << " <===";
    SLOG(WiFi, 3) << __func__ << "(msg:NULL)";
    return;  // Skip current message, continue parsing buffer.
  }
  SLOG(WiFi, 3) << "NL Message " << sequence_number
                << " Received (" << msg->nlmsg_len << " bytes) <===";
  message->Print(6);
  NetlinkMessage::PrintBytes(8, reinterpret_cast<const unsigned char *>(msg),
                             msg->nlmsg_len);

  // Call (then erase) any message-specific handler.
  if (ContainsKey(message_handlers_, sequence_number)) {
    SLOG(WiFi, 3) << "found message-specific handler";
    if (message_handlers_[sequence_number].is_null()) {
      LOG(ERROR) << "NetlinkMessageHandler exists but is NULL for ID "
                 << sequence_number;
    } else {
      message_handlers_[sequence_number].Run(*message);
    }

    if (message->message_type() == ErrorAckMessage::kMessageType) {
      const ErrorAckMessage *error_ack_message =
          reinterpret_cast<const ErrorAckMessage *>(message.get());
      if (error_ack_message->error()) {
        SLOG(WiFi, 3) << "Removing callback";
        message_handlers_.erase(sequence_number);
      } else {
        SLOG(WiFi, 3) << "ACK message -- not removing callback";
      }
    } else if ((message->flags() & NLM_F_MULTI) &&
        (message->message_type() != NLMSG_DONE)) {
      SLOG(WiFi, 3) << "Multi-part message -- not removing callback";
    } else {
      SLOG(WiFi, 3) << "Removing callback";
      message_handlers_.erase(sequence_number);
    }
  } else {
    list<NetlinkMessageHandler>::const_iterator i =
        broadcast_handlers_.begin();
    while (i != broadcast_handlers_.end()) {
      SLOG(WiFi, 3) << __func__ << " - calling broadcast handler";
      i->Run(*message);
      ++i;
    }
  }
}

void Config80211::OnReadError(const Error &error) {
  // TODO(wdg): When config80211 is used for scan, et al., this should either be
  // LOG(FATAL) or the code should properly deal with errors, e.g., dropped
  // messages due to the socket buffer being full.
  LOG(ERROR) << "Config80211's netlink Socket read returns error: "
             << error.message();
}


}  // namespace shill.
