// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arc/network/neighbor_finder.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/icmp6.h>
#include <netinet/in.h>
#include <string.h>

#include <ndp.h>

#include <base/message_loop/message_loop.h>

#include "arc/network/net_util.h"

namespace {

const int kTimeoutMs = 1000;

}  // namespace

namespace arc_networkd {

bool NeighborFinder::Check(const std::string& ifname,
                           const struct in6_addr& addr,
                           const base::Callback<void(bool)>& callback) {
  int success;
  struct ndp_msg* msg;

  CHECK(!running_);
  memcpy(&check_addr_, &addr, sizeof(check_addr_));
  result_callback_ = callback;

  running_ = true;
  StartNdp(ifname, NDP_MSG_NA);
  base::MessageLoopForIO::current()->task_runner()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&NeighborFinder::Timeout, weak_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(kTimeoutMs));

  success = ndp_msg_new(&msg, NDP_MSG_NS);
  if (success < 0) {
    errno = -success;
    PLOG(WARNING) << "Failed to allocate NDP Neighbor Solicitation msg";
    return false;
  }

  ndp_msg_ifindex_set(msg, ifindex_);

  struct nd_neighbor_solicit** ns =
      reinterpret_cast<struct nd_neighbor_solicit**>(ndp_msgns(msg));
  memcpy(&(*ns)->nd_ns_target, &addr, sizeof((*ns)->nd_ns_target));

  success = ndp_msg_send(ndp_, msg);
  if (success < 0) {
    errno = -success;
    PLOG(WARNING) << "Error sending Neighbor Solicitation msg to "
                  << check_addr_;
  }

  ndp_msg_destroy(msg);

  return success == 0;
}

void NeighborFinder::Timeout() {
  if (!running_)
    return;

  VLOG(1) << "no answer for neighbor solicitation to " << check_addr_;

  running_ = false;
  StopNdp();
  result_callback_.Run(false);
}

int NeighborFinder::OnNdpMsg(struct ndp* ndp, struct ndp_msg* msg) {
  if (!running_)
    return 0;

  if (memcmp(&check_addr_, ndp_msg_addrto(msg), sizeof(check_addr_)) == 0) {
    VLOG(1) << "got answer for neighbor solicitation to " << check_addr_;

    running_ = false;
    StopNdp();
    result_callback_.Run(true);
  }
  return 0;
}

}  // namespace arc_networkd
