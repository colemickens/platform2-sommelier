// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/device_info.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <linux/if_tun.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <netinet/ether.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#include <string>

#include <base/callback_old.h>
#include <base/file_util.h>
#include <base/logging.h>
#include <base/memory/scoped_ptr.h>
#include <base/stl_util-inl.h>
#include <base/string_number_conversions.h>
#include <base/string_util.h>
#include <base/stringprintf.h>

#include "shill/control_interface.h"
#include "shill/device.h"
#include "shill/device_stub.h"
#include "shill/ethernet.h"
#include "shill/manager.h"
#include "shill/rtnl_handler.h"
#include "shill/rtnl_listener.h"
#include "shill/rtnl_message.h"
#include "shill/service.h"
#include "shill/wifi.h"

using std::map;
using std::string;
using std::vector;

namespace shill {

// static
const char DeviceInfo::kInterfaceUevent[] = "/sys/class/net/%s/uevent";
// static
const char DeviceInfo::kInterfaceUeventWifiSignature[] = "DEVTYPE=wlan\n";
// static
const char DeviceInfo::kInterfaceDriver[] = "/sys/class/net/%s/device/driver";
// static
const char DeviceInfo::kInterfaceTunFlags[] = "/sys/class/net/%s/tun_flags";
// static
const char DeviceInfo::kInterfaceType[] = "/sys/class/net/%s/type";
// static
const char *DeviceInfo::kModemDrivers[] = {
    "gobi",
    "QCUSBNet2k",
    "GobiNet",
    "cdc_ether",
    NULL
};
// static
const char DeviceInfo::kTunDeviceName[] = "/dev/net/tun";

DeviceInfo::DeviceInfo(ControlInterface *control_interface,
                       EventDispatcher *dispatcher,
                       Metrics *metrics,
                       Manager *manager)
    : control_interface_(control_interface),
      dispatcher_(dispatcher),
      metrics_(metrics),
      manager_(manager),
      link_callback_(NewCallback(this, &DeviceInfo::LinkMsgHandler)),
      address_callback_(NewCallback(this, &DeviceInfo::AddressMsgHandler)),
      link_listener_(NULL),
      address_listener_(NULL),
      rtnl_handler_(RTNLHandler::GetInstance()) {
}

DeviceInfo::~DeviceInfo() {}

void DeviceInfo::AddDeviceToBlackList(const string &device_name) {
  black_list_.insert(device_name);
}

void DeviceInfo::Start() {
  link_listener_.reset(
      new RTNLListener(RTNLHandler::kRequestLink, link_callback_.get()));
  address_listener_.reset(
      new RTNLListener(RTNLHandler::kRequestAddr, address_callback_.get()));
  rtnl_handler_->RequestDump(RTNLHandler::kRequestLink |
                             RTNLHandler::kRequestAddr);
}

void DeviceInfo::Stop() {
  link_listener_.reset();
  address_listener_.reset();
}

void DeviceInfo::RegisterDevice(const DeviceRefPtr &device) {
  VLOG(2) << __func__ << "(" << device->link_name() << ", "
          << device->interface_index() << ")";
  CHECK(!GetDevice(device->interface_index()).get());
  infos_[device->interface_index()].device = device;
  if (device->TechnologyIs(Technology::kCellular) ||
      device->TechnologyIs(Technology::kEthernet) ||
      device->TechnologyIs(Technology::kWifi)) {
    manager_->RegisterDevice(device);
  }
}

void DeviceInfo::DeregisterDevice(const DeviceRefPtr &device) {
  int interface_index = device->interface_index();

  VLOG(2) << __func__ << "(" << device->link_name() << ", "
          << interface_index << ")";
  CHECK(device->TechnologyIs(Technology::kCellular));

  // Release reference to the device
  map<int, Info>::iterator iter = infos_.find(interface_index);
  if (iter != infos_.end()) {
    VLOG(2) << "Removing device from info for index: " << interface_index;
    manager_->DeregisterDevice(device);
    // Release the reference to the device, but maintain the mapping
    // for the index.  That will be cleaned up by an RTNL message.
    iter->second.device = NULL;
  }
}

Technology::Identifier DeviceInfo::GetDeviceTechnology(
    const string &iface_name) {
  FilePath uevent_file(StringPrintf(kInterfaceUevent, iface_name.c_str()));
  string contents;
  if (!file_util::ReadFileToString(uevent_file, &contents)) {
    VLOG(2) << StringPrintf("%s: device %s has no uevent file",
                            __func__, iface_name.c_str());
    return Technology::kUnknown;
  }

  /*
   * If the "uevent" file contains the string "DEVTYPE=wlan\n" at the
   * start of the file or after a newline, we can safely assume this
   * is a wifi device.
   */
  if (contents.find(kInterfaceUeventWifiSignature) != string::npos) {
    VLOG(2) << StringPrintf("%s: device %s has wifi signature in uevent file",
                            __func__, iface_name.c_str());
    FilePath type_file(StringPrintf(kInterfaceType, iface_name.c_str()));
    string type_string;
    int type_val = 0;
    if (file_util::ReadFileToString(type_file, &type_string) &&
        TrimString(type_string, "\n", &type_string) &&
        base::StringToInt(type_string, &type_val) &&
        type_val == ARPHRD_IEEE80211_RADIOTAP) {
      VLOG(2) << StringPrintf("%s: wifi device %s is in monitor mode",
                              __func__, iface_name.c_str());
      return Technology::kWiFiMonitor;
    }
    return Technology::kWifi;
  }

  FilePath driver_file(StringPrintf(kInterfaceDriver, iface_name.c_str()));
  FilePath driver_path;
  if (!file_util::ReadSymbolicLink(driver_file, &driver_path)) {
    VLOG(2) << StringPrintf("%s: device %s has no device symlink",
                            __func__, iface_name.c_str());
    FilePath tun_flags_file(StringPrintf(kInterfaceTunFlags,
                                         iface_name.c_str()));
    string tun_flags_string;
    int tun_flags = 0;
    if (file_util::ReadFileToString(tun_flags_file, &tun_flags_string) &&
        TrimString(tun_flags_string, "\n", &tun_flags_string) &&
        base::HexStringToInt(tun_flags_string, &tun_flags) &&
        (tun_flags & IFF_TUN)) {
      VLOG(2) << StringPrintf("%s: device %s is tun device",
                              __func__, iface_name.c_str());
      return Technology::kTunnel;
    }
     return Technology::kUnknown;
  }

  string driver_name(driver_path.BaseName().value());
  // See if driver for this interface is in a list of known modem driver names
  for (int modem_idx = 0; kModemDrivers[modem_idx] != NULL; ++modem_idx) {
    // TODO(ers) should have additional checks to make sure a cdc_ether
    // device is really a modem. flimflam uses udev to make such checks,
    // looking to see whether a ttyACM or ttyUSB device is associated.
    if (driver_name == kModemDrivers[modem_idx]) {
      VLOG(2) << StringPrintf("%s: device %s is matched with modem driver %s",
                              __func__, iface_name.c_str(),
                              driver_name.c_str());
      return Technology::kCellular;
    }
  }

  VLOG(2) << StringPrintf("%s: device %s is defaulted to type ethernet",
                          __func__, iface_name.c_str());
  return Technology::kEthernet;
}

void DeviceInfo::AddLinkMsgHandler(const RTNLMessage &msg) {
  DCHECK(msg.type() == RTNLMessage::kTypeLink &&
         msg.mode() == RTNLMessage::kModeAdd);
  int dev_index = msg.interface_index();
  Technology::Identifier technology = Technology::kUnknown;

  unsigned int flags = msg.link_status().flags;
  unsigned int change = msg.link_status().change;
  VLOG(2) << __func__ << "(index=" << dev_index
          << std::showbase << std::hex
          << ", flags=" << flags << ", change=" << change << ")"
          << std::dec << std::noshowbase;
  infos_[dev_index].flags = flags;

  DeviceRefPtr device = GetDevice(dev_index);
  if (!device.get()) {
    if (!msg.HasAttribute(IFLA_IFNAME)) {
      LOG(ERROR) << "Add Link message does not have IFLA_IFNAME!";
      return;
    }
    ByteString b(msg.GetAttribute(IFLA_IFNAME));
    string link_name(reinterpret_cast<const char*>(b.GetConstData()));
    VLOG(2) << "add link index "  << dev_index << " name " << link_name;

    if (!link_name.empty()) {
      if (ContainsKey(black_list_, link_name)) {
        technology = Technology::kBlacklisted;
      } else {
        technology = GetDeviceTechnology(link_name);
      }
    }
    string address;
    if (msg.HasAttribute(IFLA_ADDRESS)) {
      infos_[dev_index].mac_address = msg.GetAttribute(IFLA_ADDRESS);
      address = StringToLowerASCII(infos_[dev_index].mac_address.HexEncode());
      VLOG(2) << "link index " << dev_index << " address "
              << infos_[dev_index].mac_address.HexEncode();
    } else if (technology != Technology::kTunnel) {
      LOG(ERROR) << "Add Link message does not have IFLA_ADDRESS!";
      return;
    }
    switch (technology) {
      case Technology::kCellular:
        // Cellular devices are managed by ModemInfo.
        VLOG(2) << "Cellular link " << link_name << " at index " << dev_index
                << " -- notifying ModemInfo.";
        manager_->modem_info()->OnDeviceInfoAvailable(link_name);
        return;
      case Technology::kEthernet:
        device = new Ethernet(control_interface_, dispatcher_, metrics_,
                              manager_, link_name, address, dev_index);
        device->EnableIPv6Privacy();
        break;
      case Technology::kWifi:
        device = new WiFi(control_interface_, dispatcher_, metrics_, manager_,
                          link_name, address, dev_index);
        device->EnableIPv6Privacy();
        break;
      case Technology::kTunnel:
        // Tunnel devices are managed by the VPN code.
        VLOG(2) << "Tunnel link " << link_name << " at index " << dev_index
                << " -- notifying VPNProvider.";
        if (manager_->vpn_provider()->OnDeviceInfoAvailable(link_name,
                                                             dev_index)) {
          // VPN does not know anything about this tunnel, it is probably
          // left over from a previous instance and should not exist.
          VLOG(2) << "Tunnel link is unused.  Deleting.";
          DeleteInterface(dev_index);
        }
        return;
      default:
        device = new DeviceStub(control_interface_, dispatcher_, metrics_,
                                manager_, link_name, address, dev_index,
                                technology);
        break;
    }
    RegisterDevice(device);
  }
  device->LinkEvent(flags, change);
}

void DeviceInfo::DelLinkMsgHandler(const RTNLMessage &msg) {
  VLOG(2) << __func__ << "(index=" << msg.interface_index() << ")";

  DCHECK(msg.type() == RTNLMessage::kTypeLink &&
         msg.mode() == RTNLMessage::kModeDelete);
  VLOG(2) << __func__ << "(index=" << msg.interface_index()
          << std::showbase << std::hex
          << ", flags=" << msg.link_status().flags
          << ", change=" << msg.link_status().change << ")";
  RemoveInfo(msg.interface_index());
}

DeviceRefPtr DeviceInfo::GetDevice(int interface_index) const {
  const Info *info = GetInfo(interface_index);
  return info ? info->device : NULL;
}

bool DeviceInfo::GetMACAddress(int interface_index, ByteString *address) const {
  const Info *info = GetInfo(interface_index);
  if (!info) {
    return false;
  }
  *address = info->mac_address;
  return true;
}

bool DeviceInfo::GetAddresses(int interface_index,
                              vector<AddressData> *addresses) const {
  const Info *info = GetInfo(interface_index);
  if (!info) {
    return false;
  }
  *addresses = info->ip_addresses;
  return true;
}

void DeviceInfo::FlushAddresses(int interface_index) const {
  VLOG(2) << __func__ << "(" << interface_index << ")";
  const Info *info = GetInfo(interface_index);
  if (!info) {
    return;
  }
  const vector<AddressData> &addresses = info->ip_addresses;
  vector<AddressData>::const_iterator iter;
  for (iter = addresses.begin(); iter != addresses.end(); ++iter) {
    if (iter->address.family() == IPAddress::kFamilyIPv4 ||
        (iter->scope == RT_SCOPE_UNIVERSE &&
         (iter->flags & ~IFA_F_TEMPORARY) == 0)) {
      VLOG(2) << __func__ << ": removing ip address from interface "
              << interface_index;
      rtnl_handler_->RemoveInterfaceAddress(interface_index, iter->address);
    }
  }
}

bool DeviceInfo::GetFlags(int interface_index, unsigned int *flags) const {
  const Info *info = GetInfo(interface_index);
  if (!info) {
    return false;
  }
  *flags = info->flags;
  return true;
}

bool DeviceInfo::CreateTunnelInterface(string *interface_name) const {
  int fd = HANDLE_EINTR(open(kTunDeviceName, O_RDWR));
  if (fd < 0) {
    PLOG(ERROR) << "failed to open " << kTunDeviceName;
    return false;
  }
  file_util::ScopedFD scoped_fd(&fd);

  struct ifreq ifr;
  memset(&ifr, 0, sizeof(ifr));
  ifr.ifr_flags = IFF_TUN | IFF_NO_PI;
  if (HANDLE_EINTR(ioctl(fd, TUNSETIFF, &ifr))) {
    PLOG(ERROR) << "failed to create tunnel interface";
    return false;
  }

  if (HANDLE_EINTR(ioctl(fd, TUNSETPERSIST, 1))) {
    PLOG(ERROR) << "failed to set tunnel interface to be persistent";
    return false;
  }

  *interface_name = string(ifr.ifr_name);

  return true;
}

bool DeviceInfo::DeleteInterface(int interface_index) const {
  return rtnl_handler_->RemoveInterface(interface_index);
}

const DeviceInfo::Info *DeviceInfo::GetInfo(int interface_index) const {
  map<int, Info>::const_iterator iter = infos_.find(interface_index);
  if (iter == infos_.end()) {
    return NULL;
  }
  return &iter->second;
}

void DeviceInfo::RemoveInfo(int interface_index) {
  map<int, Info>::iterator iter = infos_.find(interface_index);
  if (iter != infos_.end()) {
    VLOG(2) << "Removing info for device index: " << interface_index;
    if (iter->second.device.get()) {
      manager_->DeregisterDevice(iter->second.device);
    }
    infos_.erase(iter);
  } else {
    VLOG(2) << __func__ << "unknown device index: " << interface_index;
  }
}

void DeviceInfo::LinkMsgHandler(const RTNLMessage &msg) {
  DCHECK(msg.type() == RTNLMessage::kTypeLink);
  if (msg.mode() == RTNLMessage::kModeAdd) {
    AddLinkMsgHandler(msg);
  } else if (msg.mode() == RTNLMessage::kModeDelete) {
    DelLinkMsgHandler(msg);
  } else {
    NOTREACHED();
  }
}

void DeviceInfo::AddressMsgHandler(const RTNLMessage &msg) {
  VLOG(2) << __func__;
  DCHECK(msg.type() == RTNLMessage::kTypeAddress);
  int interface_index = msg.interface_index();
  if (!ContainsKey(infos_, interface_index)) {
    LOG(ERROR) << "Got address type message for unknown index "
               << interface_index;
    return;
  }
  const RTNLMessage::AddressStatus &status = msg.address_status();
  IPAddress address(msg.family(),
                    msg.GetAttribute(IFA_ADDRESS),
                    status.prefix_len);

  vector<AddressData> &address_list = infos_[interface_index].ip_addresses;
  vector<AddressData>::iterator iter;
  for (iter = address_list.begin(); iter != address_list.end(); ++iter) {
    if (address.Equals(iter->address)) {
      break;
    }
  }
  if (iter != address_list.end()) {
    if (msg.mode() == RTNLMessage::kModeDelete) {
      VLOG(2) << "Delete address for interface " << interface_index;
      address_list.erase(iter);
    } else {
      iter->flags = status.flags;
      iter->scope = status.scope;
    }
  } else if (msg.mode() == RTNLMessage::kModeAdd) {
    address_list.push_back(AddressData(address, status.flags, status.scope));
    VLOG(2) << "Add address for interface " << interface_index;
  }
}

}  // namespace shill
