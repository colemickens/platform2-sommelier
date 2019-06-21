// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arc/network/router_finder.h"

#include <errno.h>
#include <stdint.h>
#include <string.h>

#include <ndp.h>

#include <base/logging.h>
#include <base/time/time.h>

#include "arc/network/arc_ip_config.h"

namespace {

const int kInitialRtrSolicitationIntervalMs = 4000;
const int kRtrSolicitationIntervalMs = 4000;
const int kMaxRtrSolicitations = 3;

bool ArePrefixesEqual(const struct in6_addr& addr1,
                      int len1,
                      const struct in6_addr& addr2,
                      int len2) {
  return len1 == len2 && memcmp(&addr1, &addr2, sizeof(addr1)) == 0;
}
}  // namespace

namespace arc_networkd {

RouterFinder::RouterFinder() = default;
RouterFinder::~RouterFinder() = default;

bool RouterFinder::Start(
    const std::string& ifname,
    const base::Callback<void(const struct in6_addr& prefix,
                              int prefix_len,
                              const struct in6_addr& router)>& callback) {
  result_callback_ = callback;

  have_prefix_ = false;
  prefix_ = IN6ADDR_ANY_INIT;
  prefix_len_ = 0;
  ifname_ = ifname;

  // FIXME: This magic delay is needed or else the sendto() may return
  // EADDRNOTAVAIL.  Figure out why.
  rs_attempts_ = 0;
  base::MessageLoopForIO::current()->task_runner()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&RouterFinder::CheckForRouter, weak_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(kInitialRtrSolicitationIntervalMs));

  return true;
}

void RouterFinder::CheckForRouter() {
  if (have_prefix_)
    return;

  rs_attempts_++;
  if (rs_attempts_ > kMaxRtrSolicitations) {
    LOG(INFO) << "No IPv6 router found on " << ifname_;
    return;
  }

  if (!ndp_)
    StartNdp(ifname_, NDP_MSG_RA);
  SendRouterSolicitation();

  base::MessageLoopForIO::current()->task_runner()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&RouterFinder::CheckForRouter, weak_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(kRtrSolicitationIntervalMs));
}

void RouterFinder::SendRouterSolicitation() {
  int success;
  struct ndp_msg* msg;

  success = ndp_msg_new(&msg, NDP_MSG_RS);
  if (success < 0) {
    errno = -success;
    PLOG(WARNING) << "Failed to allocate RS msg for NDP receiver on "
                  << ifname_;
    return;
  }

  ndp_msg_ifindex_set(msg, ifindex_);
  success = ndp_msg_send(ndp_, msg);
  if (success < 0) {
    errno = -success;
    PLOG(WARNING) << "Error sending RS msg for NDP receiver on " << ifname_;
  }
  ndp_msg_destroy(msg);
}

int RouterFinder::OnNdpMsg(struct ndp* ndp, struct ndp_msg* msg) {
  enum ndp_msg_type msg_type = ndp_msg_type(msg);
  if (msg_type != NDP_MSG_RA) {
    LOG(WARNING) << "Unexpected message type " << MsgTypeName(msg_type)
                 << " for NDP receiver on " << ifname_;
    return -1;
  }

  int offset;
  struct in6_addr* router = ndp_msg_addrto(msg);
  if (!router) {
    return 0;
  }

  // TODO(cernekee): Validate RA fields per the RFC.
  // (I think some of this happens in libndp, although our version
  // might be out of date.)

  ndp_msg_opt_for_each_offset(offset, msg, NDP_MSG_OPT_PREFIX) {
    struct in6_addr* prefix = ndp_msg_opt_prefix(msg, offset);
    if (!prefix) {
      continue;
    }

    int prefix_len = ndp_msg_opt_prefix_len(msg, offset);
    uint32_t valid_time = ndp_msg_opt_prefix_valid_time(msg, offset);

    // TODO(cernekee): handle expiration and other special cases.
    // For now just use any prefix found.
    if (valid_time &&
        (!have_prefix_ ||
         !ArePrefixesEqual(prefix_, prefix_len_, *prefix, prefix_len))) {
      AssignPrefix(*prefix, prefix_len, *router);
      break;
    }

    if (!valid_time) {
      AssignPrefix(IN6ADDR_ANY_INIT, 0, IN6ADDR_ANY_INIT);
      break;
    }
  }

  return 0;
}

void RouterFinder::AssignPrefix(const struct in6_addr& prefix,
                                int prefix_len,
                                const struct in6_addr& router) {
  memcpy(&prefix_, &prefix, sizeof(prefix));
  prefix_len_ = prefix_len;
  have_prefix_ = (prefix_len != 0);
  result_callback_.Run(prefix_, prefix_len_, router);
}
}  // namespace arc_networkd
