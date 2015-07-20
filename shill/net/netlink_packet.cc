// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/net/netlink_packet.h"

#include <algorithm>

#include <base/logging.h>

#include "shill/net/byte_string.h"

namespace shill {

NetlinkPacket::NetlinkPacket(const unsigned char* buf, size_t len)
    : consumed_bytes_(0) {
  if (!buf || len < sizeof(header_)) {
    LOG(ERROR) << "Cannot retrieve header.";
    return;
  }

  memcpy(&header_, buf, sizeof(header_));
  if (len < header_.nlmsg_len || header_.nlmsg_len < sizeof(header_)) {
    LOG(ERROR) << "Discarding incomplete / invalid message.";
    return;
  }

  payload_.reset(
      new ByteString(buf + sizeof(header_), len - sizeof(header_)));
}

NetlinkPacket::~NetlinkPacket() {
}

bool NetlinkPacket::IsValid() const {
  return payload_ != nullptr;
}

size_t NetlinkPacket::GetLength() const {
  return IsValid() ? header_.nlmsg_len : 0;
}

uint16_t NetlinkPacket::GetMessageType() const {
  return header_.nlmsg_type;
}

uint16_t NetlinkPacket::GetMessageSequence() const {
  return header_.nlmsg_seq;
}

size_t NetlinkPacket::GetRemainingLength() const {
  return payload_->GetLength() - consumed_bytes_;
}

const ByteString &NetlinkPacket::GetPayload() const {
  CHECK(IsValid());
  return *payload_.get();
}

bool NetlinkPacket::ConsumeData(size_t len, void* data) {
  if (GetRemainingLength() < len) {
    LOG(ERROR) << "Not enough bytes remaining.";
    return false;
  }

  memcpy(data, payload_->GetData() + consumed_bytes_, len);
  consumed_bytes_ = std::min(payload_->GetLength(),
                             consumed_bytes_ + NLMSG_ALIGN(len));
  return true;
}

bool NetlinkPacket::GetGenlMsgHdr(genlmsghdr *header) const {
  if (!IsValid() || payload_->GetLength() < sizeof(*header)) {
    return false;
  }
  memcpy(header, payload_->GetConstData(), sizeof(*header));
  return true;
}

}  // namespace shill.
