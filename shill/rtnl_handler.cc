// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/ether.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <fcntl.h>

#include <base/logging.h>

#include "shill/io_handler.h"
#include "shill/ip_address.h"
#include "shill/ipconfig.h"
#include "shill/rtnl_handler.h"
#include "shill/rtnl_listener.h"
#include "shill/rtnl_message.h"
#include "shill/shill_event.h"
#include "shill/sockets.h"

using std::string;

namespace shill {

RTNLHandler::RTNLHandler()
    : sockets_(NULL),
      in_request_(false),
      rtnl_socket_(-1),
      request_flags_(0),
      request_sequence_(0),
      rtnl_callback_(NewCallback(this, &RTNLHandler::ParseRTNL)) {
  VLOG(2) << "RTNLHandler created";
}

RTNLHandler::~RTNLHandler() {
  VLOG(2) << "RTNLHandler removed";
  Stop();
}

RTNLHandler* RTNLHandler::GetInstance() {
  return Singleton<RTNLHandler>::get();
}

void RTNLHandler::Start(EventDispatcher *dispatcher, Sockets *sockets) {
  struct sockaddr_nl addr;

  if (sockets_) {
    return;
  }

  rtnl_socket_ = sockets->Socket(PF_NETLINK, SOCK_DGRAM, NETLINK_ROUTE);
  if (rtnl_socket_ < 0) {
    LOG(ERROR) << "Failed to open rtnl socket";
    return;
  }

  memset(&addr, 0, sizeof(addr));
  addr.nl_family = AF_NETLINK;
  addr.nl_groups = RTMGRP_LINK | RTMGRP_IPV4_IFADDR | RTMGRP_IPV4_ROUTE;

  if (sockets->Bind(rtnl_socket_,
                    reinterpret_cast<struct sockaddr *>(&addr),
                    sizeof(addr)) < 0) {
    sockets->Close(rtnl_socket_);
    rtnl_socket_ = -1;
    LOG(ERROR) << "RTNL socket bind failed";
    return;
  }

  rtnl_handler_.reset(dispatcher->CreateInputHandler(rtnl_socket_,
                                                     rtnl_callback_.get()));
  sockets_ = sockets;

  NextRequest(request_sequence_);
  VLOG(2) << "RTNLHandler started";
}

void RTNLHandler::Stop() {
  if (!sockets_)
    return;

  rtnl_handler_.reset(NULL);
  sockets_->Close(rtnl_socket_);
  in_request_ = false;
  sockets_ = NULL;
  request_flags_ = 0;
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

  if (sockets_->Send(rtnl_socket_, &req, sizeof(req), 0) < 0) {
    LOG(ERROR) << "RTNL sendto failed: " << strerror(errno);
  }
}

void RTNLHandler::RequestDump(int request_flags) {
  request_flags_ |= request_flags;

  VLOG(2) << "RTNLHandler got request to dump " << request_flags;

  if (!in_request_ && sockets_)
    NextRequest(request_sequence_);
}

void RTNLHandler::DispatchEvent(int type, const RTNLMessage &msg) {
  std::vector<RTNLListener *>::iterator it;
  for (it = listeners_.begin(); it != listeners_.end(); ++it) {
    (*it)->NotifyEvent(type, msg);
  }
}

void RTNLHandler::NextRequest(uint32_t seq) {
  struct rtnl_request {
    struct nlmsghdr hdr;
    struct rtgenmsg msg;
  } req;
  struct sockaddr_nl addr;
  int flag = 0;

  VLOG(2) << "RTNLHandler nextrequest " << seq << " " << request_sequence_
          << " " << request_flags_;

  if (seq != request_sequence_)
    return;

  request_sequence_++;
  memset(&req, 0, sizeof(req));

  req.hdr.nlmsg_len = sizeof(req);
  req.hdr.nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
  req.hdr.nlmsg_pid = 0;
  req.hdr.nlmsg_seq = request_sequence_;

  if ((request_flags_ & kRequestLink) != 0) {
    req.msg.rtgen_family = AF_INET;
    req.hdr.nlmsg_type = RTM_GETLINK;
    flag = kRequestLink;
  } else if ((request_flags_ & kRequestAddr) != 0) {
    req.msg.rtgen_family = AF_INET;
    req.hdr.nlmsg_type = RTM_GETADDR;
    flag = kRequestAddr;
  } else if ((request_flags_ & kRequestRoute) != 0) {
    req.msg.rtgen_family = AF_INET;
    req.hdr.nlmsg_type = RTM_GETROUTE;
    flag = kRequestRoute;
  } else if ((request_flags_ & kRequestAddr6) != 0) {
    req.msg.rtgen_family = AF_INET6;
    req.hdr.nlmsg_type = RTM_GETADDR;
    flag = kRequestAddr6;
  } else if ((request_flags_ & kRequestRoute6) != 0) {
    req.msg.rtgen_family = AF_INET6;
    req.hdr.nlmsg_type = RTM_GETROUTE;
    flag = kRequestRoute6;
  } else {
    in_request_ = false;
    return;
  }

  memset(&addr, 0, sizeof(addr));
  addr.nl_family = AF_NETLINK;

  if (sockets_->SendTo(rtnl_socket_,
                       &req,
                       sizeof(req),
                       0,
                       reinterpret_cast<struct sockaddr *>(&addr),
                       sizeof(addr)) < 0) {
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
    if (!NLMSG_OK(hdr, static_cast<unsigned int>(end - buf)))
      break;

    RTNLMessage msg;
    if (!msg.Decode(ByteString(reinterpret_cast<unsigned char *>(hdr),
                               hdr->nlmsg_len))) {

      switch (hdr->nlmsg_type) {
        case NLMSG_NOOP:
        case NLMSG_OVERRUN:
          break;
        case NLMSG_DONE:
          NextRequest(hdr->nlmsg_seq);
          break;
        case NLMSG_ERROR:
          {
            struct nlmsgerr *err =
                reinterpret_cast<nlmsgerr *>(NLMSG_DATA(hdr));
            LOG(ERROR) << "error " << -err->error << " ("
                       << strerror(-err->error) << ")";
            break;
          }
        default:
          NOTIMPLEMENTED() << "Unknown NL message type.";
      }
    } else {
      switch (msg.type()) {
        case RTNLMessage::kMessageTypeLink:
          DispatchEvent(kRequestLink, msg);
          break;
        case RTNLMessage::kMessageTypeAddress:
          DispatchEvent(kRequestAddr, msg);
          break;
        case RTNLMessage::kMessageTypeRoute:
          DispatchEvent(kRequestRoute, msg);
          break;
        default:
          NOTIMPLEMENTED() << "Unknown RTNL message type.";
      }
    }
    buf += hdr->nlmsg_len;
  }
}

static bool AddAtribute(struct nlmsghdr *hdr, int max_msg_size, int attr_type,
                        const void *attr_data, int attr_len) {
  int len = RTA_LENGTH(attr_len);
  int new_msg_size = NLMSG_ALIGN(hdr->nlmsg_len) + RTA_ALIGN(len);
  struct rtattr *rt_attr;

  if (new_msg_size > max_msg_size)
    return false;

  rt_attr = reinterpret_cast<struct rtattr *> (reinterpret_cast<unsigned char *>
                                               (hdr) +
                                               NLMSG_ALIGN(hdr->nlmsg_len));
  rt_attr->rta_type = attr_type;
  rt_attr->rta_len = len;
  memcpy(RTA_DATA(rt_attr), attr_data, attr_len);
  hdr->nlmsg_len = new_msg_size;
  return true;
}

bool RTNLHandler::AddressRequest(int interface_index, int cmd, int flags,
                                 const IPConfig &ipconfig) {
  const IPConfig::Properties &properties = ipconfig.properties();
  int address_family;
  int address_size;
  unsigned char *attrs, *attrs_end;
  int max_msg_size;
  struct {
    struct nlmsghdr   hdr;
    struct ifaddrmsg  ifa;
    unsigned char     attrs[256];
  } req;
  union {
    in_addr ip4;
    in6_addr in6;
  } addr;

  if (properties.address_family == IPAddress::kAddressFamilyIPv4) {
    address_family = AF_INET;
    address_size = sizeof(struct in_addr);
  } else if (properties.address_family == IPAddress::kAddressFamilyIPv6) {
    address_family = AF_INET6;
    address_size = sizeof(struct in6_addr);
  } else {
    return false;
  }

  request_sequence_++;
  memset(&req, 0, sizeof(req));
  req.hdr.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifaddrmsg));
  req.hdr.nlmsg_flags = NLM_F_REQUEST | flags;
  req.hdr.nlmsg_type = cmd;
  req.ifa.ifa_family = address_family;
  req.ifa.ifa_index = interface_index;

  max_msg_size = req.hdr.nlmsg_len + sizeof(req.attrs);

  // TODO(pstew): This code only works for Ethernet-like setups,
  //              not with devices that have a peer address like PPP.
  if (inet_pton(address_family, properties.address.c_str(), &addr) <= 0 ||
      !AddAtribute(&req.hdr, max_msg_size, IFA_LOCAL, &addr, address_size))
    return false;

  if (inet_pton(address_family, properties.broadcast_address.c_str(),
                &addr) <= 0 ||
      !AddAtribute(&req.hdr, max_msg_size, IFA_BROADCAST, &addr, address_size))
    return false;

  req.ifa.ifa_prefixlen = properties.subnet_cidr;

  if (sockets_->Send(rtnl_socket_, &req, req.hdr.nlmsg_len, 0) < 0) {
    LOG(ERROR) << "RTNL sendto failed: " << strerror(errno);
    return false;
  }

  return true;
}

bool RTNLHandler::AddInterfaceAddress(int interface_index,
                                      const IPConfig &ipconfig) {
  return AddressRequest(interface_index, RTM_NEWADDR, NLM_F_CREATE | NLM_F_EXCL,
                        ipconfig);
}

bool RTNLHandler::RemoveInterfaceAddress(int interface_index,
                                         const IPConfig &ipconfig) {
  return AddressRequest(interface_index, RTM_DELADDR, 0, ipconfig);
}

int RTNLHandler::GetInterfaceIndex(const string &interface_name) {
  if (interface_name.empty()) {
    LOG(ERROR) << "Empty interface name -- unable to obtain index.";
    return -1;
  }
  struct ifreq ifr;
  if (interface_name.size() >= sizeof(ifr.ifr_name)) {
    LOG(ERROR) << "Interface name too long: " << interface_name.size() << " >= "
               << sizeof(ifr.ifr_name);
    return -1;
  }
  int socket = sockets_->Socket(PF_INET, SOCK_DGRAM, 0);
  if (socket < 0) {
    PLOG(ERROR) << "Unable to open INET socket";
    return -1;
  }
  ScopedSocketCloser socket_closer(sockets_, socket);
  memset(&ifr, 0, sizeof(ifr));
  strncpy(ifr.ifr_name, interface_name.c_str(), sizeof(ifr.ifr_name));
  if (sockets_->Ioctl(socket, SIOCGIFINDEX, &ifr) < 0) {
    PLOG(ERROR) << "SIOCGIFINDEX error for " << interface_name;
    return -1;
  }
  return ifr.ifr_ifindex;
}

bool RTNLHandler::SendMessage(RTNLMessage *message) {
  message->set_seq(request_sequence_++);
  ByteString msgdata = message->Encode();

  if (msgdata.GetLength() == 0) {
    return false;
  }

  if (sockets_->Send(rtnl_socket_,
                     msgdata.GetData(),
                     msgdata.GetLength(),
                     0) < 0) {
    PLOG(ERROR) << "RTNL send failed: " << strerror(errno);
    return false;
  }

  return true;
}

}  // namespace shill
