// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/ether.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <fcntl.h>
#include <string>

#include <base/callback_old.h>
#include <base/logging.h>
#include <base/memory/scoped_ptr.h>
#include <base/memory/singleton.h>

#include "shill/io_handler.h"
#include "shill/rtnl_handler.h"
#include "shill/rtnl_listener.h"
#include "shill/shill_event.h"

using std::string;

namespace shill {

RTNLHandler::RTNLHandler()
    : running_(false),
      in_request_(false),
      rtnl_callback_(NewCallback(this, &RTNLHandler::ParseRTNL)),
      rtnl_socket_(-1),
      request_flags_(0),
      request_sequence_(0) {
  VLOG(2) << "RTNLHandler created";
}

RTNLHandler::~RTNLHandler() {
  VLOG(2) << "RTNLHandler removed";
  Stop();
}

RTNLHandler* RTNLHandler::GetInstance() {
  return Singleton<RTNLHandler>::get();
}

void RTNLHandler::Start(EventDispatcher *dispatcher) {
  struct sockaddr_nl addr;

  if (running_)
    return;

  rtnl_socket_ = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_ROUTE);
  if (rtnl_socket_ < 0) {
    LOG(ERROR) << "Failed to open rtnl socket";
    return;
  }

  memset(&addr, 0, sizeof(addr));
  addr.nl_family = AF_NETLINK;
  addr.nl_groups = RTMGRP_LINK | RTMGRP_IPV4_IFADDR | RTMGRP_IPV4_ROUTE;

  if (bind(rtnl_socket_, reinterpret_cast<struct sockaddr *>(&addr),
           sizeof(addr)) < 0) {
    close(rtnl_socket_);
    rtnl_socket_ = -1;
    LOG(ERROR) << "RTNL socket bind failed";
    return;
  }

  rtnl_handler_.reset(dispatcher->CreateInputHandler(rtnl_socket_,
                                                     rtnl_callback_.get()));
  running_ = true;

  NextRequest(request_sequence_);

  VLOG(2) << "RTNLHandler started";
  VLOG(2) << "RTNLHandler started";
}

void RTNLHandler::Stop() {
  if (!running_)
    return;

  rtnl_handler_.reset(NULL);
  close(rtnl_socket_);
  running_ = false;
  in_request_ = false;
  VLOG(2) << "RTNLHandler stopped";
}

void RTNLHandler::AddListener(RTNLListener *to_add) {
  std::vector<RTNLListener *>::iterator it;
  for (it = listeners_.begin(); it != listeners_.end(); ++it) {
    if (to_add == *it)
      return;
  }
  listeners_.push_back(to_add);
  VLOG(2) << "RTNLHandler added listener";
}

void RTNLHandler::RemoveListener(RTNLListener *to_remove) {
  std::vector<RTNLListener *>::iterator it;
  for (it = listeners_.begin(); it != listeners_.end(); ++it) {
    if (to_remove == *it) {
      listeners_.erase(it);
      return;
    }
  }
  VLOG(2) << "RTNLHandler removed listener";
}

void RTNLHandler::SetInterfaceFlags(int interface_index, unsigned int flags,
                                   unsigned int change) {
  struct rtnl_request {
    struct nlmsghdr hdr;
    struct ifinfomsg msg;
  } req;

  request_sequence_++;
  memset(&req, 0, sizeof(req));

  req.hdr.nlmsg_len = sizeof(req);
  req.hdr.nlmsg_flags = NLM_F_REQUEST;
  req.hdr.nlmsg_pid = 0;
  req.hdr.nlmsg_seq = request_sequence_;
  req.hdr.nlmsg_type = RTM_NEWLINK;
  req.msg.ifi_index = interface_index;
  req.msg.ifi_flags = flags;
  req.msg.ifi_change = change;

  if (send(rtnl_socket_, &req, sizeof(req), 0) < 0) {
    LOG(ERROR) << "RTNL sendto failed: " << strerror(errno);
  }
}

void RTNLHandler::RequestDump(int request_flags) {
  request_flags_ |= request_flags;

  VLOG(2) << "RTNLHandler got request to dump " << request_flags;

  if (!in_request_ && running_)
    NextRequest(request_sequence_);
}

void RTNLHandler::DispatchEvent(int type, struct nlmsghdr *hdr) {
  std::vector<RTNLListener *>::iterator it;
  for (it = listeners_.begin(); it != listeners_.end(); ++it) {
    (*it)->NotifyEvent(type, hdr);
  }
}

void RTNLHandler::NextRequest(uint32_t seq) {
  struct rtnl_request {
    struct nlmsghdr hdr;
    struct rtgenmsg msg;
  } req;
  struct sockaddr_nl addr;
  int flag = 0;

  VLOG(2) << "RTNLHandler nextrequest " << seq << " " << request_sequence_ <<
    " " << request_flags_;

  if (seq != request_sequence_)
    return;

  request_sequence_++;
  memset(&req, 0, sizeof(req));

  req.hdr.nlmsg_len = sizeof(req);
  req.hdr.nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
  req.hdr.nlmsg_pid = 0;
  req.hdr.nlmsg_seq = request_sequence_;
  req.msg.rtgen_family = AF_INET;

  if ((request_flags_ & kRequestLink) != 0) {
    req.hdr.nlmsg_type = RTM_GETLINK;
    flag = kRequestLink;
  } else if ((request_flags_ & kRequestAddr) != 0) {
    req.hdr.nlmsg_type = RTM_GETADDR;
    flag = kRequestAddr;
  } else if ((request_flags_ & kRequestRoute) != 0) {
    req.hdr.nlmsg_type = RTM_GETROUTE;
    flag = kRequestRoute;
  } else {
    in_request_ = false;
    return;
  }

  memset(&addr, 0, sizeof(addr));
  addr.nl_family = AF_NETLINK;

  if (sendto(rtnl_socket_, &req, sizeof(req), 0,
             reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)) < 0) {
    LOG(ERROR) << "RTNL sendto failed";
    return;
  }
  request_flags_ &= ~flag;
  in_request_ = true;
}

void RTNLHandler::ParseRTNL(InputData *data) {
  unsigned char *buf = data->buf;
  unsigned char *end = buf + data->len;

  while (buf < end) {
    struct nlmsghdr *hdr = reinterpret_cast<struct nlmsghdr *>(buf);
    struct nlmsgerr *err;

    if (!NLMSG_OK(hdr, end - buf))
      break;

    switch (hdr->nlmsg_type) {
    case NLMSG_NOOP:
    case NLMSG_OVERRUN:
      return;
    case NLMSG_DONE:
      NextRequest(hdr->nlmsg_seq);
      return;
    case NLMSG_ERROR:
      err = reinterpret_cast<nlmsgerr *>(NLMSG_DATA(hdr));
      LOG(ERROR) << "error " << -err->error << " (" << strerror(-err->error) <<
                 ")";
      return;
    case RTM_NEWLINK:
    case RTM_DELLINK:
      DispatchEvent(kRequestLink, hdr);
      break;
    case RTM_NEWADDR:
    case RTM_DELADDR:
      DispatchEvent(kRequestAddr, hdr);
      break;
    case RTM_NEWROUTE:
    case RTM_DELROUTE:
      DispatchEvent(kRequestRoute, hdr);
      break;
    }

    buf += hdr->nlmsg_len;
  }
}

}  // namespace shill
