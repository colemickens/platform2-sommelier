// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/kernel_bound_nlmessage.h"

#include <net/if.h>
#include <netlink/genl/genl.h>
#include <netlink/msg.h>
#include <netlink/netlink.h>

#include <base/logging.h>

#include "shill/logging.h"
#include "shill/netlink_socket.h"
#include "shill/scope_logger.h"

namespace shill {

const uint32_t KernelBoundNlMessage::kIllegalMessage = 0xFFFFFFFF;

KernelBoundNlMessage::~KernelBoundNlMessage() {
  if (message_) {
    nlmsg_free(message_);
    message_ = NULL;
  }
}

bool KernelBoundNlMessage::Init() {
  message_ = nlmsg_alloc();

  if (!message_) {
    LOG(ERROR) << "Couldn't allocate |message_|";
    return false;
  }

  return true;
}

uint32_t KernelBoundNlMessage::GetId() const {
  if (!message_) {
    LOG(ERROR) << "NULL |message_|";
    return kIllegalMessage;
  }
  struct nlmsghdr *header = nlmsg_hdr(message_);
  if (!header) {
    LOG(ERROR) << "Couldn't make header";
    return kIllegalMessage;
  }
  return header->nlmsg_seq;
}

bool KernelBoundNlMessage::AddNetlinkHeader(uint32_t port, uint32_t seq,
                                            int family_id, int hdrlen,
                                            int flags, uint8_t cmd,
                                            uint8_t version) {
  if (!message_) {
    LOG(ERROR) << "NULL |message_|";
    return false;
  }

  // Parameters to genlmsg_put:
  //  @message: struct nl_msg *message_.
  //  @pid: netlink pid the message is addressed to.
  //  @seq: sequence number (usually the one of the sender).
  //  @family: generic netlink family.
  //  @flags netlink message flags.
  //  @cmd: netlink command.
  //  @version: version of communication protocol.
  // genlmsg_put returns a void * pointing to the header but we don't want to
  // encourage its use outside of this object.

  if (genlmsg_put(message_, port, seq, family_id, hdrlen, flags, cmd, version)
      == NULL) {
    LOG(ERROR) << "genlmsg_put returned a NULL pointer.";
    return false;
  }

  return true;
}

int KernelBoundNlMessage::AddAttribute(int attrtype, int attrlen,
                                       const void *data) {
  if (!data) {
    LOG(ERROR) << "NULL |data| parameter";
    return -1;
  }
  if (!message_) {
    LOG(ERROR) << "NULL |message_|.";
    return -1;
  }
  return nla_put(message_, attrtype, attrlen, data);
}

bool KernelBoundNlMessage::Send(NetlinkSocket *socket) {
  if (!socket) {
    LOG(ERROR) << "NULL |socket| parameter";
    return false;
  }
  if (!message_) {
    LOG(ERROR) << "NULL |message_|.";
    return -1;
  }

  // Manually set the sequence number -- seems to work.
  struct nlmsghdr *header = nlmsg_hdr(message_);
  if (header != 0) {
    header->nlmsg_seq = socket->GetSequenceNumber();
  }

  // Complete AND SEND a message.
  int result = nl_send_auto_complete(
      const_cast<struct nl_sock *>(socket->GetConstNlSock()), message_);

  SLOG(WiFi, 6) << "NL Message " << GetId() << " ===>";

  if (result < 0) {
    LOG(ERROR) << "Failed call to 'nl_send_auto_complete': " << result;
    return false;
  }
  return true;
}

}  // namespace shill.
