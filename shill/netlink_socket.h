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

enum nl_cb_kind;
enum nl_cb_type;
struct nl_msg;

namespace shill {

class Nl80211Message;

// Provides an abstraction to a netlink socket.  See
// http://www.infradead.org/~tgr/libnl/ for documentation on how netlink
// sockets work.
class NetlinkSocket {
 public:
  NetlinkSocket() : nl_sock_(NULL) {}
  virtual ~NetlinkSocket();

  // Non-trivial initialization.
  bool Init();

  // Disables sequence checking on the message stream.
  virtual bool DisableSequenceChecking();

  // Returns the file descriptor used by the socket.
  virtual int GetFd() const;

  // Get the next message sequence number for this socket.  Disallow zero so
  // that we can use that as the 'broadcast' sequence number.
  virtual uint32_t GetSequenceNumber();

  // Returns the value returned by the 'genl_ctrl_resolve' call.
  virtual int family_id() const = 0;

  // Returns the family name of the socket created by this type of object.
  virtual std::string GetSocketFamilyName() const = 0;

  // Sends a message, returns true if successful.
  virtual bool SendMessage(Nl80211Message *message);

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
