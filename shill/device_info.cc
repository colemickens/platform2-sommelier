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
#include <fcntl.h>
#include <string>

#include <base/callback_old.h>
#include <base/hash_tables.h>
#include <base/logging.h>
#include <base/memory/scoped_ptr.h>
#include <base/stringprintf.h>

#include "shill/control_interface.h"
#include "shill/device.h"
#include "shill/device_info.h"
#include "shill/ethernet.h"
#include "shill/manager.h"
#include "shill/service.h"
#include "shill/wifi.h"

using std::string;

namespace shill {

// static
const char *DeviceInfo::kInterfaceUevent= "/sys/class/net/%s/uevent";
// static
const char *DeviceInfo::kInterfaceDriver= "/sys/class/net/%s/device/driver";
// static
const char *DeviceInfo::kModemDrivers[] = {
    "gobi",
    "QCUSBNet2k",
    "GobiNet",
    NULL
};


DeviceInfo::DeviceInfo(ControlInterface *control_interface,
                       EventDispatcher *dispatcher,
                       Manager *manager)
  : running_(false),
    control_interface_(control_interface),
    dispatcher_(dispatcher),
    manager_(manager),
    rtnl_callback_(NewCallback(this, &DeviceInfo::ParseRTNL)),
    rtnl_socket_(-1),
    request_flags_(0),
    request_sequence_(0) {}

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

  if (bind(rtnl_socket_, reinterpret_cast<struct sockaddr *>(&addr),
           sizeof(addr)) < 0) {
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

  VLOG(2) << "DeviceInfo started";
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
             reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)) < 0) {
    LOG(ERROR) << "RTNL sendto failed";
    return;
  }
  request_flags_ &= ~flag;
}

Device::Technology DeviceInfo::GetDeviceTechnology(const char *interface_name) {
  char contents[1024];
  int length;
  int fd;
  string uevent_file = StringPrintf(kInterfaceUevent, interface_name);
  string driver_file = StringPrintf(kInterfaceDriver, interface_name);
  const char *wifi_type;
  const char *driver_name;
  int modem_idx;

  fd = open(uevent_file.c_str(), O_RDONLY);
  if (fd < 0)
    return Device::kUnknown;

  length = read(fd, contents, sizeof(contents) - 1);
  if (length < 0)
    return Device::kUnknown;

  /*
   * If the "uevent" file contains the string "DEVTYPE=wlan\n" at the
   * start of the file or after a newline, we can safely assume this
   * is a wifi device.
   */
  contents[length] = '\0';
  wifi_type = strstr(contents, "DEVTYPE=wlan\n");
  if (wifi_type != NULL && (wifi_type == contents || wifi_type[-1] == '\n'))
    return Device::kWifi;

  length = readlink(driver_file.c_str(), contents, sizeof(contents)-1);
  if (length < 0)
    return Device::kUnknown;

  contents[length] = '\0';
  driver_name = strrchr(contents, '/');
  if (driver_name != NULL) {
    driver_name++;
    // See if driver for this interface is in a list of known modem driver names
    for (modem_idx = 0; kModemDrivers[modem_idx] != NULL; modem_idx++)
      if (strcmp(driver_name, kModemDrivers[modem_idx]) == 0)
        return Device::kCellular;
  }

  return Device::kEthernet;
}

void DeviceInfo::AddLinkMsg(struct nlmsghdr *hdr) {
  struct ifinfomsg *msg = reinterpret_cast<struct ifinfomsg *>(NLMSG_DATA(hdr));
  base::hash_map<int, scoped_refptr<Device> >::iterator ndev =
      devices_.find(msg->ifi_index);
  int bytes = IFLA_PAYLOAD(hdr);
  int dev_index = msg->ifi_index;
  struct rtattr *rta;
  int rta_bytes;
  char *link_name = NULL;
  Device *device;
  Device::Technology technology;
  bool is_stub = false;

  VLOG(2) << "add link index "  << dev_index << " flags " <<
    msg->ifi_flags;

  if (ndev == devices_.end()) {
    rta_bytes = IFLA_PAYLOAD(hdr);
    for (rta = IFLA_RTA(msg); RTA_OK(rta, rta_bytes);
         rta = RTA_NEXT(rta, rta_bytes)) {
      if (rta->rta_type == IFLA_IFNAME) {
        link_name = reinterpret_cast<char *>(RTA_DATA(rta));
        break;
      } else {
        VLOG(2) << " RTA type " << rta->rta_type;
      }
    }

    if (link_name != NULL)
      technology = GetDeviceTechnology(link_name);

    switch (technology) {
    case Device::kEthernet:
      device = new Ethernet(control_interface_, dispatcher_,
                            link_name, dev_index);
      break;
    case Device::kWifi:
      device = new WiFi(control_interface_, dispatcher_, link_name, dev_index);
      break;
    default:
      device = new StubDevice(control_interface_, dispatcher_,
                              link_name, dev_index, technology);
      is_stub = true;
    }

    devices_[dev_index] = device;

    if (!is_stub)
      manager_->RegisterDevice(device);
  } else {
    device = ndev->second;
  }

  // TODO(pstew): Send the the flags change upwards to the device
}

void DeviceInfo::DelLinkMsg(struct nlmsghdr *hdr) {
  struct ifinfomsg *msg = reinterpret_cast<struct ifinfomsg *>(NLMSG_DATA(hdr));
  base::hash_map<int, scoped_refptr<Device> >::iterator ndev =
      devices_.find(msg->ifi_index);
  int dev_index = msg->ifi_index;
  Device *device;

  VLOG(2) << "del link index "  << dev_index << " flags " <<
    msg->ifi_flags;

  if (ndev != devices_.end()) {
    device = ndev->second;
    devices_.erase(ndev);
    manager_->DeregisterDevice(device);
  }
}

void DeviceInfo::AddAddrMsg(struct nlmsghdr *hdr) {
  struct ifaddrmsg *msg = reinterpret_cast<struct ifaddrmsg *>(NLMSG_DATA(hdr));
  VLOG(2) << "add addrmsg family "  << (int) msg->ifa_family << " index " <<
    msg->ifa_index;
}

void DeviceInfo::DelAddrMsg(struct nlmsghdr *hdr) {
  struct ifaddrmsg *msg = reinterpret_cast<struct ifaddrmsg *>(NLMSG_DATA(hdr));
  VLOG(2) << "del addrmsg family "  << (int) msg->ifa_family << " index " <<
    msg->ifa_index;
}

void DeviceInfo::AddRouteMsg(struct nlmsghdr *hdr) {
  struct rtmsg *msg = reinterpret_cast<struct rtmsg *>(NLMSG_DATA(hdr));
  VLOG(2) << "add routemsg family "  << (int) msg->rtm_family << " table " <<
    (int) msg->rtm_table << " proto " << (int) msg->rtm_protocol;
}

void DeviceInfo::DelRouteMsg(struct nlmsghdr *hdr) {
  struct rtmsg *msg = reinterpret_cast<struct rtmsg *>(NLMSG_DATA(hdr));
  VLOG(2) << "del routemsg family "  << (int) msg->rtm_family << " table " <<
    (int) msg->rtm_table << " proto " << (int) msg->rtm_protocol;
}

void DeviceInfo::ParseRTNL(InputData *data) {
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
      AddLinkMsg(hdr);
      break;
    case RTM_DELLINK:
      DelLinkMsg(hdr);
      break;
    case RTM_NEWADDR:
      AddAddrMsg(hdr);
      break;
    case RTM_DELADDR:
      DelAddrMsg(hdr);
      break;
    case RTM_NEWROUTE:
      AddRouteMsg(hdr);
      break;
    case RTM_DELROUTE:
      DelRouteMsg(hdr);
      break;
    }

    buf += hdr->nlmsg_len;
  }
}

}  // namespace shill
