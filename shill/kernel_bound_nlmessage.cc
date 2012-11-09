// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/kernel_bound_nlmessage.h"

#include <netlink/msg.h>

#include "shill/logging.h"

namespace shill {

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

uint32 KernelBoundNlMessage::sequence_number() const {
  if (message_ && nlmsg_hdr(message_)) {
    return nlmsg_hdr(message_)->nlmsg_seq;
  }
  return 0;
}

}  // namespace shill.
