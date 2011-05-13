// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
#include <string>

#include <base/callback_old.h>
#include <base/hash_tables.h>
#include <base/logging.h>
#include <base/memory/scoped_ptr.h>

#include "shill/control_interface.h"
#include "shill/device_info.h"
#include "shill/service.h"

using std::string;

namespace shill {
DeviceInfo::DeviceInfo(EventDispatcher *dispatcher)
  : running_(false),
    dispatcher_(dispatcher),
    rtnl_callback_(NewCallback(this, &DeviceInfo::ParseRTNL)),
    rtnl_socket_(-1),
    request_flags_(0),
    request_sequence_(0) {
  LOG(INFO) << "DeviceInfo initialized";
}

DeviceInfo::~DeviceInfo() {
  Stop();
}

void DeviceInfo::Start()
{
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

  if (bind(rtnl_socket_, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
    close(rtnl_socket_);
    rtnl_socket_ = -1;
    LOG(ERROR) << "RTNL socket bind failed";
    return;
  }

  rtnl_handler_.reset(dispatcher_->CreateInputHandler(rtnl_socket_,
                                                      rtnl_callback_.get()));
  running_ = true;

  request_flags_ = REQUEST_LINK | REQUEST_ADDR | REQUEST_ROUTE;
  NextRequest(request_sequence_);

  LOG(INFO) << "DeviceInfo started";
}

void DeviceInfo::Stop()
{
  if (!running_)
    return;

  rtnl_handler_.reset(NULL);
  close(rtnl_socket_);
  running_ = false;
}

void DeviceInfo::NextRequest(uint32_t seq) {
  struct rtnl_request {
    struct nlmsghdr hdr;
    struct rtgenmsg msg;
  } req;
  struct sockaddr_nl addr;
  int flag = 0;

  LOG(INFO) << __func__ << ": " << seq << " :: " << request_sequence_;

  if (seq != request_sequence_)
    return;

  request_sequence_++;
  memset(&req, 0, sizeof(req));

  req.hdr.nlmsg_len = sizeof(req);
  req.hdr.nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
  req.hdr.nlmsg_pid = 0;
  req.hdr.nlmsg_seq = request_sequence_;
  req.msg.rtgen_family = AF_INET;

  if ((request_flags_ & REQUEST_LINK) != 0) {
    req.hdr.nlmsg_type = RTM_GETLINK;
    flag = REQUEST_LINK;
  } else if ((request_flags_ & REQUEST_ADDR) != 0) {
    req.hdr.nlmsg_type = RTM_GETADDR;
    flag = REQUEST_ADDR;
  } else if ((request_flags_ & REQUEST_ROUTE) != 0) {
    req.hdr.nlmsg_type = RTM_GETROUTE;
    flag = REQUEST_ROUTE;
  } else
    return;

  memset(&addr, 0, sizeof(addr));
  addr.nl_family = AF_NETLINK;

  if (sendto(rtnl_socket_, &req, sizeof(req), 0,
             (struct sockaddr *) &addr, sizeof(addr)) < 0) {
    LOG(ERROR) << "RTNL sendto failed";
    return;
  }
  request_flags_ &= ~flag;
}


void DeviceInfo::LinkMsg(unsigned char *buf, bool del) {
  struct nlmsghdr *hdr = (struct nlmsghdr *) buf;
  struct ifinfomsg *msg = (struct ifinfomsg *) NLMSG_DATA(hdr);
  int bytes = IFLA_PAYLOAD(hdr);

  LOG(INFO) << "index "  << msg->ifi_index << " flags " << msg->ifi_flags <<
    ": del = " << del;
}

void DeviceInfo::AddrMsg(unsigned char *buf, bool del) {
  struct nlmsghdr *hdr = (struct nlmsghdr *) buf;
  struct ifinfomsg *msg = (struct ifinfomsg *) NLMSG_DATA(hdr);
  LOG(INFO) << "index "  << msg->ifi_index << " flags " << msg->ifi_flags <<
    ": del = " << del;
}

void DeviceInfo::RouteMsg(unsigned char *buf, bool del) {
  struct nlmsghdr *hdr = (struct nlmsghdr *) buf;
  struct ifinfomsg *msg = (struct ifinfomsg *) NLMSG_DATA(hdr);
  LOG(INFO) << "index "  << msg->ifi_index << " flags " << msg->ifi_flags <<
    ": del = " << del;
}

void DeviceInfo::ParseRTNL(InputData *data) {
  unsigned char *buf = data->buf;
  unsigned char *end = buf + data->len;

  while (buf < end) {
    struct nlmsghdr *hdr = (struct nlmsghdr *) buf;
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
      err = (nlmsgerr *) NLMSG_DATA(hdr);
      LOG(ERROR) << "error " << -err->error << " (" << strerror(-err->error) <<
                 ")";
      return;
    case RTM_NEWLINK:
      LinkMsg(buf, false);
      break;
    case RTM_DELLINK:
      LinkMsg(buf, true);
      break;
    case RTM_NEWADDR:
      AddrMsg(buf, false);
      break;
    case RTM_DELADDR:
      AddrMsg(buf, true);
      break;
    case RTM_NEWROUTE:
      RouteMsg(buf, false);
      break;
    case RTM_DELROUTE:
      RouteMsg(buf, true);
      break;
    }

    buf += hdr->nlmsg_len;
    }
}

}  // namespace shill
