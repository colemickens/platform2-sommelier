// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arc/network/ndp_handler.h"

#include <net/if.h>

#include <base/logging.h>

namespace {
constexpr const char kNdRouterSolicit[] = "ND_ROUTER_SOLICIT";
constexpr const char kNdRouterAdvert[] = "ND_ROUTER_ADVERT";
constexpr const char kNdNeighborSolicit[] = "ND_NEIGHBOR_SOLICIT";
constexpr const char kNdNeighborAdvert[] = "ND_NEIGHBOR_ADVERT";
constexpr const char kNdRedirect[] = "ND_REDIRECT";
}  // namespace

namespace arc_networkd {

NdpHandler::NdpHandler() : watcher_(FROM_HERE) {}

NdpHandler::~NdpHandler() {
  StopNdp();
}

bool NdpHandler::StartNdp(const std::string& ifname,
                          enum ndp_msg_type msg_type) {
  CHECK(!ndp_);

  msg_type_ = msg_type;
  ifindex_ = if_nametoindex(ifname.c_str());
  if (!ifindex_) {
    LOG(WARNING) << "Can't find ifindex for " << ifname;
    return false;
  }

  CHECK_EQ(ndp_open(&ndp_), 0);

  if (ndp_msgrcv_handler_register(ndp_, &NdpHandler::LibNdpCallback, msg_type_,
                                  ifindex_, this)) {
    LOG(WARNING) << "Can't register NDP receiver " << MsgTypeName(msg_type_)
                 << " for " << ifname;
    ndp_close(ndp_);
    ndp_ = nullptr;
    return false;
  }

  fd_ = ndp_get_eventfd(ndp_);
  MessageLoopForIO::current()->WatchFileDescriptor(
      fd_, true, MessageLoopForIO::WATCH_READ, &watcher_, this);

  VLOG(1) << "NDP receiver " << MsgTypeName(msg_type_) << " started for "
          << ifname;

  return true;
}

void NdpHandler::StopNdp() {
  if (ndp_) {
    watcher_.StopWatchingFileDescriptor();
    ndp_msgrcv_handler_unregister(ndp_, &NdpHandler::LibNdpCallback, msg_type_,
                                  ifindex_, this);
    ndp_close(ndp_);
    ndp_ = nullptr;

    char ifname[IF_NAMESIZE] = "'unknown'";
    if_indextoname(ifindex_, ifname);
    VLOG(1) << "NDP receiver " << MsgTypeName(msg_type_) << " stopped for "
            << ifname;
  }
}

void NdpHandler::OnFileCanReadWithoutBlocking(int fd) {
  CHECK_EQ(fd_, fd);
  if (ndp_call_eventfd_handler(ndp_))
    LOG(WARNING) << "NDP event handler failed"
                 << " for " << MsgTypeName(msg_type_);
}

// static
int NdpHandler::LibNdpCallback(struct ndp* ndp,
                               struct ndp_msg* msg,
                               void* priv) {
  NdpHandler* that = reinterpret_cast<NdpHandler*>(priv);
  return that->OnNdpMsg(ndp, msg);
}

// static
const char* NdpHandler::MsgTypeName(enum ndp_msg_type msg_type) {
  switch (msg_type) {
    case NDP_MSG_RS:
      return kNdRouterSolicit;
    case NDP_MSG_RA:
      return kNdRouterAdvert;
    case NDP_MSG_NS:
      return kNdNeighborSolicit;
    case NDP_MSG_NA:
      return kNdNeighborAdvert;
    case NDP_MSG_R:
      return kNdRedirect;
    default:
      return "unknown NDP msg_type";
  }
}

}  // namespace arc_networkd
