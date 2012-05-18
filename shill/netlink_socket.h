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

#ifndef SHILL_NETLINK_SOCKET_H_
#define SHILL_NETLINK_SOCKET_H_

#include <iomanip>
#include <string>

#include <asm/types.h>
#include <linux/nl80211.h>
#include <netlink/handlers.h>
#include <netlink/netlink.h>

#include <base/basictypes.h>
#include <base/bind.h>
#include <base/logging.h>

#include "shill/kernel_bound_nlmessage.h"

enum nl_cb_kind;
enum nl_cb_type;
struct nl_msg;

namespace shill {

// libnl 1.x compatibility code -- these functions provide libnl v2.x/v3.x
// interfaces for systems that only have v1.x libraries.
#if !defined(CONFIG_LIBNL20) && !defined(CONFIG_LIBNL30)
#define nl_sock nl_handle
static inline struct nl_handle *nl_socket_alloc(void) {
  return nl_handle_alloc();
}

static inline void nl_socket_free(struct nl_sock *h) {
  nl_handle_destroy(h);
}
#endif /* CONFIG_LIBNL20 && CONFIG_LIBNL30 */


// Provides an abstraction to a netlink socket.  See
// http://www.infradead.org/~tgr/libnl/ for documentation on how netlink
// sockets work.
class NetlinkSocket {
 public:
  // Provides a wrapper around the netlink callback.
  class Callback {
   public:
    Callback() : cb_(NULL) {}
    virtual ~Callback();

    // Non-trivial initialization.
    bool Init();

    // Very thin abstraction of nl_cb_err.  Takes the same parameters used by
    // 'nl_cb_err' except for the first parameter of 'nl_cb_err' (which is
    // filled in using the member variable |cb_|).
    bool ErrHandler(nl_cb_kind kind, nl_recvmsg_err_cb_t func, void *arg);

    // Very thin abstraction of nl_cb_set.  Takes the same parameters used by
    // 'nl_cb_set' except for the first parameter of 'nl_cb_set' (which is
    // filled in using the member variable |cb_|).
    bool SetHandler(nl_cb_type type, nl_cb_kind kind, nl_recvmsg_msg_cb_t func,
                    void *arg);

   private:
    friend class NetlinkSocket;  // Because GetMessagesUsingCallback needs cb().

    const struct nl_cb *cb() const { return cb_; }

    struct nl_cb *cb_;

    DISALLOW_COPY_AND_ASSIGN(Callback);
  };

  NetlinkSocket() : nl_sock_(NULL) {}
  virtual ~NetlinkSocket();

  // Non-trivial initialization.
  bool Init();

  // Disables sequence checking on the message stream.
  virtual bool DisableSequenceChecking();

  // Returns the file descriptor used by the socket.
  virtual int GetFd() const;

  // Receives one or more messages (perhaps a response to a previously sent
  // message) over the netlink socket.  The message(s) are handled with the
  // default callback (configured with 'SetNetlinkCallback').
  virtual bool GetMessages();

  // Receives one or more messages over the netlink socket.  The message(s)
  // are handled with the supplied callback (uses socket's default callback
  // function if NULL).
  virtual bool GetMessagesUsingCallback(NetlinkSocket::Callback *callback);

  virtual unsigned int GetSequenceNumber() {
    return nl_socket_use_seq(nl_sock_);
  }

  // This method is called |callback_function| to differentiate it from the
  // 'callback' method in KernelBoundNlMessage since they return different
  // types.
  virtual bool SetNetlinkCallback(nl_recvmsg_msg_cb_t on_netlink_data,
                                  void *callback_parameter);

  // Returns the family name of the socket created by this type of object.
  virtual std::string GetSocketFamilyName() const = 0;

  const struct nl_sock *GetConstNlSock() const { return nl_sock_; }

 protected:
  struct nl_sock *GetNlSock() { return nl_sock_; }

 private:
  // Netlink Callback function for nl80211.  Used to disable the sequence
  // checking on messages received from the netlink module.
  static int IgnoreSequenceCheck(nl_msg *ignored_msg, void *ignored_arg);

  struct nl_sock *nl_sock_;

  DISALLOW_COPY_AND_ASSIGN(NetlinkSocket);
};

}  // namespace shill

#endif  // SHILL_NETLINK_SOCKET_H_
