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

#include <base/logging.h>

#include "shill/kernel_bound_nlmessage.h"
#include "shill/logging.h"
#include "shill/netlink_socket.h"
#include "shill/scope_logger.h"
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

  return true;
}

bool Nl80211Socket::AddGroupMembership(const string &group_name) {
  int id = GetMulticastGroupId(group_name);
  if (id < 0) {
    LOG(ERROR) << "No Id for group " << group_name;
    return false;
  } else {
    int result = nl_socket_add_membership(GetNlSock(), id);
    if (result != 0) {
      LOG(ERROR) << "Failed call to 'nl_socket_add_membership': " << result;
      return false;
    }
  }
  return true;
}

int Nl80211Socket::OnAck(struct nl_msg *unused_msg, void *arg) {
  if (arg) {
    int *ret = reinterpret_cast<int *>(arg);
    *ret = 0;
  }
  return NL_STOP;  // Stop parsing and discard remainder of buffer.
}

int Nl80211Socket::OnError(struct sockaddr_nl *unused_nla, struct nlmsgerr *err,
                           void *arg) {
  int *ret = reinterpret_cast<int *>(arg);
  if (err) {
    if (ret)
      *ret = err->error;
    LOG(ERROR) << "Error(" << err->error << ") " << strerror(err->error);
  } else {
    if (ret)
      *ret = -1;
    LOG(ERROR) << "Error(<unknown>)";
  }
  return NL_STOP;  // Stop parsing and discard remainder of buffer.
}

int Nl80211Socket::OnFamilyResponse(struct nl_msg *msg, void *arg) {
  if (!msg) {
    LOG(ERROR) << "NULL |msg| parameter";
    return NL_SKIP;  // Skip current message, continue parsing buffer.
  }

  if (!arg) {
    LOG(ERROR) << "NULL |arg| parameter";
    return NL_SKIP;  // Skip current message, continue parsing buffer.
  }

  struct HandlerArgs *grp = reinterpret_cast<struct HandlerArgs *>(arg);
  struct nlattr *tb[CTRL_ATTR_MAX + 1];
  struct genlmsghdr *gnlh = reinterpret_cast<struct genlmsghdr *>(
      nlmsg_data(nlmsg_hdr(msg)));
  struct nlattr *mcgrp = NULL;
  int rem_mcgrp;

  nla_parse(tb, CTRL_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
            genlmsg_attrlen(gnlh, 0), NULL);

  if (!tb[CTRL_ATTR_MCAST_GROUPS])
    return NL_SKIP;  // Skip current message, continue parsing buffer.

  // nla_for_each_nested(...)
  mcgrp = reinterpret_cast<nlattr *>(nla_data(tb[CTRL_ATTR_MCAST_GROUPS]));
  rem_mcgrp = nla_len(tb[CTRL_ATTR_MCAST_GROUPS]);
  bool found_one = false;
  for (; nla_ok(mcgrp, rem_mcgrp); mcgrp = nla_next(mcgrp, &(rem_mcgrp))) {
    struct nlattr *tb_mcgrp[CTRL_ATTR_MCAST_GRP_MAX + 1];

    nla_parse(tb_mcgrp, CTRL_ATTR_MCAST_GRP_MAX,
        reinterpret_cast<nlattr *>(nla_data(mcgrp)), nla_len(mcgrp), NULL);

    if (!tb_mcgrp[CTRL_ATTR_MCAST_GRP_NAME]) {
      LOG(ERROR) << "No group name in 'group' message";
      continue;
    } else if (!tb_mcgrp[CTRL_ATTR_MCAST_GRP_ID]) {
      LOG(ERROR) << "No group id in 'group' message";
      continue;
    }

    if (strncmp(reinterpret_cast<char *>(
        nla_data(tb_mcgrp[CTRL_ATTR_MCAST_GRP_NAME])),
          grp->group, nla_len(tb_mcgrp[CTRL_ATTR_MCAST_GRP_NAME]))) {
      continue;
    }
    found_one = true;
    grp->id = nla_get_u32(tb_mcgrp[CTRL_ATTR_MCAST_GRP_ID]);
    SLOG(WiFi, 6) << "GROUP '"
                  << reinterpret_cast<char *>(
                      nla_data(tb_mcgrp[CTRL_ATTR_MCAST_GRP_NAME]))
                  << "' has ID "
                  << grp->id;
    break;
  }
  if (!found_one) {
    LOG(ERROR) << "NO GROUP matched '"
               << grp->group
               << "', the one for which we were looking";
  }

  return NL_SKIP;  // Skip current message, continue parsing buffer.
}

int Nl80211Socket::GetMulticastGroupId(const string &group) {
  // Allocate and build the message.
  KernelBoundNlMessage message;
  if (!message.Init()) {
    LOG(ERROR) << "Couldn't initialize message";
    return -1;
  }

  if (!message.AddNetlinkHeader(NL_AUTO_PID, NL_AUTO_SEQ, GetFamilyId(), 0, 0,
                                CTRL_CMD_GETFAMILY, 0))
    return -1;

  int result = message.AddAttribute(CTRL_ATTR_FAMILY_NAME,
                                    GetSocketFamilyName().length() + 1,
                                    GetSocketFamilyName().c_str());
  if (result < 0) {
    LOG(ERROR) << "nla_put return error: " << result;
    return -1;
  }

  if (!message.Send(this)) {
    return -1;
  }

  // Wait for the response.

  NetlinkSocket::Callback netlink_callback;
  struct HandlerArgs grp(group.c_str(), -ENOENT);
  int status = 1;

  if (netlink_callback.Init()) {
    if (!netlink_callback.ErrHandler(NL_CB_CUSTOM, OnError, &status)) {
      return -1;
    }
    if (!netlink_callback.SetHandler(NL_CB_ACK, NL_CB_CUSTOM, OnAck, &status)) {
      return -1;
    }
    if (!netlink_callback.SetHandler(NL_CB_VALID, NL_CB_CUSTOM,
                                     OnFamilyResponse, &grp)) {
      return -1;
    }
  } else {
    LOG(ERROR) << "Couldn't initialize callback";
    return -1;
  }

  while (status > 0) {  // 'status' is set by the NL_CB_ACK handler.
    if (!GetMessagesUsingCallback(&netlink_callback)) {
      return -1;
    }
  }

  if (status != 0) {
    return -1;
  }

  return grp.id;
}

}  // namespace shill
