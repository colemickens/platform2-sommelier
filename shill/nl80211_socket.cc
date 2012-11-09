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

#include "shill/nl80211_socket.h"

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
#include <sstream>
#include <string>

#include "shill/kernel_bound_nlmessage.h"
#include "shill/logging.h"
#include "shill/netlink_socket.h"
#include "shill/user_bound_nlmessage.h"

using std::string;

namespace shill {

const char Nl80211Socket::kSocketFamilyName[] = "nl80211";

bool Nl80211Socket::Init() {
  if (!NetlinkSocket::Init()) {
    LOG(ERROR) << "NetlinkSocket didn't initialize.";
    return false;
  }

  nl80211_id_ = genl_ctrl_resolve(GetNlSock(), "nlctrl");
  if (nl80211_id_ < 0) {
    LOG(ERROR) << "nl80211 not found.";
    return false;
  }

  LOG(INFO) << "Nl80211Socket initialized successfully";
  return true;
}

bool Nl80211Socket::AddGroupMembership(const string &group_name) {
  int id = genl_ctrl_resolve_grp(GetNlSock(),
                                 kSocketFamilyName,
                                 group_name.c_str());
  if (id < 0) {
    LOG(ERROR) << "No Id for group " << group_name;
    return false;
  }
  int result = nl_socket_add_membership(GetNlSock(), id);
  if (result != 0) {
    LOG(ERROR) << "Failed to join netlink multicast group: " << result;
    return false;
  }
  LOG(INFO) << " Group " << group_name << " added successfully";
  return true;
}

uint32 Nl80211Socket::Send(KernelBoundNlMessage *message) {
  CHECK(message);
  return NetlinkSocket::Send(message->message(),
                             message->command(),
                             GetFamilyId());
}

}  // namespace shill
