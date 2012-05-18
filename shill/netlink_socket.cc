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

#include <net/if.h>
#include <netlink/attr.h>
#include <netlink/genl/ctrl.h>
#include <netlink/genl/family.h>
#include <netlink/genl/genl.h>
#include <netlink/msg.h>
#include <netlink/netlink.h>

#include <iomanip>

#include <base/logging.h>

#include "shill/kernel_bound_nlmessage.h"
#include "shill/scope_logger.h"

namespace shill {

//
// NetlinkSocket::Callback.
//

NetlinkSocket::Callback::~Callback() {
  if (cb_) {
    nl_cb_put(cb_);
    cb_ = NULL;
  }
}

bool NetlinkSocket::Callback::Init() {
  cb_ = nl_cb_alloc(NL_CB_DEFAULT);
  if (!cb_) {
    LOG(ERROR) << "NULL cb_";
    return false;
  }
  return true;
}

bool NetlinkSocket::Callback::ErrHandler(enum nl_cb_kind kind,
                                         nl_recvmsg_err_cb_t func,
                                         void *arg) {
  int result = nl_cb_err(cb_, kind, func, arg);
  if (result) {
    LOG(ERROR) << "nl_cb_err returned " << result;
    return false;
  }
  return true;
}

bool NetlinkSocket::Callback::SetHandler(enum nl_cb_type type,
                                         enum nl_cb_kind kind,
                                         nl_recvmsg_msg_cb_t func,
                                         void *arg) {
  int result = nl_cb_set(cb_, type, kind, func, arg);
  if (result) {
    LOG(ERROR) << "nl_cb_set returned " << result;
    return false;
  }
  return true;
}

//
// NetlinkSocket.
//

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

bool NetlinkSocket::GetMessages() {
  // TODO(wdg): make this non-blocking.
  // Blocks until a message is available.  When that happens, the message is
  // read and passed to the default callback (i.e., the one set with
  // NetlinkSocket::SetNetlinkCallback).
  int result = nl_recvmsgs_default(nl_sock_);
  if (result < 0) {
    LOG(ERROR) << "Failed call to nl_recvmsgs_default: " << result;
    return false;
  }
  return true;
}

bool NetlinkSocket::GetMessagesUsingCallback(
    NetlinkSocket::Callback *on_netlink_data) {
  if (!on_netlink_data || !on_netlink_data->cb_)
    return GetMessages();  // Default to generic callback.

  int result = nl_recvmsgs(nl_sock_, on_netlink_data->cb_);
  if (result < 0) {
    LOG(ERROR) << "Failed call to nl_recvmsgs: " << result;
    return false;
  }
  return true;
}

bool NetlinkSocket::SetNetlinkCallback(nl_recvmsg_msg_cb_t on_netlink_data,
                                       void *callback_parameter) {
  if (!nl_sock_) {
    LOG(ERROR) << "NULL socket";
    return false;
  }

  int result = nl_socket_modify_cb(nl_sock_, NL_CB_VALID, NL_CB_CUSTOM,
                                   on_netlink_data, callback_parameter);
  if (result) {
    LOG(ERROR) << "nl_socket_modify_cb returned " << result;
    return false;
  }
  return true;
}

int NetlinkSocket::IgnoreSequenceCheck(struct nl_msg *ignored_msg,
                                       void *ignored_arg) {
  return NL_OK;  // Proceed.
}

}  // namespace shill.
