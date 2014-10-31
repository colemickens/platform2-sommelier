// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/net/generic_netlink_message.h"

#include <netlink/msg.h>
#include <netlink/netlink.h>

#include <base/bind.h>
#include <base/logging.h>
#include <base/strings/stringprintf.h>

#include "shill/net/netlink_attribute.h"

using base::Bind;
using base::StringPrintf;

namespace shill {

ByteString GenericNetlinkMessage::EncodeHeader(uint32_t sequence_number) {
  // Build nlmsghdr.
  ByteString result(NetlinkMessage::EncodeHeader(sequence_number));
  if (result.GetLength() == 0) {
    LOG(ERROR) << "Couldn't encode message header.";
    return result;
  }

  // Build and append the genl message header.
  genlmsghdr genl_header;
  genl_header.cmd = command();
  genl_header.version = 1;
  genl_header.reserved = 0;

  ByteString genl_header_string(
      reinterpret_cast<unsigned char *>(&genl_header), sizeof(genl_header));
  size_t genlmsghdr_with_pad = NLMSG_ALIGN(sizeof(genl_header));
  genl_header_string.Resize(genlmsghdr_with_pad);  // Zero-fill.

  nlmsghdr *pheader = reinterpret_cast<nlmsghdr *>(result.GetData());
  pheader->nlmsg_len += genlmsghdr_with_pad;
  result.Append(genl_header_string);
  return result;
}

ByteString GenericNetlinkMessage::Encode(uint32_t sequence_number) {
  ByteString result(EncodeHeader(sequence_number));
  if (result.GetLength() == 0) {
    LOG(ERROR) << "Couldn't encode message header.";
    return result;
  }

  // Build and append attributes (padding is included by
  // AttributeList::Encode).
  ByteString attribute_string = attributes_->Encode();

  // Need to re-calculate |header| since |Append|, above, moves the data.
  nlmsghdr *pheader = reinterpret_cast<nlmsghdr *>(result.GetData());
  pheader->nlmsg_len += attribute_string.GetLength();
  result.Append(attribute_string);

  return result;
}

bool GenericNetlinkMessage::InitAndStripHeader(ByteString *input) {
  if (!input) {
    LOG(ERROR) << "NULL input";
    return false;
  }
  if (!NetlinkMessage::InitAndStripHeader(input)) {
    return false;
  }

  // Read the genlmsghdr.
  genlmsghdr *gnlh = reinterpret_cast<genlmsghdr *>(input->GetData());
  if (command_ != gnlh->cmd) {
    LOG(WARNING) << "This object thinks it's a " << command_
                 << " but the message thinks it's a " << gnlh->cmd;
  }

  // Strip the genlmsghdr.
  input->RemovePrefix(NLMSG_ALIGN(sizeof(struct genlmsghdr)));
  return true;
}

void GenericNetlinkMessage::Print(int header_log_level,
                                  int detail_log_level) const {
  VLOG(header_log_level) << StringPrintf("Message %s (%d)",
                                         command_string(),
                                         command());
  attributes_->Print(detail_log_level, 1);
}

// Control Message

const uint16_t ControlNetlinkMessage::kMessageType = GENL_ID_CTRL;

bool ControlNetlinkMessage::InitFromNlmsg(const nlmsghdr *const_msg) {
  if (!const_msg) {
    LOG(ERROR) << "Null |msg| parameter";
    return false;
  }
  ByteString message(reinterpret_cast<const unsigned char *>(const_msg),
                     const_msg->nlmsg_len);

  if (!InitAndStripHeader(&message)) {
    return false;
  }

  // Attributes.
  // Parse the attributes from the nl message payload into the 'tb' array.
  nlattr *tb[CTRL_ATTR_MAX + 1];
  nla_parse(tb, CTRL_ATTR_MAX,
            reinterpret_cast<nlattr *>(message.GetData()), message.GetLength(),
            nullptr);

  for (int i = 0; i < CTRL_ATTR_MAX + 1; ++i) {
    if (tb[i]) {
      attributes_->CreateAndInitAttribute(
          i, tb[i], Bind(&NetlinkAttribute::NewControlAttributeFromId));
    }
  }
  return true;
}

// Specific Control types.

const uint8_t NewFamilyMessage::kCommand = CTRL_CMD_NEWFAMILY;
const char NewFamilyMessage::kCommandString[] = "CTRL_CMD_NEWFAMILY";

const uint8_t GetFamilyMessage::kCommand = CTRL_CMD_GETFAMILY;
const char GetFamilyMessage::kCommandString[] = "CTRL_CMD_GETFAMILY";

GetFamilyMessage::GetFamilyMessage()
    : ControlNetlinkMessage(kCommand, kCommandString) {
  attributes()->CreateStringAttribute(CTRL_ATTR_FAMILY_NAME,
                                      "CTRL_ATTR_FAMILY_NAME");
}

// static
NetlinkMessage *ControlNetlinkMessage::CreateMessage(
    const nlmsghdr *const_msg) {
  if (!const_msg) {
    LOG(ERROR) << "NULL |const_msg| parameter";
    return nullptr;
  }
  // Casting away constness since, while nlmsg_data doesn't change its
  // parameter, it also doesn't declare its paramenter as const.
  nlmsghdr *msg = const_cast<nlmsghdr *>(const_msg);
  void *payload = nlmsg_data(msg);
  genlmsghdr *gnlh = reinterpret_cast<genlmsghdr *>(payload);

  switch (gnlh->cmd) {
    case NewFamilyMessage::kCommand:
      return new NewFamilyMessage();
    case GetFamilyMessage::kCommand:
      return new GetFamilyMessage();
    default:
      LOG(WARNING) << "Unknown/unhandled netlink control message " << gnlh->cmd;
      return new UnknownControlMessage(gnlh->cmd);
      break;
  }
  return nullptr;
}

}  // namespace shill.
