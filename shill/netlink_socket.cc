// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This code is derived from the 'iw' source code.  The copyright and license
// of that code is as follows:
//
// Copyright (c) 2007, 2008  Johannes Berg
// Copyright (c) 2007  Andy Lutomirski
// Copyright (c) 2007  Mike Kershaw
// Copyright (c) 2008-2009  Luis R. Rodriguez
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice appear in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
// WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
// ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
// WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
// ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
// OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

#include "shill/netlink_socket.h"

#include <ctype.h>
#include <errno.h>
#include <string.h>

#include <net/if.h>
#include <netlink/attr.h>
#include <netlink/genl/ctrl.h>
#include <netlink/genl/family.h>
#include <netlink/genl/genl.h>
#include <netlink/msg.h>
#include <netlink/netlink.h>

#include <iomanip>
#include <string>

#include <base/posix/eintr_wrapper.h>
#include <base/logging.h>
#include <base/stringprintf.h>

#include "shill/logging.h"
#include "shill/nl80211_message.h"

using base::StringAppendF;
using std::string;

namespace shill {

NetlinkSocket::~NetlinkSocket() {
  if (nl_sock_) {
    nl_socket_free(nl_sock_);
    nl_sock_ = NULL;
  }
}

bool NetlinkSocket::Init() {
  nl_sock_ = nl_socket_alloc();
  if (!nl_sock_) {
    LOG(ERROR) << "Failed to allocate netlink socket.";
    return false;
  }

  if (genl_connect(nl_sock_)) {
    LOG(ERROR) << "Failed to connect to generic netlink.";
    return false;
  }

  return true;
}

bool NetlinkSocket::SendMessage(Nl80211Message *message) {
  if (!message) {
    LOG(ERROR) << "NULL |message|.";
    return false;
  }

  if (!nl_sock_) {
    LOG(ERROR) << "Need to initialize the socket first.";
    return false;
  }

  ByteString out_msg = message->Encode(family_id());

  if (SLOG_IS_ON(WiFi, 6)) {
    SLOG(WiFi, 6) << "NL Message " << message->sequence_number() << " ===>";
    SLOG(WiFi, 6) << "  Sending (" << out_msg.GetLength() << " bytes) : "
              << message->GenericToString();

    const unsigned char *out_data = out_msg.GetConstData();
    string output;
    for (size_t i = 0; i < out_msg.GetLength(); ++i) {
      StringAppendF(&output, " %02x", out_data[i]);
    }
    SLOG(WiFi, 6) << output;
  }

  int result = HANDLE_EINTR(send(GetFd(), out_msg.GetConstData(),
                                 out_msg.GetLength(), 0));
  if (!result) {
    PLOG(ERROR) << "Send failed.";
    return false;
  }

  return true;
}


bool NetlinkSocket::DisableSequenceChecking() {
  if (!nl_sock_) {
    LOG(ERROR) << "NULL socket";
    return false;
  }

  // NOTE: can't use nl_socket_disable_seq_check(); it's not in this version
  // of the library.
  int result = nl_socket_modify_cb(nl_sock_, NL_CB_SEQ_CHECK, NL_CB_CUSTOM,
                                   NetlinkSocket::IgnoreSequenceCheck, NULL);
  if (result) {
    LOG(ERROR) << "Failed call to nl_socket_modify_cb: " << result;
    return false;
  }

  return true;
}

int NetlinkSocket::GetFd() const {
  if (!nl_sock_) {
    LOG(ERROR) << "NULL socket";
    return -1;
  }
  return nl_socket_get_fd(nl_sock_);
}

unsigned int NetlinkSocket::GetSequenceNumber() {
  unsigned int number = nl_socket_use_seq(nl_sock_);
  if (number == 0) {
    number = nl_socket_use_seq(nl_sock_);
  }
  if (number == 0) {
    LOG(WARNING) << "Couldn't get non-zero sequence number";
    number = 1;
  }
  return number;
}

int NetlinkSocket::IgnoreSequenceCheck(struct nl_msg *ignored_msg,
                                       void *ignored_arg) {
  return NL_OK;  // Proceed.
}

}  // namespace shill.
