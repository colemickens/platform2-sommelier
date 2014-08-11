// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/netlink_manager.h"

#include <netlink/netlink.h>
#include <sys/select.h>
#include <sys/time.h>

#include <list>
#include <map>

#include <base/memory/weak_ptr.h>
#include <base/stl_util.h>

#include "shill/attribute_list.h"
#include "shill/error.h"
#include "shill/event_dispatcher.h"
#include "shill/generic_netlink_message.h"
#include "shill/io_handler.h"
#include "shill/logging.h"
#include "shill/netlink_message.h"
#include "shill/netlink_socket.h"
#include "shill/nl80211_message.h"
#include "shill/scope_logger.h"
#include "shill/shill_time.h"
#include "shill/sockets.h"

using base::Bind;
using base::LazyInstance;
using std::list;
using std::map;
using std::string;

namespace shill {

namespace {
LazyInstance<NetlinkManager> g_netlink_manager = LAZY_INSTANCE_INITIALIZER;
}  // namespace

const char NetlinkManager::kEventTypeConfig[] = "config";
const char NetlinkManager::kEventTypeScan[] = "scan";
const char NetlinkManager::kEventTypeRegulatory[] = "regulatory";
const char NetlinkManager::kEventTypeMlme[] = "mlme";
const long NetlinkManager::kMaximumNewFamilyWaitSeconds = 1;  // NOLINT
const long NetlinkManager::kMaximumNewFamilyWaitMicroSeconds = 0;  // NOLINT
const long NetlinkManager::kResponseTimeoutSeconds = 5;  // NOLINT
const long NetlinkManager::kResponseTimeoutMicroSeconds = 0;  // NOLINT

NetlinkManager::NetlinkResponseHandler::NetlinkResponseHandler(
    const NetlinkManager::NetlinkAuxilliaryMessageHandler &error_handler)
    : error_handler_(error_handler) {}

NetlinkManager::NetlinkResponseHandler::~NetlinkResponseHandler() {}

void NetlinkManager::NetlinkResponseHandler::HandleError(
    AuxilliaryMessageType type, const NetlinkMessage *netlink_message) const {
  if (!error_handler_.is_null())
    error_handler_.Run(type, netlink_message);
}

class ControlResponseHandler : public NetlinkManager::NetlinkResponseHandler {
 public:
  ControlResponseHandler(
      const NetlinkManager::ControlNetlinkMessageHandler &handler,
      const NetlinkManager::NetlinkAuxilliaryMessageHandler &error_handler)
    : NetlinkManager::NetlinkResponseHandler(error_handler),
      handler_(handler) {}

  virtual bool HandleMessage(
      const NetlinkMessage &netlink_message) const override {
    if (netlink_message.message_type() !=
        ControlNetlinkMessage::GetMessageType()) {
      LOG(ERROR) << "Message is type " << netlink_message.message_type()
                 << ", not " << ControlNetlinkMessage::GetMessageType()
                 << " (Control).";
      return false;
    }
    if (!handler_.is_null()) {
      const ControlNetlinkMessage *message =
          dynamic_cast<const ControlNetlinkMessage *>(&netlink_message);
      handler_.Run(*message);
    }
    return true;
  }

 private:
  NetlinkManager::ControlNetlinkMessageHandler handler_;

  DISALLOW_COPY_AND_ASSIGN(ControlResponseHandler);
};

class Nl80211ResponseHandler : public NetlinkManager::NetlinkResponseHandler {
 public:
  Nl80211ResponseHandler(
      const NetlinkManager::Nl80211MessageHandler &handler,
      const NetlinkManager::NetlinkAuxilliaryMessageHandler &error_handler)
    : NetlinkManager::NetlinkResponseHandler(error_handler),
      handler_(handler) {}

  virtual bool HandleMessage(
      const NetlinkMessage &netlink_message) const override {
    if (netlink_message.message_type() != Nl80211Message::GetMessageType()) {
      LOG(ERROR) << "Message is type " << netlink_message.message_type()
                 << ", not " << Nl80211Message::GetMessageType()
                 << " (Nl80211).";
      return false;
    }
    if (!handler_.is_null()) {
      const Nl80211Message *message =
          dynamic_cast<const Nl80211Message *>(&netlink_message);
      handler_.Run(*message);
    }
    return true;
  }

 private:
  NetlinkManager::Nl80211MessageHandler handler_;

  DISALLOW_COPY_AND_ASSIGN(Nl80211ResponseHandler);
};


NetlinkManager::MessageType::MessageType() :
  family_id(NetlinkMessage::kIllegalMessageType) {}

NetlinkManager::NetlinkManager()
    : dispatcher_(NULL),
      weak_ptr_factory_(this),
      dispatcher_callback_(Bind(&NetlinkManager::OnRawNlMessageReceived,
                                weak_ptr_factory_.GetWeakPtr())),
      sock_(NULL),
      time_(Time::GetInstance()) {}

NetlinkManager *NetlinkManager::GetInstance() {
  return g_netlink_manager.Pointer();
}

void NetlinkManager::Reset(bool full) {
  ClearBroadcastHandlers();
  message_handlers_.clear();
  message_types_.clear();
  if (full) {
    dispatcher_ = NULL;
    delete sock_;
    sock_ = NULL;
  }
}

void NetlinkManager::OnNewFamilyMessage(const ControlNetlinkMessage &message) {
  uint16_t family_id;
  string family_name;

  if (!message.const_attributes()->GetU16AttributeValue(CTRL_ATTR_FAMILY_ID,
                                                         &family_id)) {
    LOG(ERROR) << __func__ << ": Couldn't get family_id attribute";
    return;
  }

  if (!message.const_attributes()->GetStringAttributeValue(
      CTRL_ATTR_FAMILY_NAME, &family_name)) {
    LOG(ERROR) << __func__ << ": Couldn't get family_name attribute";
    return;
  }

  SLOG(WiFi, 3) << "Socket family '" << family_name << "' has id=" << family_id;

  // Extract the available multicast groups from the message.
  AttributeListConstRefPtr multicast_groups;
  if (message.const_attributes()->ConstGetNestedAttributeList(
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

// static
void NetlinkManager::OnNetlinkMessageError(AuxilliaryMessageType type,
                                           const NetlinkMessage *raw_message) {
  switch (type) {
    case kErrorFromKernel:
      if (!raw_message) {
        LOG(ERROR) << "Unknown error from kernel.";
        break;
      }
      if (raw_message->message_type() == ErrorAckMessage::GetMessageType()) {
        const ErrorAckMessage *error_ack_message =
            dynamic_cast<const ErrorAckMessage *>(raw_message);
        if (error_ack_message->error()) {
          LOG(ERROR) << __func__ << ": Message (seq: "
                     << error_ack_message->sequence_number() << ") failed: "
                     << error_ack_message->ToString();
        } else {
          SLOG(WiFi, 6) << __func__ << ": Message (seq: "
                     << error_ack_message->sequence_number() << ") ACKed";
        }
      }
      break;

    case kUnexpectedResponseType:
      LOG(ERROR) << "Message not handled by regular message handler:";
      if (raw_message) {
        raw_message->Print(0, 0);
      }
      break;

    case kTimeoutWaitingForResponse:
      LOG(WARNING) << "Timeout waiting for response";
      break;

    default:
      LOG(ERROR) << "Unexpected auxilliary message type: " << type;
      break;
  }
}

bool NetlinkManager::Init() {
  // Install message factory for control class of messages, which has
  // statically-known message type.
  message_factory_.AddFactoryMethod(
      ControlNetlinkMessage::kMessageType,
      Bind(&ControlNetlinkMessage::CreateMessage));
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

void NetlinkManager::Start(EventDispatcher *dispatcher) {
  dispatcher_ = dispatcher;
  CHECK(dispatcher_);
  // Install ourselves in the shill mainloop so we receive messages on the
  // netlink socket.
  dispatcher_handler_.reset(dispatcher_->CreateInputHandler(
      file_descriptor(),
      dispatcher_callback_,
      Bind(&NetlinkManager::OnReadError, weak_ptr_factory_.GetWeakPtr())));
}

int NetlinkManager::file_descriptor() const {
  return (sock_ ? sock_->file_descriptor() : -1);
}

uint16_t NetlinkManager::GetFamily(const string &name,
    const NetlinkMessageFactory::FactoryMethod &message_factory) {
  MessageType &message_type = message_types_[name];
  if (message_type.family_id != NetlinkMessage::kIllegalMessageType) {
    return message_type.family_id;
  }
  if (!sock_) {
    LOG(FATAL) << "Must call |Init| before this method.";
    return false;
  }

  GetFamilyMessage msg;
  if (!msg.attributes()->SetStringAttributeValue(CTRL_ATTR_FAMILY_NAME, name)) {
    LOG(ERROR) << "Couldn't set string attribute";
    return false;
  }
  SendControlMessage(&msg,
                     Bind(&NetlinkManager::OnNewFamilyMessage,
                          weak_ptr_factory_.GetWeakPtr()),
                     Bind(&NetlinkManager::OnNetlinkMessageError));

  // Wait for a response.  The code absolutely needs family_ids for its
  // message types so we do a synchronous wait.  It's OK to do this because
  // a) libnl does a synchronous wait (so there's prior art), b) waiting
  // asynchronously would add significant and unnecessary complexity to the
  // code that deals with pending messages that could, potentially, be waiting
  // for a message type, and c) it really doesn't take very long for the
  // GETFAMILY / NEWFAMILY transaction to transpire (this transaction was timed
  // over 20 times and found a maximum duration of 11.1 microseconds and an
  // average of 4.0 microseconds).
  struct timeval now, end_time;
  struct timeval maximum_wait_duration = {kMaximumNewFamilyWaitSeconds,
                                          kMaximumNewFamilyWaitMicroSeconds};
  time_->GetTimeMonotonic(&now);
  timeradd(&now, &maximum_wait_duration, &end_time);

  do {
    // Wait with timeout for a message from the netlink socket.
    fd_set read_fds;
    FD_ZERO(&read_fds);

    int socket = file_descriptor();
    if (socket >= FD_SETSIZE)
       LOG(FATAL) << "Invalid file_descriptor.";
    FD_SET(socket, &read_fds);

    struct timeval wait_duration;
    timersub(&end_time, &now, &wait_duration);
    int result = sock_->sockets()->Select(file_descriptor() + 1,
                                          &read_fds,
                                          NULL,
                                          NULL,
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
      uint16_t family_id = message_type.family_id;
      if (family_id != NetlinkMessage::kIllegalMessageType) {
        message_factory_.AddFactoryMethod(family_id, message_factory);
      }
      return message_type.family_id;
    }
    time_->GetTimeMonotonic(&now);
  } while (timercmp(&now, &end_time, <));

  LOG(ERROR) << "Timed out waiting for family_id for family '" << name << "'.";
  return NetlinkMessage::kIllegalMessageType;
}

bool NetlinkManager::AddBroadcastHandler(const NetlinkMessageHandler &handler) {
  if (FindBroadcastHandler(handler)) {
    LOG(WARNING) << "Trying to re-add a handler";
    return false;  // Should only be one copy in the list.
  }
  if (handler.is_null()) {
    LOG(WARNING) << "Trying to add a NULL handler";
    return false;
  }
  // And add the handler to the list.
  SLOG(WiFi, 3) << "NetlinkManager::" << __func__ << " - adding handler";
  broadcast_handlers_.push_back(handler);
  return true;
}

bool NetlinkManager::RemoveBroadcastHandler(
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

bool NetlinkManager::FindBroadcastHandler(const NetlinkMessageHandler &handler)
    const {
  for (const auto &broadcast_handler : broadcast_handlers_) {
    if (broadcast_handler.Equals(handler)) {
      return true;
    }
  }
  return false;
}

void NetlinkManager::ClearBroadcastHandlers() {
  broadcast_handlers_.clear();
}

bool NetlinkManager::SendControlMessage(
    ControlNetlinkMessage *message,
    const ControlNetlinkMessageHandler &message_handler,
    const NetlinkAuxilliaryMessageHandler &error_handler) {
  return SendMessageInternal(message,
                             new ControlResponseHandler(message_handler,
                                                        error_handler));
}

bool NetlinkManager::SendNl80211Message(
    Nl80211Message *message,
    const Nl80211MessageHandler &message_handler,
    const NetlinkAuxilliaryMessageHandler &error_handler) {
  return SendMessageInternal(message,
                             new Nl80211ResponseHandler(message_handler,
                                                        error_handler));
}

bool NetlinkManager::SendMessageInternal(
    NetlinkMessage *message,
    NetlinkManager::NetlinkResponseHandler *response_handler) {
  if (!message) {
    LOG(ERROR) << "Message is NULL.";
    return false;
  }

  // Clean out timed-out message handlers.  The list of outstanding messages
  // should be small so the time wasted by looking through all of them should
  // be small.
  struct timeval now;
  time_->GetTimeMonotonic(&now);
  map<uint32_t, NetlinkResponseHandlerRefPtr>::iterator handler_it =
      message_handlers_.begin();
  while (handler_it != message_handlers_.end()) {
    if (timercmp(&now, &handler_it->second->delete_after(), >)) {
      // A timeout isn't always unexpected so this is not a warning.
      SLOG(WiFi, 3) << "Removing timed-out handler for sequence number "
                    << handler_it->first;
      handler_it->second->HandleError(kTimeoutWaitingForResponse, NULL);
      handler_it = message_handlers_.erase(handler_it);
    } else {
      ++handler_it;
    }
  }

  // On to the business at hand...
  ByteString message_string = message->Encode(this->GetSequenceNumber());

  if (!response_handler) {
    SLOG(WiFi, 3) << "Handler for message was null.";
  } else if (ContainsKey(message_handlers_, message->sequence_number())) {
    LOG(ERROR) << "A handler already existed for sequence: "
               << message->sequence_number();
    return false;
  } else {
    struct timeval response_timeout = {kResponseTimeoutSeconds,
                                       kResponseTimeoutMicroSeconds};
    struct timeval delete_after;
    timeradd(&now, &response_timeout, &delete_after);
    response_handler->set_delete_after(delete_after);

    message_handlers_[message->sequence_number()] =
        NetlinkResponseHandlerRefPtr(response_handler);
  }

  SLOG(WiFi, 5) << "NL Message " << message->sequence_number()
                << " Sending (" << message_string.GetLength()
                << " bytes) ===>";
  message->Print(6, 7);
  NetlinkMessage::PrintBytes(8, message_string.GetConstData(),
                             message_string.GetLength());

  if (!sock_->SendMessage(message_string)) {
    LOG(ERROR) << "Failed to send Netlink message.";
    return false;
  }
  return true;
}

bool NetlinkManager::RemoveMessageHandler(const NetlinkMessage &message) {
  if (!ContainsKey(message_handlers_, message.sequence_number())) {
    return false;
  }
  message_handlers_.erase(message.sequence_number());
  return true;
}

uint32_t NetlinkManager::GetSequenceNumber() {
  return sock_ ?
      sock_->GetSequenceNumber() : NetlinkMessage::kBroadcastSequenceNumber;
}

bool NetlinkManager::SubscribeToEvents(const string &family_id,
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

void NetlinkManager::OnRawNlMessageReceived(InputData *data) {
  if (!data) {
    LOG(ERROR) << __func__ << "() called with null header.";
    return;
  }
  unsigned char *buf = data->buf;
  unsigned char *end = buf + data->len;
  while (buf < end) {
    nlmsghdr *msg = reinterpret_cast<nlmsghdr *>(buf);
    size_t bytes_left = end - buf;
    if (bytes_left < sizeof(nlmsghdr) || bytes_left < msg->nlmsg_len) {
      LOG(ERROR) << "Discarding incomplete message.";
      return;
    }
    OnNlMessageReceived(msg);
    buf += msg->nlmsg_len;
  }
}

void NetlinkManager::OnNlMessageReceived(nlmsghdr *msg) {
  if (!msg) {
    LOG(ERROR) << __func__ << "() called with null header.";
    return;
  }
  const uint32_t sequence_number = msg->nlmsg_seq;

  scoped_ptr<NetlinkMessage> message(message_factory_.CreateMessage(msg));
  if (message == NULL) {
    SLOG(WiFi, 3) << "NL Message " << sequence_number << " <===";
    SLOG(WiFi, 3) << __func__ << "(msg:NULL)";
    return;  // Skip current message, continue parsing buffer.
  }
  SLOG(WiFi, 5) << "NL Message " << sequence_number
                << " Received (" << msg->nlmsg_len << " bytes) <===";
  message->Print(6, 7);
  NetlinkMessage::PrintBytes(8, reinterpret_cast<const unsigned char *>(msg),
                             msg->nlmsg_len);

  if (message->message_type() == ErrorAckMessage::GetMessageType()) {
    SLOG(WiFi, 3) << "Error response to message " << sequence_number;
    const ErrorAckMessage *error_ack_message =
        dynamic_cast<const ErrorAckMessage *>(message.get());
    if (error_ack_message->error()) {
      if (ContainsKey(message_handlers_, sequence_number)) {
        SLOG(WiFi, 6) << "Found message-specific error handler";
        message_handlers_[sequence_number]->HandleError(kErrorFromKernel,
                                                        message.get());
        message_handlers_.erase(sequence_number);
      }
    } else {
      SLOG(WiFi, 3) << "ACK message -- not removing callback";
    }
    return;
  }

  if (ContainsKey(message_handlers_, sequence_number)) {
    SLOG(WiFi, 6) << "Found message-specific handler";
    if (!message_handlers_[sequence_number]->HandleMessage(*message)) {
      LOG(ERROR) << "Couldn't call message handler for " << sequence_number;
      // Call the error handler but, since we don't have an |ErrorAckMessage|,
      // we'll have to pass a NULL pointer.
      message_handlers_[sequence_number]->HandleError(kUnexpectedResponseType,
                                                      NULL);
    }
    if ((message->flags() & NLM_F_MULTI) &&
        (message->message_type() != NLMSG_DONE)) {
      SLOG(WiFi, 6) << "Multi-part message -- not removing callback";
    } else {
      SLOG(WiFi, 6) << "Removing callbacks";
      message_handlers_.erase(sequence_number);
    }
    return;
  }

  for (const auto &handler : broadcast_handlers_) {
    SLOG(WiFi, 6) << "Calling broadcast handler";
    if (!handler.is_null()) {
      handler.Run(*message);
    }
  }
}

void NetlinkManager::OnReadError(const Error &error) {
  // TODO(wdg): When netlink_manager is used for scan, et al., this should
  // either be LOG(FATAL) or the code should properly deal with errors,
  // e.g., dropped messages due to the socket buffer being full.
  LOG(ERROR) << "NetlinkManager's netlink Socket read returns error: "
             << error.message();
}


}  // namespace shill.
