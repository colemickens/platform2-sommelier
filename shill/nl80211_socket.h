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

#ifndef SHILL_NL80211_SOCKET_H_
#define SHILL_NL80211_SOCKET_H_

#include <iomanip>
#include <string>

#include <linux/nl80211.h>

#include <base/basictypes.h>
#include <base/bind.h>

#include "shill/netlink_socket.h"

struct nl_msg;
struct sockaddr_nl;
struct nlmsgerr;

namespace shill {

// Provides a mechanism to communicate with the cfg80211 and mac80211 modules
// utilizing a netlink socket.
class Nl80211Socket : public NetlinkSocket {
 public:
  Nl80211Socket() : nl80211_id_(-1) {}
  virtual ~Nl80211Socket() {}

  // Perform non-trivial initialization.
  bool Init();

  // Add ourself to the multicast group that gets sent messages of the
  // specificed type.  Legal |group_name| character strings are defined by
  // cfg80211 module but they include "config", "scan", "regulatory", and
  // "mlme".
  virtual bool AddGroupMembership(const std::string &group_name);

  // Returns the value returned by the 'genl_ctrl_resolve' call.
  int GetFamilyId() const { return nl80211_id_; }

  // Gets the name of the socket family.
  std::string GetSocketFamilyName() const {
    return Nl80211Socket::kSocketFamilyName;
  }

 private:
  struct HandlerArgs {
    HandlerArgs() : group(NULL), id(0) {}
    HandlerArgs(const char *group_arg, int id_arg) :
      group(group_arg), id(id_arg) {}
    const char *group;
    int id;
  };

  // Contains 'nl80211', the family name of the netlink socket.
  static const char kSocketFamilyName[];

  // Method called by cfg80211 to acknowledge messages sent to cfg80211.
  static int OnAck(nl_msg *unused_msg, void *arg);

  // Method called by cfg80211 for message errors.
  static int OnError(sockaddr_nl *unused_nla, nlmsgerr *err, void *arg);

  // Netlink Callback for handling response to 'CTRL_CMD_GETFAMILY' message.
  static int OnFamilyResponse(nl_msg *msg, void *arg);

  // Gets an ID for a specified type of multicast messages sent from the
  // cfg80211 module.
  virtual int GetMulticastGroupId(const std::string &group);

  // The id returned by a call to 'genl_ctrl_resolve'.
  int nl80211_id_;

  DISALLOW_COPY_AND_ASSIGN(Nl80211Socket);
};


}  // namespace shill

#endif  // SHILL_NL80211_SOCKET_H_
