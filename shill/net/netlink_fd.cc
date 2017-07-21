//
// Copyright 2017 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include "shill/net/netlink_fd.h"

#include <linux/netlink.h>
#include <sys/socket.h>

#include <base/logging.h>

#include "shill/net/sockets.h"

namespace shill {

int OpenNetlinkSocketFD(Sockets* sockets,
                        int netlink_family,
                        int netlink_groups_mask) {
  int sockfd = sockets->Socket(PF_NETLINK, SOCK_DGRAM, netlink_family);
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

  if (sockets->Bind(sockfd,
                    reinterpret_cast<struct sockaddr*>(&addr),
                    sizeof(addr)) < 0) {
    sockets->Close(sockfd);
    LOG(ERROR) << "Netlink socket bind failed";
    return Sockets::kInvalidFileDescriptor;
  }

  return sockfd;
}

}  // namespace shill
