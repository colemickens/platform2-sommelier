// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/netlink_message.h"

#include <limits.h>
#include <netlink/msg.h>
#include <netlink/netlink.h>

#include <algorithm>
#include <map>
#include <string>

#include <base/format_macros.h>
#include <base/stl_util.h>
#include <base/strings/stringprintf.h>

#include "shill/logging.h"

using base::StringAppendF;
using base::StringPrintf;
using std::map;
using std::min;
using std::string;

namespace shill {

const uint32_t NetlinkMessage::kBroadcastSequenceNumber = 0;
const uint16_t NetlinkMessage::kIllegalMessageType = UINT16_MAX;

// NetlinkMessage

ByteString NetlinkMessage::EncodeHeader(uint32_t sequence_number) {
  ByteString result;
  if (message_type_ == kIllegalMessageType) {
    LOG(ERROR) << "Message type not set";
    return result;
  }
  sequence_number_ = sequence_number;
  if (sequence_number_ == kBroadcastSequenceNumber) {
    LOG(ERROR) << "Couldn't get a legal sequence number";
    return result;
  }

  // Build netlink header.
  nlmsghdr header;
  size_t nlmsghdr_with_pad = NLMSG_ALIGN(sizeof(header));
  header.nlmsg_len = nlmsghdr_with_pad;
  header.nlmsg_type = message_type_;
  header.nlmsg_flags = NLM_F_REQUEST | flags_;
  header.nlmsg_seq = sequence_number_;
  header.nlmsg_pid = getpid();

  // Netlink header + pad.
  result.Append(ByteString(reinterpret_cast<unsigned char *>(&header),
                           sizeof(header)));
  result.Resize(nlmsghdr_with_pad);  // Zero-fill pad space (if any).
  return result;
}

bool NetlinkMessage::InitAndStripHeader(ByteString *input) {
  if (!input) {
    LOG(ERROR) << "NULL input";
    return false;
  }
  if (input->GetLength() < sizeof(nlmsghdr)) {
    LOG(ERROR) << "Insufficient input to extract nlmsghdr";
    return false;
  }

  // Read the nlmsghdr.
  nlmsghdr *header = reinterpret_cast<nlmsghdr *>(input->GetData());
  message_type_ = header->nlmsg_type;
  flags_ = header->nlmsg_flags;
  sequence_number_ = header->nlmsg_seq;

  // Strip the nlmsghdr.
  input->RemovePrefix(NLMSG_ALIGN(sizeof(struct nlmsghdr)));
  return true;
}

bool NetlinkMessage::InitFromNlmsg(const nlmsghdr *const_msg) {
  if (!const_msg) {
    LOG(ERROR) << "Null |const_msg| parameter";
    return false;
  }
  ByteString message(reinterpret_cast<const unsigned char *>(const_msg),
                     const_msg->nlmsg_len);
  if (!InitAndStripHeader(&message)) {
    return false;
  }
  return true;
}

// static
void NetlinkMessage::PrintBytes(int log_level, const unsigned char *buf,
                                size_t num_bytes) {
  SLOG(WiFi, log_level) << "Netlink Message -- Examining Bytes";
  if (!buf) {
    SLOG(WiFi, log_level) << "<NULL Buffer>";
    return;
  }

  if (num_bytes >= sizeof(nlmsghdr)) {
      const nlmsghdr *header = reinterpret_cast<const nlmsghdr *>(buf);
      SLOG(WiFi, log_level) << StringPrintf(
          "len:          %02x %02x %02x %02x = %u bytes",
          buf[0], buf[1], buf[2], buf[3], header->nlmsg_len);

      SLOG(WiFi, log_level) << StringPrintf(
          "type | flags: %02x %02x %02x %02x - type:%u flags:%s%s%s%s%s",
          buf[4], buf[5], buf[6], buf[7], header->nlmsg_type,
          ((header->nlmsg_flags & NLM_F_REQUEST) ? " REQUEST" : ""),
          ((header->nlmsg_flags & NLM_F_MULTI) ? " MULTI" : ""),
          ((header->nlmsg_flags & NLM_F_ACK) ? " ACK" : ""),
          ((header->nlmsg_flags & NLM_F_ECHO) ? " ECHO" : ""),
          ((header->nlmsg_flags & NLM_F_DUMP_INTR) ? " BAD-SEQ" : ""));

      SLOG(WiFi, log_level) << StringPrintf(
          "sequence:     %02x %02x %02x %02x = %u",
          buf[8], buf[9], buf[10], buf[11], header->nlmsg_seq);
      SLOG(WiFi, log_level) << StringPrintf(
          "pid:          %02x %02x %02x %02x = %u",
          buf[12], buf[13], buf[14], buf[15], header->nlmsg_pid);
      buf += sizeof(nlmsghdr);
      num_bytes -= sizeof(nlmsghdr);
  } else {
    SLOG(WiFi, log_level) << "Not enough bytes (" << num_bytes
                          << ") for a complete nlmsghdr (requires "
                          << sizeof(nlmsghdr) << ").";
  }

  while (num_bytes) {
    string output;
    size_t bytes_this_row = min(num_bytes, static_cast<size_t>(32));
    for (size_t i = 0; i < bytes_this_row; ++i) {
      StringAppendF(&output, " %02x", *buf++);
    }
    SLOG(WiFi, log_level) << output;
    num_bytes -= bytes_this_row;
  }
}

// ErrorAckMessage.

const uint16_t ErrorAckMessage::kMessageType = NLMSG_ERROR;

bool ErrorAckMessage::InitFromNlmsg(const nlmsghdr *const_msg) {
  if (!const_msg) {
    LOG(ERROR) << "Null |const_msg| parameter";
    return false;
  }
  ByteString message(reinterpret_cast<const unsigned char *>(const_msg),
                     const_msg->nlmsg_len);
  if (!InitAndStripHeader(&message)) {
    return false;
  }

  // Get the error code from the payload.
  error_ = *(reinterpret_cast<const uint32_t *>(message.GetConstData()));
  return true;
}

ByteString ErrorAckMessage::Encode(uint32_t sequence_number) {
  LOG(ERROR) << "We're not supposed to send errors or Acks to the kernel";
  return ByteString();
}

string ErrorAckMessage::ToString() const {
  string output;
  if (error()) {
    StringAppendF(&output, "NETLINK_ERROR 0x%" PRIx32 ": %s",
                  -error_, strerror(-error_));
  } else {
    StringAppendF(&output, "ACK");
  }
  return output;
}

void ErrorAckMessage::Print(int header_log_level,
                            int /*detail_log_level*/) const {
  SLOG(WiFi, header_log_level) << ToString();
}

// NoopMessage.

const uint16_t NoopMessage::kMessageType = NLMSG_NOOP;

ByteString NoopMessage::Encode(uint32_t sequence_number) {
  LOG(ERROR) << "We're not supposed to send NOOP to the kernel";
  return ByteString();
}

void NoopMessage::Print(int header_log_level, int /*detail_log_level*/) const {
  SLOG(WiFi, header_log_level) << ToString();
}

// DoneMessage.

const uint16_t DoneMessage::kMessageType = NLMSG_DONE;

ByteString DoneMessage::Encode(uint32_t sequence_number) {
  return EncodeHeader(sequence_number);
}

void DoneMessage::Print(int header_log_level, int /*detail_log_level*/) const {
  SLOG(WiFi, header_log_level) << ToString();
}

// OverrunMessage.

const uint16_t OverrunMessage::kMessageType = NLMSG_OVERRUN;

ByteString OverrunMessage::Encode(uint32_t sequence_number) {
  LOG(ERROR) << "We're not supposed to send Overruns to the kernel";
  return ByteString();
}

void OverrunMessage::Print(int header_log_level,
                           int /*detail_log_level*/) const {
  SLOG(WiFi, header_log_level) << ToString();
}

// UnknownMessage.

ByteString UnknownMessage::Encode(uint32_t sequence_number) {
  LOG(ERROR) << "We're not supposed to send UNKNOWN messages to the kernel";
  return ByteString();
}

void UnknownMessage::Print(int header_log_level,
                           int /*detail_log_level*/) const {
  int total_bytes = message_body_.GetLength();
  const uint8_t *const_data = message_body_.GetConstData();

  string output = StringPrintf("%d bytes:", total_bytes);
  for (int i = 0; i < total_bytes; ++i) {
    StringAppendF(&output, " 0x%02x", const_data[i]);
  }
  SLOG(WiFi, header_log_level) << output;
}

//
// Factory class.
//

bool NetlinkMessageFactory::AddFactoryMethod(uint16_t message_type,
                                             FactoryMethod factory) {
  if (ContainsKey(factories_, message_type)) {
    LOG(WARNING) << "Message type " << message_type << " already exists.";
    return false;
  }
  if (message_type == NetlinkMessage::kIllegalMessageType) {
    LOG(ERROR) << "Not installing factory for illegal message type.";
    return false;
  }
  factories_[message_type] = factory;
  return true;
}

NetlinkMessage *NetlinkMessageFactory::CreateMessage(
    const nlmsghdr *const_msg) const {
  if (!const_msg) {
    LOG(ERROR) << "NULL |const_msg| parameter";
    return nullptr;
  }

  scoped_ptr<NetlinkMessage> message;

  if (const_msg->nlmsg_type == NoopMessage::kMessageType) {
    message.reset(new NoopMessage());
  } else if (const_msg->nlmsg_type == DoneMessage::kMessageType) {
    message.reset(new DoneMessage());
  } else if (const_msg->nlmsg_type == OverrunMessage::kMessageType) {
    message.reset(new OverrunMessage());
  } else if (const_msg->nlmsg_type == ErrorAckMessage::kMessageType) {
    message.reset(new ErrorAckMessage());
  } else if (ContainsKey(factories_, const_msg->nlmsg_type)) {
    map<uint16_t, FactoryMethod>::const_iterator factory;
    factory = factories_.find(const_msg->nlmsg_type);
    message.reset(factory->second.Run(const_msg));
  }

  // If no factory exists for this message _or_ if a factory exists but it
  // failed, there'll be no message.  Handle either of those cases, by
  // creating an |UnknownMessage|.
  if (!message) {
    // Casting away constness since, while nlmsg_data doesn't change its
    // parameter, it also doesn't declare its paramenter as const.
    nlmsghdr *msg = const_cast<nlmsghdr *>(const_msg);
    ByteString payload(reinterpret_cast<char *>(nlmsg_data(msg)),
                       nlmsg_datalen(msg));
    message.reset(new UnknownMessage(msg->nlmsg_type, payload));
  }

  if (!message->InitFromNlmsg(const_msg)) {
    LOG(ERROR) << "Message did not initialize properly";
    return nullptr;
  }

  return message.release();
}

}  // namespace shill.
