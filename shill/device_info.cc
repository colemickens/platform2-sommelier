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
#include <base/stl_util-inl.h>
#include <base/stringprintf.h>

#include "shill/control_interface.h"
#include "shill/device.h"
#include "shill/device_info.h"
#include "shill/device_stub.h"
#include "shill/ethernet.h"
#include "shill/manager.h"
#include "shill/rtnl_handler.h"
#include "shill/rtnl_listener.h"
#include "shill/service.h"
#include "shill/wifi.h"

using base::hash_map;
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
    : control_interface_(control_interface),
      dispatcher_(dispatcher),
      manager_(manager),
      link_callback_(NewCallback(this, &DeviceInfo::LinkMsgHandler)),
      link_listener_(NULL) {
}

DeviceInfo::~DeviceInfo() {}

void DeviceInfo::AddDeviceToBlackList(const string &device_name) {
  black_list_.insert(device_name);
}

void DeviceInfo::Start() {
  link_listener_.reset(
      new RTNLListener(RTNLHandler::kRequestLink, link_callback_.get()));
  RTNLHandler::GetInstance()->RequestDump(RTNLHandler::kRequestLink);
}

void DeviceInfo::Stop() {
  link_listener_.reset(NULL);
}

void DeviceInfo::RegisterDevice(const DeviceRefPtr &device) {
  VLOG(2) << __func__ << "(" << device->link_name() << ", "
          << device->interface_index() << ")";
  CHECK(!ContainsKey(devices_, device->interface_index()));
  devices_[device->interface_index()] = device;
  if (device->TechnologyIs(Device::kCellular) ||
      device->TechnologyIs(Device::kEthernet) ||
      device->TechnologyIs(Device::kWifi)) {
    manager_->RegisterDevice(device);
  }
}

DeviceRefPtr DeviceInfo::GetDevice(int interface_index) {
  hash_map<int, DeviceRefPtr>::iterator iter = devices_.find(interface_index);
  if (iter == devices_.end()) {
    return NULL;
  }
  return iter->second;
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

void DeviceInfo::AddLinkMsgHandler(struct nlmsghdr *hdr) {
  struct ifinfomsg *msg = reinterpret_cast<struct ifinfomsg *>(NLMSG_DATA(hdr));
  int dev_index = msg->ifi_index;
  struct rtattr *rta;
  int rta_bytes;
  char *link_name = NULL;
  Device::Technology technology = Device::kUnknown;

  VLOG(2) << __func__;

  DeviceRefPtr device = GetDevice(dev_index);
  if (!device.get()) {
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

    VLOG(2) << "add link index "  << dev_index << " name " << link_name;

    if (link_name) {
      if (ContainsKey(black_list_, link_name)) {
        technology = Device::kBlacklisted;
      } else {
        technology = GetDeviceTechnology(link_name);
      }
    }

    switch (technology) {
      case Device::kCellular:
        // Cellular devices are managed by ModemInfo.
        VLOG(2) << "Cellular link " << link_name << " at index " << dev_index
                << " ignored.";
        return;
      case Device::kEthernet:
        device = new Ethernet(control_interface_, dispatcher_, manager_,
                              link_name, dev_index);
        break;
      case Device::kWifi:
        device = new WiFi(control_interface_, dispatcher_, manager_,
                          link_name, dev_index);
        break;
      default:
        device = new DeviceStub(control_interface_, dispatcher_, manager_,
                                link_name, dev_index, technology);
        break;
    }
    RegisterDevice(device);
  }

  device->LinkEvent(msg->ifi_flags, msg->ifi_change);
}

void DeviceInfo::DelLinkMsgHandler(struct nlmsghdr *hdr) {
  struct ifinfomsg *msg = reinterpret_cast<struct ifinfomsg *>(NLMSG_DATA(hdr));
  RemoveDevice(msg->ifi_index);
}

void DeviceInfo::RemoveDevice(int interface_index) {
  hash_map<int, DeviceRefPtr>::iterator ndev = devices_.find(interface_index);
  if (ndev != devices_.end()) {
    VLOG(2) << "Removing device index: " << interface_index;
    manager_->DeregisterDevice(ndev->second);
    devices_.erase(ndev);
  }
}

void DeviceInfo::LinkMsgHandler(struct nlmsghdr *hdr) {
  if (hdr->nlmsg_type == RTM_NEWLINK) {
    AddLinkMsgHandler(hdr);
  } else if (hdr->nlmsg_type == RTM_DELLINK) {
    DelLinkMsgHandler(hdr);
  }
}

}  // namespace shill
