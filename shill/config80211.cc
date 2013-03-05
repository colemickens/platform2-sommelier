// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/config80211.h"

#include <ctype.h>
#include <netlink/msg.h>

#include <map>
#include <sstream>
#include <string>

#include <base/memory/weak_ptr.h>
#include <base/stl_util.h>

#include "shill/error.h"
#include "shill/io_handler.h"
#include "shill/logging.h"
#include "shill/nl80211_message.h"
#include "shill/nl80211_socket.h"
#include "shill/scope_logger.h"

using base::Bind;
using base::LazyInstance;
using std::list;
using std::string;

namespace shill {

namespace {
LazyInstance<Config80211> g_config80211 = LAZY_INSTANCE_INITIALIZER;
}  // namespace

Config80211::EventTypeStrings *Config80211::event_types_ = NULL;

Config80211::Config80211()
    : wifi_state_(kWifiDown),
      dispatcher_(NULL),
      weak_ptr_factory_(this),
      dispatcher_callback_(Bind(&Config80211::OnRawNlMessageReceived,
                                weak_ptr_factory_.GetWeakPtr())),
      sock_(NULL) {
}

Config80211::~Config80211() {
  // Since Config80211 is a singleton, it should be safe to delete a static
  // member.
  delete event_types_;
  event_types_ = NULL;
}

Config80211 *Config80211::GetInstance() {
  return g_config80211.Pointer();
}

void Config80211::Reset(bool full) {
  wifi_state_ = kWifiDown;
  subscribed_events_.clear();
  ClearBroadcastHandlers();
  if (full) {
    dispatcher_ = NULL;
    delete sock_;
    sock_ = NULL;
  }
}

bool Config80211::Init(EventDispatcher *dispatcher) {
  if (!sock_) {
    sock_ = new Nl80211Socket;
    if (!sock_) {
      LOG(ERROR) << "No memory";
      return false;
    }

    if (!sock_->Init()) {
      return false;
    }
    // Don't make libnl do the sequence checking (because it'll not like
    // the broadcast messages for which we'll eventually ask).  We'll do all the
    // sequence checking we need.
    sock_->DisableSequenceChecking();
  }

  if (!event_types_) {
    event_types_ = new EventTypeStrings;
    (*event_types_)[Config80211::kEventTypeConfig] = "config";
    (*event_types_)[Config80211::kEventTypeScan] = "scan";
    (*event_types_)[Config80211::kEventTypeRegulatory] = "regulatory";
    (*event_types_)[Config80211::kEventTypeMlme] = "mlme";
  }

  dispatcher_ = dispatcher;
  return true;
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

  ByteString message_string = message->Encode(this->GetSequenceNumber(),
                                              sock_->family_id());

  SLOG(WiFi, 6) << "NL Message " << message->sequence_number()
                << " Sending ===>";
  message->Print(6);

  if (!sock_->SendMessage(message_string)) {
    LOG(ERROR) << "Failed to send Netlink message.";
    return false;
  }
  if (handler.is_null()) {
    LOG(INFO) << "Handler for message was null.";
    return true;
  }
  if (ContainsKey(message_handlers_, message->sequence_number())) {
    LOG(ERROR) << "Sent message, but already had a handler for this message?";
    return false;
  }
  message_handlers_[message->sequence_number()] = handler;
  LOG(INFO) << "Sent Netlink message with sequence number: "
            << message->sequence_number();
  return true;
}

bool Config80211::RemoveMessageHandler(const NetlinkMessage &message) {
  if (!ContainsKey(message_handlers_, message.sequence_number())) {
    return false;
  }
  message_handlers_.erase(message.sequence_number());
  return true;
}

// static
bool Config80211::GetEventTypeString(EventType type, string *value) {
  if (!value) {
    LOG(ERROR) << "NULL |value|";
    return false;
  }
  if (!event_types_) {
    LOG(ERROR) << "NULL |event_types_|";
    return false;
  }

  EventTypeStrings::iterator match = (*event_types_).find(type);
  if (match == (*event_types_).end()) {
    LOG(ERROR) << "Event type " << type << " not found";
    return false;
  }
  *value = match->second;
  return true;
}

void Config80211::SetWifiState(WifiState new_state) {
  if (wifi_state_ == new_state) {
    return;
  }

  if (!sock_) {
    LOG(ERROR) << "Config80211::Init needs to be called before this";
    return;
  }

  if (new_state == kWifiUp) {
    // Install ourselves in the shill mainloop so we receive messages on the
    // nl80211 socket.
    if (dispatcher_) {
      dispatcher_handler_.reset(dispatcher_->CreateInputHandler(
          GetFd(),
          dispatcher_callback_,
          Bind(&Config80211::OnReadError, weak_ptr_factory_.GetWeakPtr())));
    }

    // If we're newly-up, subscribe to all the event types that have been
    // requested.
    SubscribedEvents::const_iterator i;
    for (i = subscribed_events_.begin(); i != subscribed_events_.end(); ++i) {
      ActuallySubscribeToEvents(*i);
    }
  }
  wifi_state_ = new_state;
}

uint32_t Config80211::GetSequenceNumber() {
  return sock_ ?
      sock_->GetSequenceNumber() : NetlinkMessage::kBroadcastSequenceNumber;
}

bool Config80211::SubscribeToEvents(EventType type) {
  bool it_worked = true;
  if (!ContainsKey(subscribed_events_, type)) {
    if (wifi_state_ == kWifiUp) {
      it_worked = ActuallySubscribeToEvents(type);
    }
    // |subscribed_events_| is a list of events to which we want to subscribe
    // when wifi comes up (including when it comes _back_ up after it goes
    // down sometime in the future).
    subscribed_events_.insert(type);
  }
  return it_worked;
}

bool Config80211::ActuallySubscribeToEvents(EventType type) {
  string group_name;

  if (!GetEventTypeString(type, &group_name)) {
    return false;
  }
  if (!sock_->AddGroupMembership(group_name)) {
    return false;
  }
  return true;
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
  // TODO(wdg): Support multi-part messages; don't delete handler until last
  // part appears.
  if (ContainsKey(message_handlers_, sequence_number)) {
    SLOG(WiFi, 3) << "found message-specific handler";
    if (message_handlers_[sequence_number].is_null()) {
      LOG(ERROR) << "NetlinkMessageHandler exists but is NULL for ID "
                 << sequence_number;
    } else {
      message_handlers_[sequence_number].Run(*message);
    }
    message_handlers_.erase(sequence_number);
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
