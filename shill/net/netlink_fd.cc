// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/net/netlink_fd.h"

#include <linux/netlink.h>
#include <sys/socket.h>

#include <base/logging.h>

#include "shill/net/sockets.h"

namespace shill {

int OpenNetlinkSocketFD(Sockets* sockets,
                        int netlink_family,
                        int netlink_groups_mask) {
  int sockfd =
      sockets->Socket(PF_NETLINK, SOCK_DGRAM | SOCK_CLOEXEC, netlink_family);
  if (sockfd < 0) {
    LOG(ERROR) << "Failed to open netlink socket";
    return Sockets::kInvalidFileDescriptor;
  }

  if (sockets->SetReceiveBuffer(sockfd, kNetlinkReceiveBufferSize))
    LOG(ERROR) << "Failed to increase receive buffer size";

  struct sockaddr_nl addr;
  memset(&addr, 0, sizeof(addr));
  addr.nl_family = AF_NETLINK;
  addr.nl_groups = netlink_groups_mask;

  if (sockets->Bind(sockfd, reinterpret_cast<struct sockaddr*>(&addr),
                    sizeof(addr)) < 0) {
    sockets->Close(sockfd);
    LOG(ERROR) << "Netlink socket bind failed";
    return Sockets::kInvalidFileDescriptor;
  }

  return sockfd;
}

}  // namespace shill
